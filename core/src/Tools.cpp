#include "hhmg/Tools.h"
#include "hhmg/Analyze.h"
#include "hhmg/Guide.h"
#include <algorithm>
#include <cmath>

namespace hhmg {
namespace {

Json prop(const char* type, const char* desc) {
    Json p = Json::object(); p.set("type", type); p.set("description", desc); return p;
}
Json enumProp(const std::vector<std::string>& values, const char* desc) {
    Json p = prop("string", desc);
    Json e = Json::array(); for (const auto& v : values) e.push(v);
    p.set("enum", e);
    return p;
}
Json scaleIds() {
    Json a = Json::array();
    for (const auto& s : allScales()) a.push(s.id);
    return a;
}
Json notesProp() {
    Json p = Json::object(); p.set("type", "array"); p.set("description", "MIDI note numbers");
    Json it = Json::object(); it.set("type", "integer"); p.set("items", it);
    return p;
}
Json scaleSelectorProps() {
    Json p = Json::object();
    p.set("root", prop("string", "Tonic, e.g. C, F#, Bb (or a pitch class 0-11)"));
    Json s = prop("string", "Scale/mode id");
    s.set("enum", scaleIds());
    p.set("scale", s);
    return p;
}
Json tool(const char* name, const char* desc, Json props, std::vector<std::string> required) {
    Json t = Json::object();
    t.set("name", name); t.set("description", desc);
    Json schema = Json::object();
    schema.set("type", "object"); schema.set("properties", props);
    Json req = Json::array(); for (const auto& r : required) req.push(r);
    schema.set("required", req);
    t.set("inputSchema", schema);
    return t;
}
Json err(const std::string& m) { Json j = Json::object(); j.set("ok", false); j.set("error", m); return j; }

bool resolveScale(const Json& a, Scale& out, std::string& error) {
    const Json& r = a["root"];
    int root = r.isNumber() ? ((r.integer() % 12) + 12) % 12 : parseRoot(r.str(""));
    if (root < 0) { error = "unknown root note"; return false; }
    int idx = findScale(a["scale"].str("minor"));
    if (idx < 0) { error = "unknown scale '" + a["scale"].str("") + "'"; return false; }
    out = Scale{root, idx};
    return true;
}

Json pcNames(const Scale& s, const std::vector<int>& iv) {
    Json a = Json::array();
    for (int i : iv) a.push(noteName(s.root + i));
    return a;
}

} // namespace

Request requestFromJson(const Json& a) {
    Request r;
    const Json& rt = a["root"];
    if (rt.isNumber()) r.root = rt.integer();
    else if (rt.isString()) { int p = parseRoot(rt.str()); if (p >= 0) r.root = p; }
    if (a.has("scale")) { int i = findScale(a["scale"].str("")); if (i >= 0) r.scale = i; }
    r.bpm = a.has("bpm") ? a["bpm"].integer() : r.bpm;
    r.bars = a.has("bars") ? a["bars"].integer() : r.bars;
    r.complexity = a.has("complexity") ? a["complexity"].num() : r.complexity;
    r.humanize = a.has("humanize") ? a["humanize"].num() : r.humanize;
    r.swing = a.has("swing") ? a["swing"].num() : r.swing;
    r.velocityVariation = a.has("velocity_variation") ? a["velocity_variation"].num() : r.velocityVariation;
    r.chords = a.has("chords") ? a["chords"].boolean(true) : r.chords;
    r.melody = a.has("melody") ? a["melody"].boolean(true) : r.melody;
    r.bass = a.has("bass") ? a["bass"].boolean(false) : r.bass;
    r.melodyOctave = a.has("melody_octave") ? a["melody_octave"].integer() : r.melodyOctave;
    r.seed = a.has("seed") ? (uint32_t) a["seed"].num() : r.seed;
    const Json& m = a["style_mix"];
    if (m.isObject()) {
        r.mix.rnb = m["rnb"].num(0); r.mix.trap = m["trap"].num(0); r.mix.hiphop = m["hiphop"].num(0);
    } else if (a["style"].isString()) {
        std::string s = a["style"].str();
        if (s == "rnb") r.mix = StyleMix{1, 0, 0};
        else if (s == "trap") r.mix = StyleMix{0, 1, 0};
        else if (s == "hiphop") r.mix = StyleMix{0, 0, 1};
    }
    for (const auto& d : a["progression"].items()) r.progression.push_back(d.integer());
    return r;
}

Json resultToJson(const Result& r, bool includeNotes) {
    Json o = Json::object();
    o.set("description", r.description);
    o.set("scale", std::string(noteName(r.scale.root)) + " " + r.scale.def().id);
    Json ch = Json::array(); for (const auto& c : r.chordNames) ch.push(c);
    o.set("chords", ch);
    Json dg = Json::array(); for (int d : r.degrees) dg.push(d);
    o.set("degrees", dg);
    o.set("note_count", (int) r.notes.size());
    o.set("out_of_scale_notes", countOutOfScale(r));
    if (includeNotes) {
        Json ns = Json::array();
        static const char* chn[] = {"chords", "melody", "bass"};
        for (const auto& n : r.notes) {
            Json x = Json::object();
            x.set("pitch", n.pitch); x.set("start_ticks", std::round(n.start)); x.set("length_ticks", std::round(n.length));
            x.set("velocity", n.velocity); x.set("track", chn[n.channel]);
            ns.push(x);
        }
        o.set("notes", ns);
        o.set("ppq", kPPQ);
    }
    return o;
}

std::string applyReferences(Request& req, const Json& a, const PatternLibrary& lib, std::string& error) {
    std::string note;
    auto find = [&](const std::string& id) -> const Reference* {
        for (const auto& r : lib.references) if (r.id == id) return &r;
        return nullptr;
    };
    std::string pid = a["reference_progression"].str(""), rid = a["reference_rhythm"].str("");
    if (!pid.empty()) {
        const Reference* r = find(pid);
        if (!r) { error = "unknown reference id '" + pid + "' (use find_references)"; return note; }
        std::vector<int> d = r->degrees;
        for (size_t period : {1u, 2u, 4u, 8u}) {   // shortest repeating loop
            if (d.size() < period * 2) break;
            bool ok = true;
            for (size_t i = period; i < d.size() && ok; ++i) ok = d[i] == d[i % period];
            if (ok) { d.resize(period); break; }
        }
        if (d.size() > 8) d.resize(8);
        if (d.size() >= 2) { req.progression = d; note += "progression from '" + r->name + "'"; }
    }
    if (!rid.empty()) {
        const Reference* r = find(rid);
        if (!r) { error = "unknown reference id '" + rid + "' (use find_references)"; return note; }
        if (!r->rhythm.empty()) { req.hookRhythm = r->rhythm; note += std::string(note.empty() ? "" : ", ") + "rhythm from '" + r->name + "'"; }
    }
    return note;
}

Json toolSchemas() {
    Json list = Json::array();

    list.push(tool("list_scales", "List every supported scale/mode with its semitone intervals.", Json::object(), {}));

    list.push(tool("scale_notes", "Return the note names of a scale (melody set and chord set).",
                   scaleSelectorProps(), {"root", "scale"}));

    Json snapProps = scaleSelectorProps();
    snapProps.set("notes", notesProp());
    list.push(tool("snap_to_scale", "Move every MIDI note to the nearest note of the scale (ties go down). Use before emitting any note.",
                   snapProps, {"root", "scale", "notes"}));
    list.push(tool("check_in_scale", "Report which MIDI notes are outside the scale.", snapProps, {"root", "scale", "notes"}));

    Json gp = Json::object();
    gp.set("query", prop("string", "Free text: genre, mood, reference, in any language (e.g. 'dark trap', 'ترپ تاریک', 'late night rnb')"));
    gp.set("style", enumProp({"rnb", "trap", "hiphop"}, "Optional style"));
    gp.set("mood", prop("string", "Optional mood words"));
    gp.set("all", prop("boolean", "Return every recipe instead of the best 3"));
    list.push(tool("style_guide", "Domain knowledge: best-matching style recipes (scale, BPM, complexity, swing, progression and WHY), what each scale feels like, and what each setting does. Call this first when the request is vague, and use it to explain your choices to the user.", gp, {}));

    Json fr = Json::object();
    fr.set("style", enumProp({"rnb", "trap", "hiphop"}, "Preferred style"));
    fr.set("mode", enumProp({"minor", "major"}, "Harmonic mode of the reference"));
    fr.set("root", prop("string", "Preferred key, e.g. F#"));
    fr.set("bpm", prop("number", "Preferred tempo; closer is ranked higher"));
    fr.set("query", prop("string", "Words matched against pack / file names"));
    fr.set("limit", prop("integer", "Max results (default 8)"));
    list.push(tool("find_references", "Search the learned reference MIDI profiles (real producer loops: key, BPM, chord degrees per bar, melody rhythm, style). Use the returned ids as reference_progression / reference_rhythm in generate_midi to combine real examples.", fr, {}));

    Json am = Json::object();
    am.set("path", prop("string", "Path to a .mid file on this computer"));
    list.push(tool("analyze_midi", "Analyze a MIDI file: key/mode, BPM, chord degrees per bar, harmonic richness, swing, looseness, melodic repetition/space, R&B/Trap/Hip-Hop style scores with reasons, and the generate_midi settings that reproduce its feel.", am, {"path"}));
    list.push(tool("analyze_current_clip", "Analyze the clip currently loaded in the plugin (generated or imported MIDI) and return the same report as analyze_midi. Only available inside the plugin.", Json::object(), {}));

    Json lp = Json::object();
    lp.set("style", enumProp({"rnb", "trap", "hiphop"}, "Optional style filter"));
    list.push(tool("list_patterns", "Summarise the rhythm / motif / progression library (built-in + learned dataset).", lp, {}));

    Json pp = scaleSelectorProps();
    pp.set("style", enumProp({"rnb", "trap", "hiphop"}, "Style filter"));
    list.push(tool("list_progressions", "Chord progressions (1-based degrees) suited to the scale's mode.", pp, {}));

    Json g = scaleSelectorProps();
    g.set("bpm", prop("integer", "40-220"));
    g.set("bars", prop("integer", "1-32"));
    Json sm = Json::object(); sm.set("type", "object");
    sm.set("description", "Blend of styles, each 0-1: {rnb, trap, hiphop}");
    Json smp = Json::object(); smp.set("rnb", prop("number", "")); smp.set("trap", prop("number", "")); smp.set("hiphop", prop("number", ""));
    sm.set("properties", smp);
    g.set("style_mix", sm);
    g.set("complexity", prop("number", "0-1: chord extensions + melodic density"));
    g.set("humanize", prop("number", "0-1"));
    g.set("swing", prop("number", "0-1"));
    g.set("velocity_variation", prop("number", "0-1"));
    g.set("chords", prop("boolean", "Generate the chord track"));
    g.set("melody", prop("boolean", "Generate the melody track"));
    g.set("bass", prop("boolean", "Generate an 808/bass track"));
    g.set("melody_octave", prop("integer", "3-7, centre octave of the melody"));
    Json prog = Json::object(); prog.set("type", "array"); prog.set("description", "Optional explicit chord degrees 1-7 (e.g. [1,6,3,7])");
    Json pi = Json::object(); pi.set("type", "integer"); prog.set("items", pi);
    g.set("progression", prog);
    g.set("seed", prop("integer", "Same seed + settings = same result"));
    g.set("reference_progression", prop("string", "Reference id from find_references: use its chord progression"));
    g.set("reference_rhythm", prop("string", "Reference id from find_references: use its melody rhythm for the hook"));
    g.set("output_path", prop("string", "If set, write a .mid file here"));
    g.set("include_notes", prop("boolean", "Return every note in the response"));
    list.push(tool("generate_midi", "Generate chords/melody/808 that are guaranteed in-scale and optionally write a .mid file.", g, {"root", "scale"}));
    return list;
}

Json callTool(const std::string& name, const Json& a, const PatternLibrary& lib) {
    try {
        Json out = Json::object();
        out.set("ok", true);
        if (name == "list_scales") {
            Json arr = Json::array();
            for (const auto& s : allScales()) {
                Json x = Json::object(); x.set("id", s.id); x.set("label", s.label);
                Json iv = Json::array(); for (int i : s.intervals) iv.push(i);
                x.set("intervals", iv); arr.push(x);
            }
            out.set("scales", arr);
        } else if (name == "scale_notes") {
            Scale s; std::string e;
            if (!resolveScale(a, s, e)) return err(e);
            out.set("melody_notes", pcNames(s, s.def().intervals));
            out.set("chord_notes", pcNames(s, s.def().chordParent));
        } else if (name == "snap_to_scale" || name == "check_in_scale") {
            Scale s; std::string e;
            if (!resolveScale(a, s, e)) return err(e);
            Json snapped = Json::array(), bad = Json::array();
            for (const auto& n : a["notes"].items()) {
                int p = n.integer();
                snapped.push(s.snap(p));
                if (!s.contains(p)) bad.push(p);
            }
            if (name == "snap_to_scale") out.set("notes", snapped);
            else { out.set("all_in_scale", bad.size() == 0); out.set("out_of_scale", bad); }
        } else if (name == "style_guide") {
            Json g = styleGuide(a);
            for (size_t i = 0; i < g.keys().size(); ++i) out.set(g.keys()[i], g.valueAt(i));
        } else if (name == "find_references") {
            std::string st = a["style"].str(""), mode = a["mode"].str(""), q = a["query"].str("");
            for (auto& c : q) c = (char) std::tolower((unsigned char) c);
            int root = a["root"].isString() ? parseRoot(a["root"].str()) : (a["root"].isNumber() ? ((a["root"].integer() % 12) + 12) % 12 : -1);
            double bpm = a["bpm"].num(0);
            int limit = a.has("limit") ? std::max(1, std::min(25, a["limit"].integer())) : 8;
            std::vector<std::pair<double, const Reference*>> ranked;
            for (const auto& r : lib.references) {
                double sc = 0;
                if (!st.empty() && r.style == st) sc += 2;
                if (!mode.empty() && r.mode == mode) sc += 2;
                if (root >= 0 && r.root == root) sc += 1.5;
                if (bpm > 0 && r.bpm > 0) sc += std::max(0.0, 2.0 - std::fabs(r.bpm - bpm) / 15.0);
                if (!q.empty()) { std::string hay = r.name + " " + r.pack; for (auto& c : hay) c = (char) std::tolower((unsigned char) c); if (hay.find(q) != std::string::npos) sc += 2; }
                ranked.push_back({sc, &r});
            }
            std::stable_sort(ranked.begin(), ranked.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
            Json arr = Json::array();
            for (size_t i = 0; i < ranked.size() && (int) i < limit; ++i) {
                const Reference& r = *ranked[i].second;
                Json x = Json::object();
                x.set("id", r.id); x.set("name", r.name); x.set("pack", r.pack); x.set("style", r.style);
                x.set("key", std::string(noteName(r.root)) + " " + r.mode); x.set("bpm", r.bpm); x.set("bars", r.bars);
                Json d = Json::array(); for (int v : r.degrees) d.push(v);
                x.set("chord_degrees_per_bar", d); x.set("melody_notes_per_bar", std::round(r.density * 10) / 10); x.set("swing", std::round(r.swing * 100) / 100);
                arr.push(x);
            }
            out.set("total_references", (int) lib.references.size());
            out.set("references", arr);
        } else if (name == "analyze_midi") {
            std::string path = a["path"].str("");
            std::string low = path; for (auto& c : low) c = (char) std::tolower((unsigned char) c);
            if (low.size() < 4 || (low.substr(low.size() - 4) != ".mid" && (low.size() < 5 || low.substr(low.size() - 5) != ".midi")))
                return err("only .mid / .midi files can be analyzed");
            MidiData md; std::string e;
            if (!readMidiFile(path, md, e)) return err(e);
            Json rep = analysisToJson(analyzeMidi(md, lib));
            for (size_t i = 0; i < rep.keys().size(); ++i) out.set(rep.keys()[i], rep.valueAt(i));
        } else if (name == "analyze_current_clip") {
            return err("analyze_current_clip only works inside the plugin; use analyze_midi with a file path");
        } else if (name == "list_patterns") {
            std::string st = a["style"].str("");
            int r = 0, m = 0, p = 0;
            Json names = Json::array();
            for (const auto& x : lib.rhythms) if (st.empty() || x.style == st) { ++r; names.push(x.name); }
            for (const auto& x : lib.motifs) if (st.empty() || x.style == st) ++m;
            for (const auto& x : lib.progressions) if (st.empty() || x.style == st) ++p;
            out.set("rhythm_count", r); out.set("motif_count", m); out.set("progression_count", p);
            out.set("rhythm_names", names);
        } else if (name == "list_progressions") {
            Scale s{0, 1}; std::string e;
            if (a.has("scale") && !resolveScale(a, s, e)) return err(e);
            std::string st = a["style"].str("");
            const char* mode = s.isMinorish() ? "minor" : "major";
            Json arr = Json::array();
            for (const auto& p : lib.progressions) {
                if (p.mode != mode || (!st.empty() && p.style != st)) continue;
                Json x = Json::object(); x.set("style", p.style);
                Json d = Json::array(); for (int v : p.degrees) d.push(v);
                x.set("degrees", d); arr.push(x);
            }
            out.set("mode", mode); out.set("progressions", arr);
        } else if (name == "generate_midi") {
            Request rq = requestFromJson(a);
            std::string refErr, refNote = applyReferences(rq, a, lib, refErr);
            if (!refErr.empty()) return err(refErr);
            Result r = generate(rq, lib);
            Json body = resultToJson(r, a["include_notes"].boolean(false));
            if (!refNote.empty()) out.set("used_references", refNote);
            for (size_t i = 0; i < body.keys().size(); ++i) out.set(body.keys()[i], body.valueAt(i));
            std::string path = a["output_path"].str("");
            if (!path.empty()) {
                if (!writeMidiFile(r, path)) return err("could not write " + path);
                out.set("midi_file", path);
            }
        } else {
            return err("unknown tool '" + name + "'");
        }
        return out;
    } catch (const std::exception& ex) {
        return err(ex.what());
    }
}

} // namespace hhmg
