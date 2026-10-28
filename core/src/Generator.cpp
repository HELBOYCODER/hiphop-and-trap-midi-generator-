#include "hhmg/Generator.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace hhmg {
namespace {

// Deterministic across platforms (std distributions are not).
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0xD1B54A32D192ED03ull) { next(); next(); }
    uint64_t next() {
        s += 0x9E3779B97F4A7C15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uni() { return (double) (next() >> 11) / 9007199254740992.0; }
    int range(int lo, int hi) { return lo + (int) (uni() * (hi - lo + 1)); } // inclusive
    bool chance(double p) { return uni() < p; }
    double bipolar() { return uni() * 2.0 - 1.0; }
    size_t pick(const std::vector<double>& w) {
        double t = 0; for (double v : w) t += v;
        if (t <= 0) return (size_t) range(0, (int) w.size() - 1);
        double r = uni() * t;
        for (size_t i = 0; i < w.size(); ++i) { r -= w[i]; if (r < 0) return i; }
        return w.size() - 1;
    }
};

double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

double styleWeight(const StyleMix& m, const std::string& style) {
    if (style == "rnb") return m.rnb;
    if (style == "trap") return m.trap;
    return m.hiphop;
}

StyleMix normalised(StyleMix m) {
    m.rnb = std::max(0.0, m.rnb); m.trap = std::max(0.0, m.trap); m.hiphop = std::max(0.0, m.hiphop);
    double t = m.rnb + m.trap + m.hiphop;
    if (t <= 1e-9) return {1, 0, 0};
    return {m.rnb / t, m.trap / t, m.hiphop / t};
}

struct Segment { int startTick, lenTicks; Chord chord; int degree1; };

} // namespace

int countOutOfScale(const Result& r) {
    int bad = 0;
    for (const auto& n : r.notes) {
        bool ok = n.channel == kMelody ? r.scale.contains(n.pitch) : r.scale.chordContains(n.pitch);
        if (!ok) ++bad;
    }
    return bad;
}

