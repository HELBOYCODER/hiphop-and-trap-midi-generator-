#include "hhmg/Studio.h"
#include "hhmg/MidiFile.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace hhmg {

Json clipToJson(const std::vector<Note>& notes, int bpm, int bars, const std::string& description) {
    Json c = Json::object();
    c.set("bpm", bpm); c.set("bars", bars); c.set("description", description);
    Json arr = Json::array();
    for (const auto& n : notes) {
        Json x = Json::array();
        x.push(n.pitch); x.push(std::round(n.start)); x.push(std::round(n.length)); x.push(n.velocity); x.push(n.channel);
        arr.push(x);
    }
    c.set("notes", arr);
    return c;
}

bool clipFromJson(const Json& clip, std::vector<Note>& notes, int& bpm, int& bars) {
    if (!clip.isObject()) return false;
    bpm = clip["bpm"].integer(120); bars = clip["bars"].integer(4);
    notes.clear();
    for (const auto& x : clip["notes"].items()) {
        if (x.size() < 5) continue;
        Note n; n.pitch = x.at(0).integer(); n.start = x.at(1).num(); n.length = x.at(2).num(); n.velocity = x.at(3).integer(); n.channel = std::max(0, std::min(2, x.at(4).integer()));
        notes.push_back(n);
    }
    return !notes.empty();
}

std::vector<uint8_t> clipToMidi(const Json& clip) {
    Result r;
    std::vector<Note> notes; int bpm = 120, bars = 4;
    if (!clipFromJson(clip, notes, bpm, bars)) return {};
    r.request.bpm = std::max(40, std::min(220, bpm));
    r.request.bars = std::max(1, std::min(64, bars));
    r.notes = notes;
    r.description = clip["description"].str("HipHop MIDI Studio");
    return writeMidi(r);
}

Json transcriptionToJson(const Transcription& t) {
    Json o = Json::object();
    o.set("ok", t.error.empty());
    if (!t.error.empty()) { o.set("error", t.error); return o; }
    o.set("bpm", t.bpm); o.set("bpm_estimated", t.bpmEstimated); o.set("duration_sec", std::round(t.durationSec * 10) / 10); o.set("start_offset_sec", std::round(t.startOffsetSec * 1000) / 1000);
    Json c = Json::array();
    for (const auto& f : t.contour) {   // [timeSec, midi(-1 = silent)]
        Json p = Json::array(); p.push(std::round(f.time * 1000) / 1000); p.push(f.voiced ? std::round(f.midi * 100) / 100 : -1.0); c.push(p);
    }
    o.set("contour", c);
    Json n = Json::array();
    for (const auto& x : t.notes) { Json a = Json::array(); a.push(x.pitch); a.push(std::round(x.start)); a.push(std::round(x.length)); a.push(x.velocity); a.push(x.channel); n.push(a); }
    o.set("notes", n);
    return o;
}

namespace {
std::vector<int> loopOf(std::vector<int> d) {
    for (size_t period : {1u, 2u, 4u, 8u}) {
        if (d.size() < period * 2) break;
        bool ok = true;
        for (size_t i = period; i < d.size() && ok; ++i) ok = d[i] == d[i % period];
        if (ok) { d.resize(period); break; }
    }
    if (d.size() > 8) d.resize(8);
    return d;
}

// The user's dataset: pick a real reference close to what was sung (same mode and style, near tempo/key).
const Reference* pickReference(const Analysis& a, const PatternLibrary& lib, uint32_t seed) {
    const bool minor = Scale{a.root, a.scale}.isMinorish();
    std::vector<std::pair<double, const Reference*>> ranked;
    for (const auto& r : lib.references) {
        if (r.degrees.size() < 2) continue;
        double sc = 0;
        if ((r.mode == "minor") == minor) sc += 3;
        if (r.style == a.topStyle) sc += 2;
        if (r.root == a.root) sc += 1;
        if (r.bpm > 0 && a.bpmKnown) sc += std::max(0.0, 1.5 - std::fabs(r.bpm - a.bpm) / 20.0);
        if (r.degrees.size() >= 4) sc += 0.5;
        ranked.push_back({sc, &r});
    }
    if (ranked.empty()) return nullptr;
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    size_t top = std::min<size_t>(4, ranked.size());
    return ranked[seed % top].second;     // different seeds walk through the best matches
}
} // namespace

static int barsFor(const std::vector<Note>& notes) {
    double end = 0; for (const auto& n : notes) end = std::max(end, n.start + n.length);
    return std::max(1, std::min(32, (int) std::ceil(end / kTicksPerBar - 0.02)));
}

