#include "hhmg/MidiRead.h"
#include <algorithm>
#include <fstream>
#include <map>

namespace hhmg {
namespace {
struct Cursor {
    const std::vector<uint8_t>& d; size_t p;
    bool has(size_t n) const { return p + n <= d.size(); }
};
bool vlq(Cursor& c, unsigned& v) {
    v = 0;
    for (int i = 0; i < 4; ++i) {
        if (!c.has(1)) return false;
        uint8_t b = c.d[c.p++];
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80)) return true;
    }
    return false;
}
unsigned be(const std::vector<uint8_t>& d, size_t p, int n) { unsigned v = 0; for (int i = 0; i < n; ++i) v = (v << 8) | d[p + (size_t) i]; return v; }
} // namespace

bool readMidi(const std::vector<uint8_t>& d, MidiData& out, std::string& error) {
    out = MidiData{};
    if (d.size() < 14 || d[0] != 'M' || d[1] != 'T' || d[2] != 'h' || d[3] != 'd') { error = "not a MIDI file"; return false; }
    unsigned hlen = be(d, 4, 4), ntrks = be(d, 10, 2), div = be(d, 12, 2);
    if (div & 0x8000) { error = "SMPTE time division not supported"; return false; }
    if (div == 0) { error = "bad time division"; return false; }
    const double k = (double) kPPQ / (double) div;
    size_t pos = 8 + hlen;
    bool haveTempo = false;
    for (unsigned t = 0; t < ntrks && pos + 8 <= d.size(); ++t) {
        unsigned len = be(d, pos + 4, 4);
        bool isTrk = d[pos] == 'M' && d[pos + 1] == 'T' && d[pos + 2] == 'r' && d[pos + 3] == 'k';
        size_t start = pos + 8, end = std::min(d.size(), start + len);
        pos = start + len;
        if (!isTrk) { --t; continue; }
        Cursor c{d, start};
        unsigned long long tick = 0;
        uint8_t status = 0;
        std::map<int, std::vector<std::pair<unsigned long long, int>>> active; // key=ch*128+note
        MidiTrackNotes tn;
        while (c.p < end) {
            unsigned delta;
            if (!vlq(c, delta)) { error = "bad delta time"; return false; }
            tick += delta;
            if (!c.has(1)) break;
            uint8_t b = d[c.p];
            if (b == 0xFF) {
                if (!c.has(2)) break;
                uint8_t type = d[c.p + 1]; c.p += 2;
                unsigned ml; if (!vlq(c, ml) || !c.has(ml)) { error = "bad meta event"; return false; }
                if (type == 0x51 && ml == 3 && !haveTempo) { out.bpm = 60000000.0 / (double) be(d, c.p, 3); haveTempo = true; out.hasTempo = true; }
                c.p += ml;
                continue;
            }
            if (b == 0xF0 || b == 0xF7) {
                ++c.p; unsigned ml; if (!vlq(c, ml) || !c.has(ml)) { error = "bad sysex"; return false; }
                c.p += ml; continue;
            }
            if (b & 0x80) { status = b; ++c.p; }
            if (!status) { error = "running status without status"; return false; }
            int kind = status & 0xF0, ch = status & 0x0F;
            if (kind == 0xC0 || kind == 0xD0) { if (!c.has(1)) break; ++c.p; continue; }
            if (!c.has(2)) break;
            int d1 = d[c.p], d2 = d[c.p + 1]; c.p += 2;
            if (kind == 0x90 && d2 > 0) active[ch * 128 + d1].push_back({tick, d2});
            else if (kind == 0x80 || (kind == 0x90 && d2 == 0)) {
                auto& st = active[ch * 128 + d1];
                if (!st.empty() && ch != 9) {
                    auto on = st.front(); st.erase(st.begin());
                    Note n; n.pitch = d1; n.start = (double) on.first * k;
                    n.length = std::max(1.0, (double) (tick - on.first) * k); n.velocity = on.second; n.channel = kMelody;
                    tn.notes.push_back(n);
                }
            }
        }
        std::sort(tn.notes.begin(), tn.notes.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
        if (!tn.notes.empty()) out.tracks.push_back(std::move(tn));
    }
    if (out.tracks.empty()) { error = "no notes found"; return false; }
    return true;
}

bool readMidiFile(const std::string& path, MidiData& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open " + path; return false; }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return readMidi(bytes, out, error);
}

} // namespace hhmg
