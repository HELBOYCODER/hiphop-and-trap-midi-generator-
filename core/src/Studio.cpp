#include "hhmg/Studio.h"
#include "hhmg/MidiFile.h"
#include <algorithm>
#include <cmath>
#include <map>
#include "hhmg/Tools.h"

namespace hhmg {

Json clipToJson(const std::vector<Note>& notes, int bpm, int bars, const std::string& description) {
    Json c = Json::object();
    c.set("bpm", bpm); c.set("bars", bars); c.set("description", description);
    Json arr = Json::array();
    for (const auto& n : notes) {
        Json x = Json::array();
        x.push(n.pitch); x.push(std::round(n.start)); x.push(std::round(n.length)); x.push(n.velocity); x.push(n.channel);
        arr.push(x);
    }
    c.set("notes", arr);
    return c;
}

bool clipFromJson(const Json& clip, std::vector<Note>& notes, int& bpm, int& bars) {
    if (!clip.isObject()) return false;
    bpm = clip["bpm"].integer(120); bars = clip["bars"].integer(4);
    notes.clear();
    for (const auto& x : clip["notes"].items()) {
        if (x.size() < 5) continue;
        Note n; n.pitch = x.at(0).integer(); n.start = x.at(1).num(); n.length = x.at(2).num(); n.velocity = x.at(3).integer(); n.channel = std::max(0, std::min(2, x.at(4).integer()));
        notes.push_back(n);
    }
    return !notes.empty();
}

std::vector<uint8_t> clipToMidi(const Json& clip) {
    Result r;
    std::vector<Note> notes; int bpm = 120, bars = 4;
    if (!clipFromJson(clip, notes, bpm, bars)) return {};
    r.request.bpm = std::max(40, std::min(220, bpm));
    r.request.bars = std::max(1, std::min(64, bars));
    r.notes = notes;
    r.description = clip["description"].str("HipHop MIDI Studio");
    return writeMidi(r);
}

Json transcriptionToJson(const Transcription& t) {
    Json o = Json::object();
    o.set("ok", t.error.empty());
    if (!t.error.empty()) { o.set("error", t.error); return o; }
    o.set("bpm", t.bpm); o.set("bpm_estimated", t.bpmEstimated); o.set("duration_sec", std::round(t.durationSec * 10) / 10); o.set("start_offset_sec", std::round(t.startOffsetSec * 1000) / 1000);
    Json c = Json::array();
    for (const auto& f : t.contour) {   // [timeSec, midi(-1 = silent)]
        Json p = Json::array(); p.push(std::round(f.time * 1000) / 1000); p.push(f.voiced ? std::round(f.midi * 100) / 100 : -1.0); c.push(p);
    }
    o.set("contour", c);
    Json n = Json::array();
    for (const auto& x : t.notes) { Json a = Json::array(); a.push(x.pitch); a.push(std::round(x.start)); a.push(std::round(x.length)); a.push(x.velocity); a.push(x.channel); n.push(a); }
    o.set("notes", n);
    return o;
}

namespace {
std::vector<int> loopOf(std::vector<int> d) {
    for (size_t period : {1u, 2u, 4u, 8u}) {
        if (d.size() < period * 2) break;
        bool ok = true;
        for (size_t i = period; i < d.size() && ok; ++i) ok = d[i] == d[i % period];
        if (ok) { d.resize(period); break; }
    }
    if (d.size() > 8) d.resize(8);
    return d;
}

// The user's dataset: pick a real reference close to what was sung (same mode and style, near tempo/key).
const Reference* pickReference(const Analysis& a, const PatternLibrary& lib, uint32_t seed) {
    const bool minor = Scale{a.root, a.scale}.isMinorish();
    std::vector<std::pair<double, const Reference*>> ranked;
    for (const auto& r : lib.references) {
        if (r.degrees.size() < 2) continue;
        double sc = 0;
        if ((r.mode == "minor") == minor) sc += 3;
        if (r.style == a.topStyle) sc += 2;
        if (r.root == a.root) sc += 1;
        if (r.bpm > 0 && a.bpmKnown) sc += std::max(0.0, 1.5 - std::fabs(r.bpm - a.bpm) / 20.0);
        if (r.degrees.size() >= 4) sc += 0.5;
        ranked.push_back({sc, &r});
    }
    if (ranked.empty()) return nullptr;
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    size_t top = std::min<size_t>(4, ranked.size());
    return ranked[seed % top].second;     // different seeds walk through the best matches
}
} // namespace

static int barsFor(const std::vector<Note>& notes) {
    double end = 0; for (const auto& n : notes) end = std::max(end, n.start + n.length);
    return std::max(1, std::min(32, (int) std::ceil(end / kTicksPerBar - 0.02)));
}

Json studioProcess(const std::string& mode, const std::vector<Note>& melodyIn, double bpm, bool bpmKnown,
                   const Json& options, const PatternLibrary& lib) {
    Json out = Json::object();
    if (melodyIn.empty()) { out.set("ok", false); out.set("error", "no notes"); return out; }
    std::vector<Note> melody = melodyIn;
    for (auto& n : melody) n.channel = kMelody;

    Analysis a = analyzeNotes({}, melody, bpm, lib, bpmKnown);
    const Scale scale{a.root, a.scale};
    const bool lock = options["scale_lock"].boolean(true);
    if (lock) for (auto& n : melody) n.pitch = scale.snap(n.pitch);   // never a wrong note, even if the singer was off

    Request rq = a.suggested;
    rq.seed = (uint32_t) options["seed"].num(1);
    if (bpmKnown || options["keep_bpm"].boolean(false)) rq.bpm = (int) std::lround(bpm);   // else: a tempo typical of the detected style
    rq.bars = barsFor(melody);
    if (options["complexity"].isNumber()) rq.complexity = options["complexity"].num();
    const std::string style = options["style"].str("");
    if (style == "rnb") rq.mix = {1, 0.1, 0.1}; else if (style == "trap") rq.mix = {0.1, 1, 0.3}; else if (style == "hiphop") rq.mix = {0.1, 0.2, 1};
    rq.bass = options.has("bass") ? options["bass"].boolean(true) : (style != "rnb");

    std::vector<Note> notes;
    std::string desc;
    if (mode == "melody") {
        notes = melody;
        desc = "Your melody";
    } else if (mode == "melody_plus") {
        rq.melody = false; rq.chords = true;
        Result r = generate(rq, lib);
        notes = r.notes;
        notes.insert(notes.end(), melody.begin(), melody.end());
        desc = "Your melody + " + r.description;
        Json ch = Json::array(); for (const auto& c : r.chordNames) ch.push(c);
        out.set("chords", ch);
    } else { // similar: same key, tempo, feel and rhythm, new notes - drawing on the dataset when it has a close reference
        rq.chords = true; rq.melody = true;
        rq.hookRhythm = a.rhythm;
        if (options["use_dataset"].boolean(true)) {
            if (const Reference* ref = pickReference(a, lib, rq.seed)) {
                auto loop = loopOf(ref->degrees);
                if (loop.size() >= 2) rq.progression = loop;
                if (rq.hookRhythm.size() < 3 && !ref->rhythm.empty()) rq.hookRhythm = ref->rhythm;
                Json in = Json::object();
                in.set("id", ref->id); in.set("name", ref->name); in.set("pack", ref->pack);
                in.set("key", std::string(noteName(ref->root)) + " " + ref->mode);
                out.set("inspired_by", in);
            }
        }
        Result r = generate(rq, lib);
        notes = r.notes;
        desc = "Similar idea: " + r.description;
        Json ch = Json::array(); for (const auto& c : r.chordNames) ch.push(c);
        out.set("chords", ch);
    }
    out.set("ok", true);
    out.set("root", a.root); out.set("scale_index", a.scale);
    out.set("clip", clipToJson(notes, rq.bpm, rq.bars, desc));
    out.set("analysis", analysisToJson(a));
    return out;
}

int countSyllables(const std::string& word) {
    // Latin: vowel groups with a silent trailing 'e'; other scripts (Persian/Arabic): vowel letters + long vowels
    std::string w; bool latin = true; int nonAscii = 0;
    for (unsigned char c : word) { if (c >= 0x80) { latin = false; ++nonAscii; } else if (std::isalpha(c)) w += (char) std::tolower(c); }
    if (latin) {
        if (w.empty()) return 0;
        int n = 0; bool prev = false;
        for (char c : w) { bool v = std::string("aeiouy").find(c) != std::string::npos; if (v && !prev) ++n; prev = v; }
        if (w.size() > 2 && w.back() == 'e' && w[w.size() - 2] != 'l' && n > 1) --n;           // "make" -> 1, "table" keeps 2
        if (w.size() > 2 && w.compare(w.size() - 2, 2, "ed") == 0 && n > 1 && w[w.size() - 3] != 't' && w[w.size() - 3] != 'd') --n;
        return std::max(1, n);
    }
    // Arabic script (Persian): long-vowel letters (ا آ و ی ى) and a final ه are vowels; two consonants in a row imply a short
    // vowel between them, so "سلام" = sa-lām = 2, "شب" = 1, "دلم" = 2. Approximate, good enough for placing notes.
    std::vector<int> cps;                                           // two-byte code units as (lead << 8 | trail)
    for (size_t i = 0; i + 1 < word.size(); ++i) {
        unsigned char a = (unsigned char) word[i], b = (unsigned char) word[i + 1];
        if (a == 0xD8 || a == 0xD9 || a == 0xDB) { cps.push_back((a << 8) | b); ++i; } else if (a >= 0x80) ++i;
    }
    if (cps.empty()) return 1;
    auto isVowel = [&](size_t k) {
        int c = cps[k];
        if (c == 0xD8A7 || c == 0xD8A2 || c == 0xD988 || c == 0xDB8C || c == 0xD989) return true;
        return k + 1 == cps.size() && (c == 0xD987 || c == 0xDB80);   // final ه
    };
    int n = 0; bool prevConsonant = false;
    for (size_t k = 0; k < cps.size(); ++k) {
        if (isVowel(k)) { ++n; prevConsonant = false; }
        else { if (prevConsonant) ++n; prevConsonant = true; }
    }
    return std::max(1, n);
}

Json lyricsToClip(const std::string& lyrics, const Json& options, const PatternLibrary& lib) {
    Json out = Json::object();
    struct Word { int syl; };
    struct Line { std::string text; std::vector<std::pair<int, bool>> syls; }; // (word index, stressed)
    std::vector<Line> lines;
    {
        std::string cur; std::vector<std::string> raw;
        for (char c : lyrics + "\n") { if (c == '\n' || c == '\r') { if (!cur.empty()) raw.push_back(cur); cur.clear(); } else cur += c; }
        for (const auto& t : raw) {
            Line l; l.text = t;
            std::string word; int wi = 0;
            auto flush = [&]() { if (word.empty()) return; int n = countSyllables(word); for (int k = 0; k < n; ++k) l.syls.push_back({wi, k == 0}); ++wi; word.clear(); };
            for (char c : t + " ") { if (c == ' ' || c == '\t' || c == ',' || c == ';' || c == '.' || c == '!' || c == '?' || c == '-') flush(); else word += c; }
            if (!l.syls.empty()) lines.push_back(l);
        }
    }
    if (lines.empty()) { out.set("ok", false); out.set("error", "no lyrics"); return out; }
    if (lines.size() > 32) lines.resize(32);

    // bars per line: 1 normally, 2 when there are many syllables
    std::vector<int> barsOf; int bars = 0;
    for (const auto& l : lines) { int b = l.syls.size() > 16 ? 2 : 1; barsOf.push_back(b); bars += b; }
    bars = std::min(bars, 32);

    Request rq = requestFromJson(options);
    if (!options.has("style")) rq.mix = {0.2, 1.0, 0.4};
    rq.bars = bars; rq.melody = false; rq.chords = true;
    if (!options.has("bass")) rq.bass = rq.mix.trap >= rq.mix.rnb;
    Result r = generate(rq, lib);
    const Scale& sc = r.scale;

    // chord tones per bar from the generated chords
    auto chordPcsAt = [&](int bar) {
        std::vector<int> pcs;
        for (const auto& n : r.notes)
            if (n.channel == kChords && n.start >= bar * kTicksPerBar - 5 && n.start < (bar + 1) * kTicksPerBar) pcs.push_back(((n.pitch % 12) + 12) % 12);
        return pcs;
    };
    uint32_t state = rq.seed * 2654435761u + 12345;
    auto rnd = [&]() { state = state * 1664525u + 1013904223u; return (state >> 8) / 16777216.0; };

    const int tonic = 12 * 6 + sc.root;
    const int lo = tonic - 5, hi = tonic + 12;
    auto pitchOf = [&](int d) { return sc.degreeToPitch(tonic, d); };
    int deg = 0, bar = 0;
    Json lineInfo = Json::array();
    for (size_t li = 0; li < lines.size(); ++li) {
        const Line& l = lines[li];
        const int nb = barsOf[li];
        const int n = (int) l.syls.size();
        const int slots = nb * 16;
        // spacing: eighths for short lines, sixteenths for dense ones; the last syllable is held to the end of the line
        int unit = n * 2 <= slots - 4 ? 2 : 1;
        if (n * 4 <= slots - 6) unit = 4;                       // very short line: quarter notes
        int startStep = (n * unit < slots - 2) ? (rnd() < 0.5 ? 0 : 2) : 0;   // sometimes a short pickup rest
        auto pcs = chordPcsAt(std::min(bar, bars - 1));
        for (int i = 0; i < n; ++i) {
            const bool stressed = l.syls[(size_t) i].second, last = i == n - 1;
            const int barHere = bar + (startStep + i * unit) / 16;
            if (barHere < bars) pcs = chordPcsAt(barHere);
            int nd = deg;
            double r2 = rnd();
            if (stressed || last) {
                int best = deg, bestDist = 99;
                for (int d = deg - 3; d <= deg + 3; ++d) {
                    int p = pitchOf(d); if (p < lo || p > hi) continue;
                    bool ct = false; for (int pc : pcs) if (((p % 12) + 12) % 12 == pc) ct = true;
                    bool root = ((p - sc.root) % 12 + 12) % 12 == 0;
                    if (!(ct || (last && root))) continue;
                    int dist = std::abs(d - deg) * 2 + (last && !root ? 1 : 0) + (r2 < 0.3 ? (d == deg ? 2 : 0) : 0);
                    if (dist < bestDist) { bestDist = dist; best = d; }
                }
                nd = best;
            } else {
                nd = deg + (r2 < 0.35 ? 0 : r2 < 0.7 ? 1 : -1);        // weak syllables: repeat or step
            }
            while (pitchOf(nd) > hi) nd -= (int) sc.def().intervals.size();
            while (pitchOf(nd) < lo) nd += (int) sc.def().intervals.size();
            deg = nd;
            Note note; note.pitch = sc.snap(pitchOf(nd)); note.channel = kMelody;
            note.start = (double) (bar * 16 + startStep + i * unit) * kTicksPerStep;
            double len = unit * kTicksPerStep * 0.9;
            if (last) len = ((double) (bar + nb) * kTicksPerBar) - note.start - kTicksPerStep;
            note.length = std::max(60.0, std::min(len, 2.0 * kTicksPerBar));
            note.velocity = stressed ? 98 : 80;
            if (note.start < (double) bars * kTicksPerBar) r.notes.push_back(note);
        }
        Json li2 = Json::object(); li2.set("text", l.text); li2.set("syllables", n); li2.set("bars", nb); lineInfo.push(li2);
        bar += nb;
    }
    std::string desc = "Lyrics melody: " + r.description;
    out.set("ok", true);
    out.set("clip", clipToJson(r.notes, rq.bpm, bars, desc));
    out.set("lines", lineInfo);
    Json ch = Json::array(); for (const auto& c : r.chordNames) ch.push(c);
    out.set("chords", ch);
    out.set("root", sc.root); out.set("scale_index", rq.scale);
    return out;
}

Json similarToClip(const Json& clip, const Json& options, const PatternLibrary& lib) {
    std::vector<Note> notes; int bpm = 120, bars = 4;
    Json e = Json::object();
    if (!clipFromJson(clip, notes, bpm, bars)) { e.set("ok", false); e.set("error", "no clip loaded"); return e; }
    std::vector<Note> lead;
    for (const auto& n : notes) if (n.channel == kMelody) lead.push_back(n);
    if (lead.empty()) for (const auto& n : notes) if (n.channel != kBass) lead.push_back(n);
    if (lead.empty()) { e.set("ok", false); e.set("error", "the clip has no melody to be similar to"); return e; }
    std::sort(lead.begin(), lead.end(), [](const Note& x, const Note& y) { return x.start < y.start; });
    return studioProcess(options["mode"].str("similar"), lead, bpm, true, options, lib);
}

Json analyzeClip(const Json& clip, const PatternLibrary& lib) {
    std::vector<Note> notes; int bpm = 120, bars = 4;
    Json e = Json::object();
    if (!clipFromJson(clip, notes, bpm, bars)) { e.set("ok", false); e.set("error", "no clip loaded"); return e; }
    std::vector<Note> chords, melody;
    for (const auto& n : notes) (n.channel == kChords ? chords : melody).push_back(n);
    Json j = analysisToJson(analyzeNotes(chords, melody, bpm, lib, true));
    j.set("ok", true);
    return j;
}

} // namespace hhmg
