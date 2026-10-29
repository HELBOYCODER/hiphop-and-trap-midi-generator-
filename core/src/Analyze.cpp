#include "hhmg/Analyze.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace hhmg {
namespace {

double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

} // namespace

Analysis analyzeNotes(const std::vector<Note>& chords, const std::vector<Note>& melody, double bpm, const PatternLibrary& lib, bool bpmKnown) {
    Analysis a;
    a.bpm = bpm;
    a.bpmKnown = bpmKnown;
    a.chordNotes = (int) chords.size();
    a.melodyNotes = (int) melody.size();
    std::vector<Note> all = chords;
    all.insert(all.end(), melody.begin(), melody.end());
    if (all.empty()) return a;

    double endTick = 0;
    for (const auto& n : all) endTick = std::max(endTick, n.start + n.length);
    a.bars = std::max(1, std::min(64, (int) std::ceil(endTick / kTicksPerBar - 0.02)));

    // ---- key & mode -------------------------------------------------------------------------------
    // Pitch content alone cannot separate a mode from its relative major/minor, so besides how well each
    // candidate scale explains the notes we weigh where the music rests: the bass note of every chord onset
    // and the very first chord.
    double hist[12] = {}, total = 0;
    for (const auto& n : all) { hist[((n.pitch % 12) + 12) % 12] += n.length; total += n.length; }
    const std::vector<Note>& src = chords.empty() ? all : chords;
    double bassHist[12] = {}, bassTotal = 0;
    int firstBass = -1;
    {
        std::map<long long, std::pair<int, double>> groups; // onset bucket -> lowest pitch, length
        for (const auto& n : src) {
            long long key = (long long) (n.start / (kPPQ / 8));
            auto it = groups.find(key);
            if (it == groups.end() || n.pitch < it->second.first) groups[key] = {n.pitch, n.length};
        }
        for (const auto& kv : groups) {
            int pc = ((kv.second.first % 12) + 12) % 12;
            if (firstBass < 0) firstBass = pc;
            bassHist[pc] += kv.second.second; bassTotal += kv.second.second;
        }
    }
    struct Cand { const char* id; double prior; };
    static const Cand cands[] = {{"minor", 0.4}, {"major", 0.4}, {"dorian", 0.15}, {"mixolydian", 0.1}, {"phrygian", 0.1},
                                 {"harmonic_minor", 0.1}, {"lydian", 0.05}, {"phrygian_dominant", 0.0}};
    double bestKeyScore = -1e9; int bRoot = 0, bScale = findScale("minor");
    for (int r = 0; r < 12; ++r)
        for (const auto& c : cands) {
            Scale sc{r, findScale(c.id)};
            double in = 0;
            for (int pc = 0; pc < 12; ++pc) if (sc.contains(r + pc)) in += hist[(r + pc) % 12];
            double fit = total > 0 ? in / total : 0;
            double bass = bassTotal > 0 ? bassHist[r] / bassTotal : 0;
            double tonicShare = total > 0 ? hist[r] / total : 0;
            double score = 20 * fit + 3.0 * bass + 1.5 * tonicShare + (firstBass == r ? 1.5 : 0) + c.prior;
            if (score > bestKeyScore) { bestKeyScore = score; bRoot = r; bScale = sc.index; }
        }
    a.root = bRoot; a.scale = bScale;
    const Scale scale{a.root, a.scale};

    // ---- chord degrees per bar & harmonic richness ------------------------------------------------
    const std::vector<Note>& harm = chords.empty() ? all : chords;
    const auto& cp = scale.def().chordParent;
    int richBars = 0, countedBars = 0;
    for (int b = 0; b < a.bars; ++b) {
        double w[12] = {};
        double lo = (double) b * kTicksPerBar, hi = lo + kTicksPerBar;
        for (const auto& n : harm) {
            double s = std::max(lo, n.start), e = std::min(hi, n.start + n.length);
            if (e > s) w[((n.pitch - a.root) % 12 + 12) % 12] += e - s;
        }
        double bestScore = 0; int bestDeg = 0; double ext = 0, tri = 0;
        for (int d = 0; d < 7; ++d) {
            int t0 = cp[(size_t) d], t1 = cp[(size_t) ((d + 2) % 7)], t2 = cp[(size_t) ((d + 4) % 7)];
            double sc = w[t0] + w[t1] + w[t2];
            for (int i = 0; i < 12; ++i) if (i != t0 && i != t1 && i != t2) sc -= 0.5 * w[i];
            if (sc > bestScore) { bestScore = sc; bestDeg = d + 1; tri = w[t0] + w[t1] + w[t2]; ext = w[cp[(size_t) ((d + 6) % 7)]] + w[cp[(size_t) ((d + 1) % 7)]]; }
        }
        if (bestDeg) { a.degrees.push_back(bestDeg); ++countedBars; if (tri > 0 && ext > 0.12 * tri) ++richBars; }
    }
    a.extensionRatio = countedBars ? (double) richBars / countedBars : 0;

    // ---- rhythm feel ---------------------------------------------------------------------------------
    const std::vector<Note>& lead = melody.empty() ? all : melody;
    double swingSum = 0; int swingN = 0, offGrid = 0; double devTicks = 0;
    std::set<long long> occupied;
    for (const auto& n : lead) {
        double step = n.start / kTicksPerStep;
        double nearest = std::round(step), dev = step - nearest;
        devTicks += std::fabs(dev) * kTicksPerStep; ++offGrid;
        if (((long long) nearest) % 2 != 0 && std::fabs(dev) < 0.45) { swingSum += std::max(0.0, dev); ++swingN; }
        for (long long g = (long long) (n.start / kTicksPerStep); g <= (long long) ((n.start + std::max(n.length - 1, 1.0)) / kTicksPerStep); ++g) occupied.insert(g);
    }
    a.swing = swingN ? clamp01((swingSum / swingN) / 0.55) : 0;
    const double msPerTick = 60000.0 / (std::max(1.0, bpm) * kPPQ);
    a.humanize = offGrid ? clamp01((devTicks / offGrid) * msPerTick / 14.0) : 0;
    a.density = (double) lead.size() / a.bars;
    a.restRatio = clamp01(1.0 - (double) occupied.size() / (double) (a.bars * 16));
    int same = 0, pairs = 0;
    for (size_t i = 1; i < lead.size(); ++i) { ++pairs; if (lead[i].pitch == lead[i - 1].pitch) ++same; }
    a.repeatRatio = pairs ? (double) same / pairs : 0;
    {   // one representative bar of onsets: the bar with the most notes
        std::map<int, std::vector<RhythmNote>> bars;
        for (const auto& n : lead) {
            int b = (int) (n.start / kTicksPerBar);
            bars[b].push_back({std::round(std::fmod(n.start, (double) kTicksPerBar) / kTicksPerStep), std::max(1.0, std::round(n.length / kTicksPerStep)), n.velocity});
        }
        size_t bestN = 0;
        for (auto& kv : bars) if (kv.second.size() > bestN && kv.second.size() <= 12) { bestN = kv.second.size(); a.rhythm = kv.second; }
    }
    double velMean = 0, velVar = 0;
    for (const auto& n : all) velMean += n.velocity;
    velMean /= (double) all.size();
    for (const auto& n : all) velVar += (n.velocity - velMean) * (n.velocity - velMean);
    double velStd = std::sqrt(velVar / (double) all.size());

    // ---- style scores ---------------------------------------------------------------------------------
    double rnb = 0, trap = 0, hip = 0;
    auto why = [&](const std::string& s) { a.reasons.push_back(s); };
    const std::string sid = scale.def().id;
    char buf[160];
    if (!bpmKnown) { why("the file has no tempo, so tempo was not used to judge style"); }
    else if (bpm >= 128 && bpm <= 165) { trap += 2; hip += 0.3; std::snprintf(buf, sizeof buf, "tempo %.0f BPM is in the trap range (128-165)", bpm); why(buf); }
    else if (bpm >= 64 && bpm < 82) { trap += 1.2; rnb += 1.5; hip += 0.5; std::snprintf(buf, sizeof buf, "tempo %.0f BPM is slow: R&B or half-time trap", bpm); why(buf); }
    else if (bpm >= 82 && bpm < 102) { hip += 2; rnb += 0.8; std::snprintf(buf, sizeof buf, "tempo %.0f BPM is the boom-bap / hip-hop pocket", bpm); why(buf); }
    else if (bpm >= 102 && bpm < 128) { hip += 0.8; rnb += 0.5; }
    if (a.swing > 0.25) { hip += 1.5; rnb += 1; why("off-beat swing detected (hip-hop / R&B feel)"); }
    else if (a.swing < 0.1) { trap += 0.8; }
    if (a.extensionRatio >= 0.5) { rnb += 2; why("many 7th/9th chords: lush R&B harmony"); }
    else if (a.extensionRatio >= 0.25) { rnb += 0.8; hip += 0.3; }
    else { trap += 1; hip += 1; why("mostly plain triads: lean trap / hip-hop harmony"); }
    if (sid == "minor" || sid == "phrygian" || sid == "harmonic_minor" || sid == "phrygian_dominant") { trap += 1.5; why("dark minor-family scale (" + sid + ")"); }
    else if (sid == "dorian") { rnb += 1; hip += 1; why("dorian: smooth minor colour"); }
    else { rnb += 1; hip += 0.5; }
    if (a.repeatRatio >= 0.3 && a.restRatio >= 0.25) { trap += 1.5; why("repeated notes with lots of space: trap hook"); }
    if (a.density < 3.5) { trap += 0.7; hip += 0.5; } else if (a.density > 7) { rnb += 0.8; why("busy melodic line: R&B-style runs"); }
    if (a.humanize >= 0.5) { rnb += 1; hip += 1; why("loose, human timing"); } else if (a.humanize <= 0.15) { trap += 1; }

    // similarity of the melody rhythm to learned rhythms of each style (Jaccard on onset steps)
    if (!a.rhythm.empty()) {
        std::set<int> mine; for (const auto& n : a.rhythm) mine.insert((int) n.step);
        double bestJ[3] = {0, 0, 0};
        for (const auto& r : lib.rhythms) {
            std::set<int> o; for (const auto& n : r.notes) o.insert((int) n.step);
            int inter = 0; for (int v : mine) if (o.count(v)) ++inter;
            double j = (double) inter / (double) (mine.size() + o.size() - inter);
            int k = r.style == "rnb" ? 0 : r.style == "trap" ? 1 : 2;
            bestJ[k] = std::max(bestJ[k], j);
        }
        rnb += 2 * bestJ[0]; trap += 2 * bestJ[1]; hip += 2 * bestJ[2];
        if (std::max({bestJ[0], bestJ[1], bestJ[2]}) > 0.6) why("melody rhythm closely matches learned library patterns");
    }
    double mn = std::min({rnb, trap, hip});
    double s0 = rnb - mn + 0.05, s1 = trap - mn + 0.05, s2 = hip - mn + 0.05, sum = s0 + s1 + s2;
    a.style = {s0 / sum, s1 / sum, s2 / sum};
    std::vector<std::pair<double, std::string>> ranked = {{a.style.rnb, "rnb"}, {a.style.trap, "trap"}, {a.style.hiphop, "hiphop"}};
    std::sort(ranked.rbegin(), ranked.rend());
    a.topStyle = ranked[0].second;
    a.confidence = ranked[0].first - ranked[1].first;

    // ---- settings that reproduce this feel -------------------------------------------------------------
    Request r;
    r.root = a.root; r.scale = a.scale;
    r.bpm = (int) std::lround(std::max(40.0, std::min(220.0, bpm)));
    if (!bpmKnown) r.bpm = a.topStyle == "trap" ? 140 : a.topStyle == "hiphop" ? 90 : 76;
    r.bars = a.bars >= 16 ? 16 : a.bars >= 8 ? 8 : a.bars >= 4 ? 4 : a.bars >= 2 ? 2 : 1;
    double top = std::max({a.style.rnb, a.style.trap, a.style.hiphop});
    r.mix = {a.style.rnb / top, a.style.trap / top, a.style.hiphop / top};
    r.complexity = clamp01(0.15 + 0.8 * a.extensionRatio + (a.density > 6 ? 0.1 : 0));
    r.humanize = clamp01(a.humanize);
    r.swing = clamp01(a.swing);
    r.velocityVariation = clamp01(velStd / 30.0);
    r.chords = a.chordNotes > 0; r.melody = a.melodyNotes > 0;
    r.bass = a.topStyle != "rnb";
    for (size_t i = 0; i < a.degrees.size() && i < 8; ++i) r.progression.push_back(a.degrees[i]);
    r.hookRhythm = a.rhythm;
    a.suggested = r;
    return a;
}

