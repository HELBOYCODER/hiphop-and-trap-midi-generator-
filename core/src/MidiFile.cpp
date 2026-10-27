#include "hhmg/MidiFile.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace hhmg {
namespace {

void be16(std::vector<uint8_t>& o, unsigned v) { o.push_back((uint8_t) (v >> 8)); o.push_back((uint8_t) v); }
void be32(std::vector<uint8_t>& o, unsigned v) { be16(o, v >> 16); be16(o, v & 0xFFFF); }
void vlq(std::vector<uint8_t>& o, unsigned v) {
    uint8_t b[5]; int n = 0;
    b[n++] = v & 0x7F;
    while (v >>= 7) b[n++] = (uint8_t) ((v & 0x7F) | 0x80);
    while (n--) o.push_back(b[n]);
}
void meta(std::vector<uint8_t>& o, unsigned delta, uint8_t type, const std::vector<uint8_t>& data) {
    vlq(o, delta); o.push_back(0xFF); o.push_back(type); vlq(o, (unsigned) data.size());
    o.insert(o.end(), data.begin(), data.end());
}
std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }
void chunk(std::vector<uint8_t>& out, const std::vector<uint8_t>& trk) {
    out.insert(out.end(), {'M', 'T', 'r', 'k'});
    be32(out, (unsigned) trk.size());
    out.insert(out.end(), trk.begin(), trk.end());
}

struct Ev { long tick; int order; uint8_t status, d1, d2; };

} // namespace

std::vector<uint8_t> writeMidi(const Result& r) {
    std::vector<uint8_t> out;
    static const char* names[] = {"Chords", "Melody", "808 Bass"};
    std::vector<int> used;
    for (int ch = 0; ch < 3; ++ch)
        for (const auto& n : r.notes) if (n.channel == ch) { used.push_back(ch); break; }

    out.insert(out.end(), {'M', 'T', 'h', 'd'});
    be32(out, 6); be16(out, 1); be16(out, (unsigned) (1 + used.size())); be16(out, kPPQ);

    std::vector<uint8_t> t0;
    meta(t0, 0, 0x03, text("Tempo"));
    meta(t0, 0, 0x01, text(r.description));
    unsigned mpqn = (unsigned) std::lround(60000000.0 / r.request.bpm);
    meta(t0, 0, 0x51, {(uint8_t) (mpqn >> 16), (uint8_t) (mpqn >> 8), (uint8_t) mpqn});
    meta(t0, 0, 0x58, {4, 2, 24, 8});
    meta(t0, 0, 0x2F, {});
    chunk(out, t0);

    for (int ch : used) {
        std::vector<Ev> evs;
        for (const auto& n : r.notes) {
            if (n.channel != ch) continue;
            long a = std::lround(n.start), b = std::max(a + 1, std::lround(n.start + n.length));
            uint8_t p = (uint8_t) std::max(0, std::min(127, n.pitch)), v = (uint8_t) std::max(1, std::min(127, n.velocity));
            evs.push_back({a, 1, (uint8_t) (0x90 | ch), p, v});
            evs.push_back({b, 0, (uint8_t) (0x80 | ch), p, 0});
        }
        std::stable_sort(evs.begin(), evs.end(), [](const Ev& x, const Ev& y) {
            return x.tick != y.tick ? x.tick < y.tick : x.order < y.order;
        });
        std::vector<uint8_t> t;
        meta(t, 0, 0x03, text(names[ch]));
        long last = 0;
        for (const auto& e : evs) {
            vlq(t, (unsigned) (e.tick - last)); last = e.tick;
            t.push_back(e.status); t.push_back(e.d1); t.push_back(e.d2);
        }
        meta(t, 0, 0x2F, {});
        chunk(out, t);
    }
    return out;
}

bool writeMidiFile(const Result& r, const std::string& path) {
    auto bytes = writeMidi(r);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*) bytes.data(), (std::streamsize) bytes.size());
    return (bool) f;
}

} // namespace hhmg
