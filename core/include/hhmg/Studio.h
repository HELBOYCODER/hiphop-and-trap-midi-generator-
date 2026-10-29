// Voice-to-MIDI studio pipeline used by the Android app (and tested natively):
// transcribed melody -> key / style analysis -> clip (melody only, melody + chords/808, or a new "similar" idea).
#pragma once
#include "Analyze.h"
#include "Transcribe.h"

namespace hhmg {

// Clip JSON: {"bpm":,"bars":,"description":,"notes":[[pitch,startTicks,lengthTicks,velocity,channel],...]}
Json clipToJson(const std::vector<Note>& notes, int bpm, int bars, const std::string& description);
bool clipFromJson(const Json& clip, std::vector<Note>& notes, int& bpm, int& bars);
std::vector<uint8_t> clipToMidi(const Json& clip);

Json transcriptionToJson(const Transcription& t); // contour + notes + bpm

// mode: "melody" | "melody_plus" | "similar".  options: {seed, style (rnb|trap|hiphop), bass (bool), complexity, scale_lock (bool)}
Json studioProcess(const std::string& mode, const std::vector<Note>& melody, double bpm, bool bpmKnown,
                   const Json& options, const PatternLibrary& lib);

// "similar" for a clip already in the app: melody = its melody channel (or everything but bass), tempo = the clip's.
Json similarToClip(const Json& clip, const Json& options, const PatternLibrary& lib);

// analysis of an arbitrary clip (used by the AI's analyze_current_clip tool)
Json analyzeClip(const Json& clip, const PatternLibrary& lib);

} // namespace hhmg
