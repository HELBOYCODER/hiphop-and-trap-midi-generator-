// Style-aware chord / melody / 808-bass generator.
// Hard guarantee: every emitted note belongs to the selected scale (melody set for the melody
// channel, chord set for chords and bass). verify() proves it and generate() enforces it.
#pragma once
#include "Patterns.h"
#include "Theory.h"
#include <cstdint>
#include <string>
#include <vector>

namespace hhmg {

constexpr int kPPQ = 480;
constexpr int kTicksPerBar = kPPQ * 4;
constexpr int kTicksPerStep = kPPQ / 4;

enum Channel { kChords = 0, kMelody = 1, kBass = 2 };

struct Note {
    int pitch = 60;
    double start = 0;  // ticks
    double length = 0; // ticks
    int velocity = 90;
    int channel = kChords;
};

struct StyleMix { double rnb = 1.0, trap = 0.0, hiphop = 0.0; };

struct Request {
    int root = 9;                 // pitch class, A
    int scale = 1;                // index into allScales()
    int bpm = 140;
    int bars = 4;                 // 1..32
    StyleMix mix;
    double complexity = 0.5;      // 0..1
    double humanize = 0.3;        // 0..1
    double swing = 0.0;           // 0..1
    double velocityVariation = 0.3;
    bool chords = true, melody = true, bass = false;
    int melodyOctave = 5;         // octave of the melody's centre (C5 = 72 with 5)
    std::vector<int> progression; // optional explicit 1-based degrees; empty = pick from library
    uint32_t seed = 1;
};

struct Result {
    Request request;
    Scale scale;
    std::vector<Note> notes;
    std::vector<std::string> chordNames;
    std::vector<int> degrees; // 1-based, per chord segment
    std::string description;
};

Result generate(const Request& req, const PatternLibrary& lib);
// Number of notes outside the scale. Always 0 for generate() output.
int countOutOfScale(const Result& r);

} // namespace hhmg
