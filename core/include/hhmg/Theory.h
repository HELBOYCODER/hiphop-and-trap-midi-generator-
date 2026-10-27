// Scale / mode / chord theory. Everything here is pitch-class exact, which is what
// lets the generator guarantee that no note ever leaves the selected scale.
#pragma once
#include <string>
#include <vector>

namespace hhmg {

struct ScaleDef {
    const char* id;
    const char* label;
    std::vector<int> intervals;   // melody set (semitones above root)
    std::vector<int> chordParent; // 7-note scale chords are stacked on (== intervals for heptatonic)
};

const std::vector<ScaleDef>& allScales();
int findScale(const std::string& id); // -1 when unknown
const char* noteName(int pitchClass);
int parseRoot(const std::string& name); // "C", "F#", "Bb", "9" ... -> 0..11, -1 on failure

struct Scale {
    int root = 9;   // pitch class
    int index = 1;  // index into allScales()

    const ScaleDef& def() const { return allScales().at((size_t) index); }
    bool isMinorish() const;

    bool contains(int pitch) const;      // melody set
    bool chordContains(int pitch) const; // chord set (== contains for 7-note scales)
    int snap(int pitch) const;           // nearest melody-set pitch (ties go down)
    int snapChord(int pitch) const;      // nearest chord-set pitch

    // Melody-set degree -> midi pitch. Degree 0 is the tonic at `tonic`; degrees may be negative or > size.
    int degreeToPitch(int tonic, int degree) const;
    // Chord-scale step -> midi pitch (steps stack in 7-note parent scale).
    int chordStepToPitch(int tonic, int step) const;
};

struct Chord {
    int degree = 0;          // 0-based degree of the 7-note chord scale
    std::vector<int> pitches; // ascending, voiced
    int rootPc = 0;
    std::string name;
};

// Stack thirds on `degree`. extensions: 0 triad, 1 = +7th, 2 = +9th, 3 = +11th, 4 = +13th.
Chord buildChord(const Scale& s, int degree, int extensions, int inversion, bool spread, int centre = 60);

} // namespace hhmg