Analysis analyzeMidi(const MidiData& midi, const PatternLibrary& lib) {
    std::vector<Note> chords, melody, other;
    const int tol = kPPQ / 8;
    for (const auto& t : midi.tracks) {
        std::map<long long, int> onsets;
        double meanPitch = 0;
        for (const auto& n : t.notes) { onsets[(long long) (n.start / tol)]++; meanPitch += n.pitch; }
        meanPitch /= (double) t.notes.size();
        double poly = (double) t.notes.size() / (double) onsets.size();
        for (auto n : t.notes) {
            if (poly >= 2.3) { n.channel = kChords; chords.push_back(n); }
            else if (poly < 1.4 && meanPitch >= 52) { n.channel = kMelody; melody.push_back(n); }
            else other.push_back(n);
        }
    }
    if (chords.empty() && melody.empty()) melody = other; // single odd track: treat as the lead
    std::sort(melody.begin(), melody.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return analyzeNotes(chords, melody, midi.bpm, lib, midi.hasTempo);
}

Json analysisToJson(const Analysis& a) {
    Json o = Json::object();
    o.set("key", std::string(noteName(a.root)) + " " + allScales()[(size_t) a.scale].id);
    if (a.bpmKnown) o.set("bpm", std::round(a.bpm * 10) / 10); else o.set("bpm", "unknown (no tempo in file)");
    o.set("bars", a.bars);
    Json dg = Json::array(); for (int d : a.degrees) dg.push(d); o.set("chord_degrees_per_bar", dg);
    Json st = Json::object();
    st.set("rnb", std::round(a.style.rnb * 100) / 100); st.set("trap", std::round(a.style.trap * 100) / 100); st.set("hiphop", std::round(a.style.hiphop * 100) / 100);
    o.set("style_scores", st);
    o.set("detected_style", a.topStyle);
    o.set("style_confidence", std::round(a.confidence * 100) / 100);
    Json rs = Json::array(); for (const auto& r : a.reasons) rs.push(r); o.set("reasons", rs);
    Json m = Json::object();
    m.set("chord_extension_ratio", std::round(a.extensionRatio * 100) / 100); m.set("melody_notes_per_bar", std::round(a.density * 10) / 10);
    m.set("swing", std::round(a.swing * 100) / 100); m.set("looseness", std::round(a.humanize * 100) / 100);
    m.set("repeat_ratio", std::round(a.repeatRatio * 100) / 100); m.set("space_ratio", std::round(a.restRatio * 100) / 100);
    m.set("chord_notes", a.chordNotes); m.set("melody_notes", a.melodyNotes);
    o.set("measures", m);
    Json sg = Json::object();
    const Request& r = a.suggested;
    sg.set("root", noteName(r.root)); sg.set("scale", allScales()[(size_t) r.scale].id); sg.set("bpm", r.bpm); sg.set("bars", r.bars);
    Json mix = Json::object(); mix.set("rnb", std::round(r.mix.rnb * 100) / 100); mix.set("trap", std::round(r.mix.trap * 100) / 100); mix.set("hiphop", std::round(r.mix.hiphop * 100) / 100);
    sg.set("style_mix", mix);
    sg.set("complexity", std::round(r.complexity * 100) / 100); sg.set("humanize", std::round(r.humanize * 100) / 100);
    sg.set("swing", std::round(r.swing * 100) / 100); sg.set("velocity_variation", std::round(r.velocityVariation * 100) / 100);
    sg.set("chords", r.chords); sg.set("melody", r.melody); sg.set("bass", r.bass);
    Json pr = Json::array(); for (int d : r.progression) pr.push(d); sg.set("progression", pr);
    o.set("suggested_generate_midi_settings", sg);
    return o;
}

} // namespace hhmg
