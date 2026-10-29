#include "hhmg/Studio.h"
#include "hhmg/MidiFile.h"
#include <algorithm>
#include <cmath>

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
    } else { // similar: same key, tempo, feel and rhythm, new notes
        rq.chords = true; rq.melody = true;
        rq.hookRhythm = a.rhythm;
        Result r = generate(rq, lib);
        notes = r.notes;
        desc = "Similar idea: " + r.description;
        Json ch = Json::array(); for (const auto& c : r.chordNames) ch.push(c);
        out.set("chords", ch);
    }
    out.set("ok", true);
    out.set("clip", clipToJson(notes, rq.bpm, rq.bars, desc));
    out.set("analysis", analysisToJson(a));
    return out;
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
