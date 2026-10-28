// Minimal Standard MIDI File reader (format 0/1) -> notes in kPPQ ticks.
#pragma once
#include "Generator.h"
#include <string>
#include <vector>

namespace hhmg {

struct MidiTrackNotes { std::vector<Note> notes; };
struct MidiData {
    double bpm = 120;
    bool hasTempo = false;      // false: the file carries no tempo event, bpm is only the MIDI default
    std::vector<MidiTrackNotes> tracks; // drums (channel 10) are dropped
};

// Returns false (and fills `error`) on malformed files. Never throws.
bool readMidi(const std::vector<uint8_t>& bytes, MidiData& out, std::string& error);
bool readMidiFile(const std::string& path, MidiData& out, std::string& error);

} // namespace hhmg
