#include "hhmg/Transcribe.h"
#include <algorithm>
#include <cmath>

namespace hhmg {
namespace {

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[v.size() / 2];
}

// Cheap anti-alias decimation to roughly 16 kHz.
std::vector<float> toAnalysisRate(const std::vector<float>& x, int sr, int& outSr) {
    int factor = std::max(1, (int) std::lround(sr / 16000.0));
    outSr = sr / factor;
    if (factor == 1) return x;
    std::vector<float> y(x.size() / (size_t) factor);
    for (size_t i = 0; i < y.size(); ++i) {
        double a = 0;
        for (int k = 0; k < factor; ++k) a += x[i * (size_t) factor + (size_t) k];
        y[i] = (float) (a / factor);
    }
    return y;
}

} // namespace

Transcription transcribeAudio(const std::vector<float>& input, int sampleRate, const TranscribeOptions& opt) {
    Transcription out;
    if (sampleRate < 8000 || input.size() < (size_t) sampleRate / 2) { out.error = "recording too short"; return out; }
    int sr = sampleRate;
    std::vector<float> x = toAnalysisRate(input, sampleRate, sr);
    out.durationSec = (double) input.size() / sampleRate;

    // remove DC and normalise
    double mean = 0; for (float v : x) mean += v; mean /= (double) x.size();
    double peak = 1e-9; for (float& v : x) { v -= (float) mean; peak = std::max(peak, (double) std::fabs(v)); }
    if (peak < 0.005) { out.error = "no sound detected"; return out; }
    for (float& v : x) v = (float) (v / peak);

    const int N = 1024, W = N / 2, hop = 256;
    const int tauMin = std::max(2, sr / 1000), tauMax = std::min(W - 1, sr / 70);
    const double dt = (double) hop / sr;
    std::vector<double> rmsAll;
    std::vector<PitchFrame> frames;
    std::vector<double> d((size_t) tauMax + 2), cm((size_t) tauMax + 2);

    for (size_t start = 0; start + (size_t) N <= x.size(); start += (size_t) hop) {
        const float* f = &x[start];
        double e = 0; for (int i = 0; i < W; ++i) e += (double) f[i] * f[i];
        double rms = std::sqrt(e / W);
        rmsAll.push_back(rms);
        PitchFrame pf; pf.time = (double) start / sr + (double) N / 2 / sr;
        // difference function
        d[0] = 0;
        for (int tau = 1; tau <= tauMax; ++tau) {
            double s = 0;
            for (int i = 0; i < W; ++i) { double diff = (double) f[i] - f[i + tau]; s += diff * diff; }
            d[(size_t) tau] = s;
        }
        // cumulative mean normalised difference
        cm[0] = 1; double run = 0;
        for (int tau = 1; tau <= tauMax; ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = run > 0 ? d[(size_t) tau] * tau / run : 1; }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cm[(size_t) tau] < 0.18 && cm[(size_t) tau] <= cm[(size_t) tau - 1] && cm[(size_t) tau] <= cm[(size_t) tau + 1]) { best = tau; break; }
        if (best < 0) {
            double mn = 1e9; for (int tau = tauMin; tau < tauMax; ++tau) if (cm[(size_t) tau] < mn) { mn = cm[(size_t) tau]; best = tau; }
            if (mn > 0.30) best = -1;
        }
        if (best > 0) {
            double a = cm[(size_t) best - 1], b = cm[(size_t) best], c = cm[(size_t) best + 1];
            double denom = a - 2 * b + c;
            double tau = best + (std::fabs(denom) > 1e-12 ? 0.5 * (a - c) / denom : 0.0);
            double f0 = sr / tau;
            pf.midi = 69.0 + 12.0 * std::log2(f0 / 440.0);
            pf.confidence = 1.0 - std::min(1.0, cm[(size_t) best]);
            pf.voiced = f0 > 65 && f0 < 1100;
        }
        pf.confidence *= 1.0;
        frames.push_back(pf);
        (void) rms;
    }
    double maxRms = 0; for (double r : rmsAll) maxRms = std::max(maxRms, r);
    for (size_t i = 0; i < frames.size(); ++i)
        if (rmsAll[i] < std::max(0.02, 0.06 * maxRms)) frames[i].voiced = false;

    // 5-frame median smoothing of voiced pitch
    std::vector<double> sm(frames.size(), 0);
    for (size_t i = 0; i < frames.size(); ++i) {
        if (!frames[i].voiced) continue;
        std::vector<double> w;
        for (int k = -2; k <= 2; ++k) { long j = (long) i + k; if (j >= 0 && j < (long) frames.size() && frames[(size_t) j].voiced) w.push_back(frames[(size_t) j].midi); }
        sm[i] = median(w);
    }
    for (size_t i = 0; i < frames.size(); ++i) if (frames[i].voiced) frames[i].midi = sm[i];
    out.contour = frames;

    // segmentation
    struct Seg { double t0, t1; double pitch; double energy; };
    std::vector<Seg> segs;
    const size_t gapMax = 4;
    size_t i = 0;
    while (i < frames.size()) {
        if (!frames[i].voiced) { ++i; continue; }
        size_t j = i; std::vector<double> vals; double en = 0; size_t gap = 0, lastVoiced = i, diverge = 0;
        while (j < frames.size()) {
            if (frames[j].voiced) {
                double ref = median(vals);
                if (!vals.empty() && std::fabs(frames[j].midi - ref) > 0.9) { if (++diverge >= 3) break; }
                else { diverge = 0; vals.push_back(frames[j].midi); en += rmsAll[j]; }
                gap = 0; lastVoiced = j;
            } else if (++gap > gapMax) break;
            ++j;
        }
        size_t endFrame = diverge >= 3 ? lastVoiced - 1 : lastVoiced;   // diverging frames start the next note
        if (endFrame < i) endFrame = i;
        if (!vals.empty()) segs.push_back({frames[i].time, frames[endFrame].time + dt, median(vals), en / (double) vals.size()});
        i = std::max(endFrame + 1, i + 1);
    }
    // drop too-short notes, merge same-pitch neighbours with tiny gaps
    std::vector<Seg> kept;
    for (const auto& s : segs) if ((s.t1 - s.t0) * 1000 >= opt.minNoteMs) kept.push_back(s);
    std::vector<Seg> merged;
    for (const auto& s : kept) {
        if (!merged.empty() && std::lround(merged.back().pitch) == std::lround(s.pitch) && s.t0 - merged.back().t1 < 0.08) merged.back().t1 = s.t1;
        else merged.push_back(s);
    }
    if (merged.empty()) { out.error = "no clear pitched notes found - try humming or singing closer to the microphone"; return out; }

    // tempo
    double bpm = opt.bpm;
    out.bpmEstimated = bpm <= 0;
    if (bpm <= 0) {
        bpm = 100;
        if (merged.size() >= 4) {
            double bestScore = -1e9;
            for (double b = 70; b <= 170; b += 0.5) {
                double sixteenth = 60.0 / b / 4.0, sc = 0;
                for (const auto& s : merged) sc += std::cos(6.283185307179586 * (s.t0 - merged[0].t0) / sixteenth);
                sc += -std::fabs(b - 110) * 0.004 * (double) merged.size();      // mild prior toward common tempos
                if (sc > bestScore) { bestScore = sc; bpm = b; }
            }
        }
    }
    out.bpm = std::round(bpm * 10) / 10;

    // to ticks on the 16th grid; the first note starts at 0
    const double t0 = merged[0].t0;
    out.startOffsetSec = t0;
    const double ticksPerSec = out.bpm / 60.0 * kPPQ;
    double prevEnd = 0;
    for (const auto& s : merged) {
        Note n;
        n.pitch = (int) std::lround(s.pitch);
        double st = (s.t0 - t0) * ticksPerSec, len = (s.t1 - s.t0) * ticksPerSec;
        if (opt.quantize) {
            st = std::round(st / kTicksPerStep) * kTicksPerStep;
            len = std::max((double) kTicksPerStep, std::round(len / kTicksPerStep) * kTicksPerStep);
        }
        if (st < prevEnd) st = prevEnd;                       // monophonic and ordered
        n.start = st; n.length = std::max(30.0, len); prevEnd = st + n.length;
        n.velocity = std::max(50, std::min(120, (int) std::lround(55 + 110 * s.energy)));
        n.channel = kMelody;
        out.notes.push_back(n);
    }
    // shorten a note that now overlaps the next one's start (after quantising)
    for (size_t k = 0; k + 1 < out.notes.size(); ++k)
        if (out.notes[k].start + out.notes[k].length > out.notes[k + 1].start) out.notes[k].length = std::max(30.0, out.notes[k + 1].start - out.notes[k].start);
    return out;
}

} // namespace hhmg
