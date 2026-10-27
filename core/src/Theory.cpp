#include "hhmg/Theory.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace hhmg {

static int floorDiv(int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
static int mod12(int v) { return ((v % 12) + 12) % 12; }

const std::vector<ScaleDef>& allScales() {
    static const std::vector<int> maj{0, 2, 4, 5, 7, 9, 11}, min{0, 2, 3, 5, 7, 8, 10};
    static const std::vector<ScaleDef> s = {
        {"major", "Major (Ionian)", maj, maj},
        {"minor", "Natural Minor (Aeolian)", min, min},
        {"harmonic_minor", "Harmonic Minor", {0, 2, 3, 5, 7, 8, 11}, {0, 2, 3, 5, 7, 8, 11}},
        {"melodic_minor", "Melodic Minor", {0, 2, 3, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 11}},
        {"dorian", "Dorian", {0, 2, 3, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 9, 10}},
        {"phrygian", "Phrygian", {0, 1, 3, 5, 7, 8, 10}, {0, 1, 3, 5, 7, 8, 10}},
        {"lydian", "Lydian", {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 6, 7, 9, 11}},
        {"mixolydian", "Mixolydian", {0, 2, 4, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10}},
        {"locrian", "Locrian", {0, 1, 3, 5, 6, 8, 10}, {0, 1, 3, 5, 6, 8, 10}},
        {"phrygian_dominant", "Phrygian Dominant", {0, 1, 4, 5, 7, 8, 10}, {0, 1, 4, 5, 7, 8, 10}},
        {"pentatonic_major", "Major Pentatonic", {0, 2, 4, 7, 9}, maj},
        {"pentatonic_minor", "Minor Pentatonic", {0, 3, 5, 7, 10}, min},
        {"blues", "Blues", {0, 3, 5, 6, 7, 10}, min},
    };
    return s;
}

int findScale(const std::string& id) {
    std::string k;
    for (char c : id) k += (c == ' ' || c == '-') ? '_' : (char) std::tolower((unsigned char) c);
    if (k == "ionian") k = "major";
    if (k == "aeolian" || k == "natural_minor") k = "minor";
    if (k == "major_pentatonic") k = "pentatonic_major";
    if (k == "minor_pentatonic") k = "pentatonic_minor";
    const auto& s = allScales();
    for (size_t i = 0; i < s.size(); ++i) if (k == s[i].id) return (int) i;
    return -1;
}

const char* noteName(int pc) {
    static const char* n[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return n[mod12(pc)];
}

int parseRoot(const std::string& name) {
    if (name.empty()) return -1;
    if (std::isdigit((unsigned char) name[0])) return mod12(std::atoi(name.c_str()));
    static const int base[] = {9, 11, 0, 2, 4, 5, 7}; // A..G
    char c = (char) std::toupper((unsigned char) name[0]);
    if (c < 'A' || c > 'G') return -1;
    int pc = base[c - 'A'];
    for (size_t i = 1; i < name.size(); ++i) {
        if (name[i] == '#') ++pc;
        else if (name[i] == 'b') --pc;
    }
    return mod12(pc);
}

bool Scale::isMinorish() const {
    const auto& cp = def().chordParent;
    return cp.size() > 2 && cp[2] == 3;
}

static bool inSet(const std::vector<int>& iv, int root, int pitch) {
    int rel = mod12(pitch - root);
    return std::find(iv.begin(), iv.end(), rel) != iv.end();
}

bool Scale::contains(int pitch) const { return inSet(def().intervals, root, pitch); }
bool Scale::chordContains(int pitch) const { return inSet(def().chordParent, root, pitch); }

static int snapTo(const std::vector<int>& iv, int root, int pitch) {
    for (int d = 0; d < 7; ++d) {
        if (inSet(iv, root, pitch - d)) return pitch - d; // ties resolve downward
        if (inSet(iv, root, pitch + d)) return pitch + d;
    }
    return pitch;
}

int Scale::snap(int pitch) const { return snapTo(def().intervals, root, pitch); }
int Scale::snapChord(int pitch) const { return snapTo(def().chordParent, root, pitch); }

int Scale::degreeToPitch(int tonic, int degree) const {
    const auto& iv = def().intervals;
    int n = (int) iv.size();
    int oct = floorDiv(degree, n);
    return tonic + 12 * oct + iv[(size_t) (degree - oct * n)];
}

int Scale::chordStepToPitch(int tonic, int step) const {
    const auto& iv = def().chordParent;
    int oct = floorDiv(step, 7);
    return tonic + 12 * oct + iv[(size_t) (step - oct * 7)];
}

Chord buildChord(const Scale& s, int degree, int extensions, int inversion, bool spread, int centre) {
    Chord c;
    c.degree = degree;
    int tonic = 48 + s.root; // C3-based tonic; whole chord is re-centred below by octave shifts only
    static const int stackSteps[] = {0, 2, 4, 6, 8, 10, 12};
    int count = 3 + std::max(0, std::min(4, extensions));
    std::vector<int> p;
    for (int i = 0; i < count; ++i) p.push_back(s.chordStepToPitch(tonic, degree + stackSteps[i]));
    c.rootPc = mod12(p[0]);

    // name: quality from the 3rd/5th/7th
    int third = mod12(p[1] - p[0]), fifth = mod12(p[2] - p[0]);
    std::string q;
    if (third == 3 && fifth == 6) q = "dim";
    else if (third == 3) q = "m";
    else if (third == 4 && fifth == 8) q = "aug";
    else if (third == 4) q = "";
    else q = "sus";
    if (count >= 4) {
        int seventh = mod12(p[3] - p[0]);
        const char* ext = count >= 7 ? "13" : count >= 6 ? "11" : count >= 5 ? "9" : "7";
        if (third == 3 && fifth == 6) q = seventh == 9 ? "dim7" : "m7b5";
        else if (seventh == 11) q = (third == 3 ? "m(maj" : "maj") + std::string(ext) + (third == 3 ? ")" : "");
        else q += ext;
    }
    c.name = std::string(noteName(c.rootPc)) + q;

    // inversion: lift lowest notes an octave
    inversion = std::max(0, std::min(inversion, (int) p.size() - 1));
    for (int i = 0; i < inversion; ++i) { p[(size_t) i] += 12; }
    std::sort(p.begin(), p.end());
    // open voicing: drop the second-highest voice by an octave
    if (spread && p.size() >= 4) { p[p.size() - 2] -= 12; std::sort(p.begin(), p.end()); }

    // re-centre with whole-octave shifts so the voicing sits around `centre`
    double mean = 0;
    for (int v : p) mean += v;
    mean /= (double) p.size();
    int shift = (int) std::lround((centre - mean) / 12.0) * 12;
    for (int& v : p) v += shift;
    c.pitches = p;
    return c;
}

} // namespace hhmg
