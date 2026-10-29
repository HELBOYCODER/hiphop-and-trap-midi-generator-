// Turning a user's own MIDI files into reference profiles + learned patterns (no notes are kept),
// so the AI and the "similar" mode can draw on the user's dataset on every platform.
#pragma once
#include "Analyze.h"
#include "Patterns.h"
#include <string>
#include <vector>

namespace hhmg {

// key written in a file name, e.g. "Essential MIDI 34 - G Min.mid" -> root 7, minor. false if none.
bool keyFromName(const std::string& name, int& root, bool& minor);

// Adds one reference (+ its progression / rhythm as learned patterns) to `lib`, and to `delta` when given
// (delta collects only what the user added, so it can be saved on its own). Returns false with `error` on bad MIDI.
bool indexMidiBytes(const std::vector<uint8_t>& bytes, const std::string& pack, const std::string& name,
                    PatternLibrary& lib, PatternLibrary* delta, std::string& error);

// Recursively indexes *.mid / *.midi under `dir`. Returns the number of files added.
int indexMidiFolder(const std::string& dir, PatternLibrary& lib, PatternLibrary* delta, int maxFiles, std::string& error);

} // namespace hhmg
