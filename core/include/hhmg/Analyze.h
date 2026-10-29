// Style recognition for MIDI: detects key/mode, tempo, chord degrees, harmonic richness, swing, looseness,
// melodic repetition/space, scores R&B vs Trap vs Hip-Hop (heuristics + similarity to the learned library),
// and proposes the plugin settings that reproduce the feel.
#pragma once
#include "Generator.h"
#include "MidiRead.h"
#include "Patterns.h"
#include <string>
#include <vector>

namespace hhmg {

struct Analysis {
    int root = 0, scale = 1, bars = 0, chordNotes = 0, melodyNotes = 0;
    bool bpmKnown = true;
    double bpm = 120, extensionRatio = 0, density = 0, swing = 0, humanize = 0, repeatRatio = 0, restRatio = 0;
    std::vector<int> degrees;
    std::vector<RhythmNote> rhythm;
    StyleMix style;
    std::string topStyle;
    double confidence = 0;
    std::vector<std::string> reasons;
    Request suggested;
};

// bpmKnown=false (e.g. a loop with no tempo event): tempo is ignored for style scoring and the suggestion uses a style-typical BPM.
Analysis analyzeNotes(const std::vector<Note>& chords, const std::vector<Note>& melody, double bpm, const PatternLibrary& lib, bool bpmKnown = true,
                      int forcedRoot = -1, int forcedScale = -1); // forced key e.g. read from a file name ("... G Min")
Analysis analyzeMidi(const MidiData& midi, const PatternLibrary& lib, int forcedRoot = -1, int forcedScale = -1); // splits tracks into chord / melody roles
Json analysisToJson(const Analysis& a);

} // namespace hhmg