Result generate(const Request& in, const PatternLibrary& lib) {
    Request rq = in;
    rq.root = ((rq.root % 12) + 12) % 12;
    if (rq.scale < 0 || rq.scale >= (int) allScales().size()) rq.scale = 1;
    rq.bpm = std::max(40, std::min(220, rq.bpm));
    rq.bars = std::max(1, std::min(32, rq.bars));
    rq.complexity = clamp01(rq.complexity);
    rq.humanize = clamp01(rq.humanize);
    rq.swing = clamp01(rq.swing);
    rq.velocityVariation = clamp01(rq.velocityVariation);
    rq.melodyOctave = std::max(3, std::min(7, rq.melodyOctave));
    rq.mix = normalised(rq.mix);

    Result res;
    res.request = rq;
    res.scale = Scale{rq.root, rq.scale};
    const Scale& sc = res.scale;
    const StyleMix& mix = rq.mix;
    const double cx = rq.complexity;
    Rng rng(rq.seed);

    // ---- progression -------------------------------------------------------------------
    std::vector<int> prog = rq.progression;
    prog.erase(std::remove_if(prog.begin(), prog.end(), [](int d) { return d < 1 || d > 7; }), prog.end());
    if (prog.empty()) {
        const char* mode = sc.isMinorish() ? "minor" : "major";
        std::vector<const Progression*> pool;
        std::vector<double> w;
        for (const auto& p : lib.progressions)
            if (p.mode == mode) { pool.push_back(&p); w.push_back(styleWeight(mix, p.style) + 0.02); }
        if (pool.empty()) prog = {1, 4, 5, 1};
        else prog = pool[rng.pick(w)]->degrees;
    }

    // ---- segments (one chord each) ------------------------------------------------------
    const int L = (int) prog.size();
    const int segCount = rq.bars >= L ? rq.bars : L;
    const int total = rq.bars * kTicksPerBar;
    std::vector<Segment> segs;
    for (int i = 0; i < segCount; ++i) {
        int a = total * i / segCount, b = total * (i + 1) / segCount;
        int degree1 = prog[(size_t) (i % L)];
        // complexity -> extensions; R&B leans on 9ths, trap/hip-hop stay lean
        double ext7 = clamp01(0.15 + 0.85 * cx * (0.6 + 0.4 * mix.rnb));
        double ext9 = clamp01(cx * (0.15 + 0.8 * mix.rnb) - 0.1);
        double ext11 = clamp01(cx * cx * 0.5 * mix.rnb);
        int ext = 0;
        if (rng.chance(ext7)) { ext = 1; if (rng.chance(ext9)) { ext = 2; if (rng.chance(ext11)) ext = 3; } }
        bool spread = rng.chance(0.25 + 0.5 * mix.rnb);
        int centre = mix.trap > 0.5 ? 57 : 60;
        // Voice leading: pick the inversion whose voices travel least from the previous chord
        // (common tones stay put), so progressions sound connected instead of jumping.
        Chord chord = buildChord(sc, degree1 - 1, ext, 0, spread, centre);
        if (!segs.empty()) {
            const auto& prev = segs.back().chord.pitches;
            double best = 1e9;
            for (int inv = 0; inv <= 2; ++inv) {
                Chord c = buildChord(sc, degree1 - 1, ext, inv, spread, centre);
                double cost = 0;
                size_t n = std::min(prev.size(), c.pitches.size());
                for (size_t k = 0; k < n; ++k) cost += std::abs(c.pitches[k] - prev[k]);
                cost += 0.5 * std::abs(c.pitches.front() - prev.front());
                if (cost < best) { best = cost; chord = c; }
            }
        }
        Segment sg{a, b - a, chord, degree1};
        segs.push_back(sg);
        res.chordNames.push_back(sg.chord.name);
        res.degrees.push_back(degree1);
    }
    auto segAt = [&](double tick) -> const Segment& {
        for (size_t i = segs.size(); i-- > 0;) if (tick >= segs[i].startTick) return segs[i];
        return segs[0];
    };

    auto pickRhythm = [&](int bars, double targetNotes) -> const Rhythm* {
        std::vector<const Rhythm*> pool; std::vector<double> w;
        for (const auto& r : lib.rhythms) {
            if (r.bars != bars) continue;
            double dens = (double) r.notes.size() / r.bars;
            pool.push_back(&r);
            w.push_back((styleWeight(mix, r.style) + 0.01) * std::exp(-std::fabs(dens - targetNotes) / 2.5));
        }
        if (pool.empty()) return nullptr;
        return pool[rng.pick(w)];
    };

    // ---- chords ---------------------------------------------------------------------------
    if (rq.chords) {
        for (const auto& sg : segs) {
            bool comp = rng.chance(0.25 + 0.55 * cx);
            const Rhythm* r = comp ? pickRhythm(1, 3.0 + 2.5 * cx) : nullptr;
            auto emit = [&](double start, double len, int vel) {
                // gentle upward strum; humanize() scales it
                for (size_t k = 0; k < sg.chord.pitches.size(); ++k) {
                    Note n; n.pitch = sg.chord.pitches[k]; n.start = start + (double) k * 4 * rq.humanize;
                    n.length = std::max(60.0, len - (double) k * 4 * rq.humanize);
                    n.velocity = vel - (int) k * 2; n.channel = kChords;
                    res.notes.push_back(n);
                }
            };
            if (!r) {
                emit(sg.startTick, sg.lenTicks * 0.97, 78 + rng.range(0, 8));
            } else {
                for (int b = 0; b * kTicksPerBar < sg.lenTicks; ++b) {
                    for (const auto& rn : r->notes) {
                        double st = (double) sg.startTick + b * kTicksPerBar + rn.step * kTicksPerStep;
                        if (st >= sg.startTick + sg.lenTicks - 1) continue;
                        double len = std::min(rn.len * kTicksPerStep * 0.92, (double) (sg.startTick + sg.lenTicks) - st);
                        emit(st, len, std::min(100, rn.vel - 14));
                    }
                }
            }
        }
    }

    // ---- melody archetype -------------------------------------------------------------------------
    // R&B keeps the flowing line below. Trap / hip-hop use producer idioms instead: plucky chord-tone
    // arpeggios, sparse high "bell" calls, and short repeated hooks built on a small palette. These are
    // deliberately not scale runs: repetition, leaps, space and held resolutions read as mature.
    enum class Arch { flow, arp, bell, hook };
    Arch arch = Arch::flow;
    if (rq.melody && rng.uni() >= mix.rnb) {
        double t = mix.trap, h = mix.hiphop;
        if (t + h <= 1e-9) t = 1;
        std::vector<double> w = {t * 0.35 + h * 0.20, t * 0.30 + h * 0.15, t * 0.35 + h * 0.65};
        arch = (Arch) (1 + (int) rng.pick(w));
    }

    if (rq.melody && arch != Arch::flow) {
        const int tonic = 12 * (rq.melodyOctave + 1) + rq.root;
        const int lo = tonic - 5, hi = tonic + 19;
        auto pitchOf = [&](int degree) { return sc.degreeToPitch(tonic, degree); };
        auto pcIn = [](int pitch, const std::vector<int>& pcs) {
            for (int v : pcs) if (((pitch - v) % 12 + 12) % 12 == 0) return true;
            return false;
        };
        auto chordPcs = [](const Chord& c) { std::vector<int> v; for (int p : c.pitches) v.push_back(((p % 12) + 12) % 12); return v; };
        auto add = [&](double start, double len, int pitch, int vel) {
            Note n; n.pitch = sc.snap(pitch); n.start = start; n.length = std::max(0.8 * kTicksPerStep, len);
            n.velocity = std::max(1, std::min(124, vel)); n.channel = kMelody;
            res.notes.push_back(n);
        };
        const int setSize = (int) sc.def().intervals.size();

        if (arch == Arch::arp) {
            static const std::vector<std::vector<int>> shapes = {
                {0, 1, 2, 1}, {0, 2, 1, 2}, {0, 1, 2, 3, 2, 1}, {2, 1, 0, 1}, {0, 0, 2, 1}, {0, 2, 3, 2}, {1, 0, 2, 0}};
            auto shape = shapes[(size_t) rng.range(0, (int) shapes.size() - 1)];
            const double drop = 0.10 + 0.25 * (1.0 - cx);
            const bool sixteenth = rng.chance(0.25 + 0.5 * cx);
            for (int bar = 0; bar < rq.bars; ++bar) {
                const Segment& sg = segAt((double) bar * kTicksPerBar);
                auto pcs = chordPcs(sg.chord);
                std::vector<int> ladder;
                for (int p = lo; p <= hi; ++p) if (pcIn(p, pcs)) ladder.push_back(p);
                if (ladder.size() < 3) continue;
                const bool lastBar = bar == rq.bars - 1;
                size_t base = ladder.size() / 3;
                size_t pos = 0;
                const int grid = sixteenth && bar % 2 == 1 ? 1 : 2;
                for (int st = 0; st < 16; st += grid) {
                    if (lastBar && st >= 8) break;
                    if (st > 0 && rng.chance(drop)) { ++pos; continue; }
                    size_t idx = std::min(ladder.size() - 1, base + (size_t) shape[pos % shape.size()]);
                    ++pos;
                    int vel = (st % 4 == 0 ? 98 : 80) + rng.range(-4, 4);
                    add((double) bar * kTicksPerBar + st * kTicksPerStep, std::max(1, grid - 1) * kTicksPerStep * 0.9, ladder[idx], vel); // plucky: leaves air
                }
                if (lastBar) {  // resolve on the root, held
                    int rootP = tonic; while (rootP < lo + 5) rootP += 12;
                    add((double) bar * kTicksPerBar + 8 * kTicksPerStep, 7 * kTicksPerStep, rootP, 100);
                }
            }
        } else if (arch == Arch::bell) {
            static const std::vector<std::vector<double>> hits = {
                {0, 6, 10}, {0, 8, 11}, {2, 8, 14}, {0, 3, 8}, {0, 7, 12}, {4, 10, 14}, {0, 10}};
            // call-and-response palette: root, 3rd/b3, 5th, octave; phrygian/harmonic minor add their colour tone
            std::vector<int> palette = {0, 2, 4, setSize};
            if (rq.scale == findScale("phrygian") || rq.scale == findScale("phrygian_dominant")) palette.push_back(1);
            if (rq.scale == findScale("harmonic_minor")) palette.push_back(6);
            std::vector<int> call;
            auto hitsA = hits[(size_t) rng.range(0, (int) hits.size() - 1)];
            for (size_t i = 0; i < hitsA.size(); ++i) call.push_back(palette[(size_t) rng.range(0, (int) palette.size() - 1)]);
            auto hitsB = rng.chance(0.5) ? hitsA : hits[(size_t) rng.range(0, (int) hits.size() - 1)];
            for (int bar = 0; bar < rq.bars; ++bar) {
                const bool answer = bar % 2 == 1;
                const bool lastBar = bar == rq.bars - 1;
                auto steps = answer ? hitsB : hitsA;
                for (size_t i = 0; i < steps.size(); ++i) {
                    int deg = call[i % call.size()];
                    if (answer && i + 1 == steps.size()) deg = palette[(size_t) rng.range(0, 2)];     // answer ends differently
                    if (bar % 4 == 3 && i + 1 == steps.size()) deg = 0;                               // phrase resolves
                    double st = steps[i];
                    double len = (i + 1 < steps.size() ? steps[i + 1] - st : 16 - st) * kTicksPerStep * 0.95;
                    int pitch = pitchOf(deg + setSize);                                               // an octave above the tonic
                    while (pitch > hi) pitch -= 12;
                    if (lastBar && i + 1 == steps.size()) { pitch = tonic + 12; len = (16 - st) * kTicksPerStep; }
                    add((double) bar * kTicksPerBar + st * kTicksPerStep, std::min(len, 4.0 * kTicksPerStep), pitch, 92 - (int) i * 3 + rng.range(-3, 3));
                }
            }
        } else { // hook: 4-note cell on a small palette, sequenced A A' A B over a library rhythm
            std::vector<int> palette = {0, 2, 4, -1, 1};
            if (rng.chance(0.5)) palette = {0, 2, 3, 4, -3};
            std::vector<int> cell;
            int prev = 0;
            for (int i = 0; i < 4; ++i) {
                int d = palette[(size_t) rng.range(0, (int) palette.size() - 1)];
                if (i > 0 && (std::abs(d - prev) == 1 ? rng.chance(0.8) : rng.chance(0.3))) d = prev;  // hooks repeat; they do not walk the scale
                cell.push_back(d); prev = d;
            }
            // Learned data shapes the hook: with a library motif (weighted by style) the cell follows a
            // real producer's contour instead of the hand-made palette.
            {
                std::vector<const Motif*> mp; std::vector<double> mw;
                for (const auto& m : lib.motifs) if (m.style != "rnb" || mix.rnb > 0.5) { mp.push_back(&m); mw.push_back(styleWeight(mix, m.style) + 1e-3); }
                if (!mp.empty() && rng.chance(0.6)) {
                    const Motif& m = *mp[rng.pick(mw)];
                    cell.clear();
                    int acc = 0;
                    for (size_t i = 0; i < m.steps.size() && i < 5; ++i) {
                        acc += m.steps[i];
                        acc = std::max(-3, std::min(5, acc));
                        cell.push_back(acc);
                    }
                }
            }
            const Rhythm* rhA = pickRhythm(1, 3.0 + 2.0 * cx);
            const Rhythm* rhB = pickRhythm(1, 2.5 + 2.0 * cx);
            Rhythm guide;
            if (!rq.hookRhythm.empty()) { guide.name = "reference"; guide.bars = 1; guide.notes = rq.hookRhythm; rhA = &guide; }
            if (!rhA) rhA = rhB;
            if (!rhA) rhA = &lib.rhythms.front();
            if (!rhB) rhB = rhA;
            for (int bar = 0; bar < rq.bars; ++bar) {
                const bool b = bar % 4 == 3;
                const Rhythm* rh = b ? rhB : rhA;
                for (size_t i = 0; i < rh->notes.size(); ++i) {
                    const auto& rn = rh->notes[i];
                    int deg = cell[i % cell.size()] + (bar % 4 == 2 && i > 0 ? 1 : 0);      // third bar nudges up
                    if (bar % 4 == 1 && i + 1 == rh->notes.size()) deg = cell[0] + 2;        // A' changes only the tail
                    const bool fin = (bar % 4 == 3 || bar == rq.bars - 1) && i + 1 == rh->notes.size();
                    if (fin) deg = 0;
                    int pitch = pitchOf(deg);
                    while (pitch > hi) pitch -= 12;
                    while (pitch < lo) pitch += 12;
                    double st = (double) bar * kTicksPerBar + rn.step * kTicksPerStep;
                    double len = rn.len * kTicksPerStep * (fin ? 1.8 : 0.9);
                    add(st, std::min(len, (double) (bar + 1) * kTicksPerBar - st), pitch, rn.vel + (i == 0 ? 8 : -2) + rng.range(-3, 3));
                }
            }
        }
    }

    // ---- melody (flowing line: R&B) -----------------------------------------------------------
    if (rq.melody && arch == Arch::flow) {
        const int tonic = 12 * (rq.melodyOctave + 1) + rq.root;       // tonic pitch near octave centre
        const int lo = tonic - 7, hi = tonic + 14;                       // playable window
        auto pitchOf = [&](int degree) { return sc.degreeToPitch(tonic, degree); };
        auto isChordTone = [&](int pitch, const Chord& c) {
            for (int p : c.pitches) if (((p - pitch) % 12 + 12) % 12 == 0) return true;
            return false;
        };
        const int setSize = (int) sc.def().intervals.size();
        auto clampDeg = [&](int d) {
            while (pitchOf(d) > hi) d -= setSize;
            while (pitchOf(d) < lo) d += setSize;
            return d;
        };

        // motif pool by style
        std::vector<const Motif*> mpool; std::vector<double> mw;
        for (const auto& m : lib.motifs) { mpool.push_back(&m); mw.push_back(styleWeight(mix, m.style) + 0.02); }
        const Motif* motif = mpool.empty() ? nullptr : mpool[rng.pick(mw)];
        size_t motifIdx = 0;

        // step-size weights: trap repeats & leaps, R&B glides, hip-hop mixes
        auto stepChoice = [&]() {
            static const int moves[] = {-3, -2, -1, 0, 1, 2, 3};
            std::vector<double> w = {
                0.3 + mix.trap * 0.6 * cx,
                0.7 + mix.rnb * 0.5,
                1.6 + mix.rnb * 1.2,
                0.4 + mix.trap * 1.6 + mix.hiphop * 1.0,
                1.6 + mix.rnb * 1.2,
                0.7 + mix.rnb * 0.5,
                0.3 + mix.trap * 0.6 * cx};
            return moves[rng.pick(w)];
        };

        int degree = 0;                       // current melody-set degree
        struct PhraseNote { double stepInBar; double len; int vel; int deltaDeg; };
        std::vector<PhraseNote> phraseA;      // remembered for repetition
        const Rhythm* rhythmA = nullptr;

        const double target = 2.0 + 5.0 * cx; // notes per bar
        for (int bar = 0; bar < rq.bars; ++bar) {
            bool repeatA = bar % 2 == 0 && bar > 0 && rhythmA && rng.chance(0.6);
            const Rhythm* rhy = nullptr;
            if (repeatA) rhy = rhythmA;
            else {
                // two-bar rhythms are used for bar pairs when available
                rhy = pickRhythm(1, target);
                if (!rhy) break;
                if (bar % 4 == 0) rhythmA = rhy;
            }

            // sequence of (stepInBar, len, vel)
            std::vector<RhythmNote> notes = rhy->notes;
            // trim by complexity: low complexity drops weak-position notes
            if (cx < 0.6 && notes.size() > 3) {
                for (size_t i = notes.size(); i-- > 0 && notes.size() > 3;)
                    if (std::fmod(notes[i].step, 2.0) != 0.0 && rng.chance(0.7 - cx)) notes.erase(notes.begin() + (long) i);
            }
            std::vector<PhraseNote> thisPhrase;
            bool lastBarOfPhrase = (bar % 4 == 3) || bar == rq.bars - 1;

            for (size_t i = 0; i < notes.size(); ++i) {
                const auto& rn = notes[i];
                double st = (double) bar * kTicksPerBar + rn.step * kTicksPerStep;
                const Segment& sg = segAt(st);
                bool strong = std::fmod(rn.step, 4.0) == 0.0 || i == 0 || rn.len >= 3.0;
                bool isFinal = lastBarOfPhrase && i + 1 == notes.size();

                int newDeg = degree;
                if (repeatA && i < phraseA.size()) newDeg = degree + phraseA[i].deltaDeg;
                else if (motif && rng.chance(0.45)) newDeg = degree + motif->steps[motifIdx++ % motif->steps.size()];
                else newDeg = degree + stepChoice();
                newDeg = clampDeg(newDeg);

                if (isFinal) {
                    // cadence: land on the tonic when close, otherwise nearest chord tone
                    int best = newDeg, bestDist = 99;
                    for (int d = newDeg - 4; d <= newDeg + 4; ++d) {
                        int p = pitchOf(d);
                        if (p < lo || p > hi) continue;
                        bool root = ((p - rq.root) % 12 + 12) % 12 == 0;
                        if (!root && !isChordTone(p, sg.chord)) continue;
                        int dist = std::abs(d - newDeg) * 2 + (root ? 0 : 1);
                        if (dist < bestDist) { bestDist = dist; best = d; }
                    }
                    newDeg = best;
                } else if (strong && !isChordTone(pitchOf(newDeg), sg.chord)) {
                    // pull strong beats onto the nearest chord tone that exists in the melody scale
                    int best = newDeg, bestDist = 99;
                    for (int d = newDeg - 3; d <= newDeg + 3; ++d) {
                        int p = pitchOf(d);
                        if (p < lo || p > hi || !isChordTone(p, sg.chord)) continue;
                        int dist = std::abs(d - newDeg);
                        if (dist < bestDist) { bestDist = dist; best = d; }
                    }
                    if (bestDist < 99) newDeg = best;
                }

                Note n;
                n.pitch = sc.snap(pitchOf(newDeg)); // belt and braces: snapping is the identity here
                n.start = st;
                n.length = std::max(0.9 * kTicksPerStep, rn.len * kTicksPerStep * 0.9);
                n.velocity = std::min(120, rn.vel + (strong ? 8 : -4) + rng.range(-3, 3));
                if (isFinal) n.length = std::min(n.length * 1.6, (double) kTicksPerBar - rn.step * kTicksPerStep);
                n.channel = kMelody;
                res.notes.push_back(n);
                thisPhrase.push_back({rn.step, rn.len, rn.vel, newDeg - degree});
                degree = newDeg;
            }
            if (bar % 4 == 0 || !repeatA) { if (bar % 4 == 0) phraseA = thisPhrase; }
        }
    }

    // ---- 808 / bass ------------------------------------------------------------------------------
    if (rq.bass) {
        for (const auto& sg : segs) {
            auto bassPitch = [&](int pc) { int p = 24 + pc; while (p < 28) p += 12; while (p > 39) p -= 12; return p; };
            int root = bassPitch(sg.chord.rootPc);
            const Rhythm* r = pickRhythm(1, 2.0 + 2.0 * cx);
            double firstLen = (r && r->notes.size() > 3 ? 4.0 : 8.0) * kTicksPerStep;
            for (int b = 0; b * kTicksPerBar < sg.lenTicks; ++b) {
                double base = (double) sg.startTick + b * kTicksPerBar;
                Note n; n.pitch = root; n.start = base; n.length = std::min(firstLen, (double) sg.lenTicks) * 0.95;
                n.velocity = 112; n.channel = kBass;
                res.notes.push_back(n);
                if (!r) continue;
                for (const auto& rn : r->notes) {
                    if (rn.step < 4.0 || !rng.chance(0.25 + 0.4 * cx)) continue;
                    double st = base + rn.step * kTicksPerStep;
                    if (st >= sg.startTick + sg.lenTicks - 60) continue;
                    int pc = sg.chord.rootPc;
                    // occasionally the chord's fifth
                    if (rng.chance(0.25) && sg.chord.pitches.size() > 2) pc = ((sg.chord.pitches[2] % 12) + 12) % 12;
                    Note m; m.pitch = bassPitch(pc); m.start = st;
                    m.length = std::min(rn.len * kTicksPerStep * 0.9, base + kTicksPerBar - st);
                    m.velocity = 96 + rng.range(-6, 6); m.channel = kBass;
                    res.notes.push_back(m);
                }
            }
        }
    }

    // ---- swing, humanize, velocity -------------------------------------------------------------------
    const double msToTicks = rq.bpm * kPPQ / 60000.0;
    for (auto& n : res.notes) {
        if (n.channel != kChords && rq.swing > 0) {
            double inStep = std::fmod(n.start, 2.0 * kTicksPerStep);
            if (std::fabs(inStep - kTicksPerStep) < 1.0) n.start += rq.swing * kTicksPerStep * 0.55;
        }
        if (rq.humanize > 0) {
            n.start += rng.bipolar() * rq.humanize * 14.0 * msToTicks;
            n.length = std::max(30.0, n.length * (1.0 + 0.12 * rq.humanize * rng.bipolar()));
            n.velocity += (int) std::lround(rng.bipolar() * rq.humanize * 10.0);
        }
        n.velocity += (int) std::lround(rng.bipolar() * rq.velocityVariation * 18.0);
        n.velocity = std::max(1, std::min(127, n.velocity));
        if (n.start < 0) n.start = 0;
    }

    // ---- final scale enforcement (should be a no-op) + overlap clean-up -----------------------------------
    for (auto& n : res.notes)
        n.pitch = n.channel == kMelody ? sc.snap(n.pitch) : sc.snapChord(n.pitch);

    std::sort(res.notes.begin(), res.notes.end(), [](const Note& a, const Note& b) {
        if (a.channel != b.channel) return a.channel < b.channel;
        if (a.pitch != b.pitch) return a.pitch < b.pitch;
        return a.start < b.start;
    });
    for (size_t i = 0; i + 1 < res.notes.size(); ++i) {
        Note& a = res.notes[i]; const Note& b = res.notes[i + 1];
        if (a.channel == b.channel && a.pitch == b.pitch && a.start + a.length > b.start - 1)
            a.length = std::max(10.0, b.start - 2 - a.start);
    }
    // monophonic melody: a note ends before the next begins
    std::sort(res.notes.begin(), res.notes.end(), [](const Note& a, const Note& b) {
        if (a.start != b.start) return a.start < b.start;
        return a.channel < b.channel;
    });
    {
        int prev = -1;
        for (size_t i = 0; i < res.notes.size(); ++i) {
            if (res.notes[i].channel != kMelody) continue;
            if (prev >= 0) {
                Note& p = res.notes[(size_t) prev];
                if (p.start + p.length > res.notes[i].start - 4) p.length = std::max(20.0, res.notes[i].start - 4 - p.start);
            }
            prev = (int) i;
        }
    }
    const double endTick = (double) total + 4 * kTicksPerStep;
    for (auto& n : res.notes) if (n.start + n.length > endTick) n.length = std::max(10.0, endTick - n.start);

    std::string d = std::string(noteName(rq.root)) + " " + sc.def().label + ", " + std::to_string(rq.bpm) + " BPM, ";
    for (size_t i = 0; i < res.chordNames.size() && i < 8; ++i) d += (i ? " | " : "") + res.chordNames[i];
    res.description = d;
    return res;
}

} // namespace hhmg