Json studioProcess(const std::string& mode, const std::vector<Note>& melodyIn, double bpm, bool bpmKnown,
                   const Json& options, const PatternLibrary& lib) {
    Json out = Json::object();
    if (melodyIn.empty()) { out.set("ok", false); out.set("error", "no notes"); return out; }
    std::vector<Note> melody = melodyIn;
    for (auto& n : melody) n.channel = kMelody;

    Analysis a = analyzeNotes({}, melody, bpm, lib, bpmKnown);
    const Scale scale{a.root, a.scale};
    const bool lock = options["scale_lock"].boolean(true);
    if (lock) for (auto& n : melody) n.pitch = scale.snap(n.pitch);   // never a wrong note, even if the singer was off

    Request rq = a.suggested;
    rq.seed = (uint32_t) options["seed"].num(1);
    if (bpmKnown || options["keep_bpm"].boolean(false)) rq.bpm = (int) std::lround(bpm);   // else: a tempo typical of the detected style
    rq.bars = barsFor(melody);
    if (options["complexity"].isNumber()) rq.complexity = options["complexity"].num();
    const std::string style = options["style"].str("");
    if (style == "rnb") rq.mix = {1, 0.1, 0.1}; else if (style == "trap") rq.mix = {0.1, 1, 0.3}; else if (style == "hiphop") rq.mix = {0.1, 0.2, 1};
    rq.bass = options.has("bass") ? options["bass"].boolean(true) : (style != "rnb");

    std::vector<Note> notes;
    std::string desc;
    if (mode == "melody") {
        notes = melody;
        desc = "Your melody";
    } else if (mode == "melody_plus") {
        rq.melody = false; rq.chords = true;
        Result r = generate(rq, lib);
        notes = r.notes;
        notes.insert(notes.end(), melody.begin(), melody.end());
        desc = "Your melody + " + r.description;
        Json ch = Json::array(); for (const auto& c : r.chordNames) ch.push(c);
        out.set("chords", ch);
    } else { // similar: same key, tempo, feel and rhythm, new notes - drawing on the dataset when it has a close reference
        rq.chords = true; rq.melody = true;
        rq.hookRhythm = a.rhythm;
        if (options["use_dataset"].boolean(true)) {
            if (const Reference* ref = pickReference(a, lib, rq.seed)) {
                auto loop = loopOf(ref->degrees);
                if (loop.size() >= 2) rq.progression = loop;
                if (rq.hookRhythm.size() < 3 && !ref->rhythm.empty()) rq.hookRhythm = ref->rhythm;
                Json in = Json::object();
                in.set("id", ref->id); in.set("name", ref->name); in.set("pack", ref->pack);
                in.set("key", std::string(noteName(ref->root)) + " " + ref->mode);
                out.set("inspired_by", in);
            }
        }
        Result r = generate(rq, lib);
        notes = r.notes;
        desc = "Similar idea: " + r.description;
        Json ch = Json::array(); for (const auto& c : r.chordNames) ch.push(c);
        out.set("chords", ch);
    }
    out.set("ok", true);
    out.set("root", a.root); out.set("scale_index", a.scale);
    out.set("clip", clipToJson(notes, rq.bpm, rq.bars, desc));
    out.set("analysis", analysisToJson(a));
    return out;
}

Json similarToClip(const Json& clip, const Json& options, const PatternLibrary& lib) {
    std::vector<Note> notes; int bpm = 120, bars = 4;
    Json e = Json::object();
    if (!clipFromJson(clip, notes, bpm, bars)) { e.set("ok", false); e.set("error", "no clip loaded"); return e; }
    std::vector<Note> lead;
    for (const auto& n : notes) if (n.channel == kMelody) lead.push_back(n);
    if (lead.empty()) for (const auto& n : notes) if (n.channel != kBass) lead.push_back(n);
    if (lead.empty()) { e.set("ok", false); e.set("error", "the clip has no melody to be similar to"); return e; }
    std::sort(lead.begin(), lead.end(), [](const Note& x, const Note& y) { return x.start < y.start; });
    return studioProcess(options["mode"].str("similar"), lead, bpm, true, options, lib);
}

Json analyzeClip(const Json& clip, const PatternLibrary& lib) {
    std::vector<Note> notes; int bpm = 120, bars = 4;
    Json e = Json::object();
    if (!clipFromJson(clip, notes, bpm, bars)) { e.set("ok", false); e.set("error", "no clip loaded"); return e; }
    std::vector<Note> chords, melody;
    for (const auto& n : notes) (n.channel == kChords ? chords : melody).push_back(n);
    Json j = analysisToJson(analyzeNotes(chords, melody, bpm, lib, true));
    j.set("ok", true);
    return j;
}

} // namespace hhmg
