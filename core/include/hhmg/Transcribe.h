// Voice / monophonic audio -> notes. YIN pitch tracking, note segmentation, tempo estimate, grid quantisation.
#pragma once
#include "Generator.h"
#include <string>
#include <vector>

namespace hhmg {

struct PitchFrame { double time = 0; double midi = 0; double confidence = 0; bool voiced = false; }; // midi is fractional

struct TranscribeOptions {
    double bpm = 0;            // 0 = estimate from the note onsets
    double minNoteMs = 90;
    bool quantize = true;      // snap starts/lengths to the 16th grid
};

struct Transcription {
    std::vector<PitchFrame> contour;   // for drawing the frequency curve
    std::vector<Note> notes;           // channel = kMelody, ticks at kPPQ, first note starts at 0
    double bpm = 120;
    bool bpmEstimated = true;
    double durationSec = 0;
    double startOffsetSec = 0;         // recording time of the first note (clip tick 0)
    std::string error;
};

// mono samples in -1..1 at any sample rate >= 8000
Transcription transcribeAudio(const std::vector<float>& mono, int sampleRate, const TranscribeOptions& opt = {});

} // namespace hhmg
