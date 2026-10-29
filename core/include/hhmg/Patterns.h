// Pattern library: rhythms, melodic motifs and chord progressions per style.
// A built-in seed set ships in the binary; datasets produced by tools/dataset/ingest.py
// are merged on top (patterns.json).
#pragma once
#include "Json.h"
#include <string>
#include <vector>

namespace hhmg {

struct RhythmNote { double step; double len; int vel; }; // in 16th-note steps from pattern start
struct Rhythm {
    std::string name, style; // style: rnb | trap | hiphop
    int bars = 1;
    std::vector<RhythmNote> notes;
};
struct Motif { std::string style; std::vector<int> steps; };            // relative scale-degree moves
struct Progression { std::string style, mode; std::vector<int> degrees; }; // mode: minor | major, degrees 1-based

// Per-file profile (no notes): what a reference MIDI is, so the AI can find and combine real examples.
struct Reference {
    std::string id, pack, name, style, mode; // mode: minor | major
    int root = 0, bars = 0;
    double bpm = 0, density = 0, swing = 0;
    std::vector<int> degrees;                // diatonic degree (1-7) per bar
    std::vector<RhythmNote> rhythm;          // one representative bar of melody onsets
};

// What "<artist/vibe> type beat" listings look like in aggregate (tempo range, key mood, tags). Statistics from public
// listing metadata only - no audio, no MIDI, no copy of any beat.
struct TypeBeatStyle {
    std::string name;                 // e.g. "metro boomin"
    int count = 0;                    // how many listings the statistics come from
    double bpmLow = 0, bpmMedian = 0, bpmHigh = 0;
    double minorShare = 0.5;          // 0..1
    std::vector<std::string> topKeys; // "F# minor", ...
    std::vector<std::string> tags;    // dark, melodic, drill ...
    double rnb = 0.1, trap = 1.0, hiphop = 0.2;
    std::vector<std::string> sources;
};

struct PatternLibrary {
    std::vector<Rhythm> rhythms;
    std::vector<Motif> motifs;
    std::vector<Progression> progressions;
    std::vector<Reference> references;
    std::vector<TypeBeatStyle> typeBeats;

    static PatternLibrary builtin();
    void mergeJson(const Json& j); // throws std::runtime_error on malformed input
    Json toJson() const;
};

} // namespace hhmg
