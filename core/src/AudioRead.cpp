#include "hhmg/AudioRead.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace hhmg {
namespace {
uint32_t le32(const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
uint16_t le16(const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }
}

bool readWav(const std::vector<uint8_t>& d, std::vector<float>& mono, int& sampleRate, std::string& error) {
    mono.clear();
    if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0) { error = "not a WAV file"; return false; }
    size_t pos = 12;
    int format = 0, channels = 0, bits = 0;
    const uint8_t* data = nullptr; size_t dataLen = 0;
    while (pos + 8 <= d.size()) {
        uint32_t len = le32(&d[pos + 4]);
        const uint8_t* body = &d[pos + 8];
        size_t avail = std::min<size_t>(len, d.size() - pos - 8);
        if (std::memcmp(&d[pos], "fmt ", 4) == 0 && avail >= 16) {
            format = le16(body); channels = le16(body + 2); sampleRate = (int) le32(body + 4); bits = le16(body + 14);
            if (format == 0xFFFE && avail >= 26) format = le16(body + 24);   // extensible: sub-format GUID starts with the real tag
        } else if (std::memcmp(&d[pos], "data", 4) == 0) { data = body; dataLen = avail; break; }
        pos += 8 + (size_t) len + (len & 1);
    }
    if (!data || channels < 1 || sampleRate < 1) { error = "WAV header incomplete"; return false; }
    const bool isFloat = format == 3, isPcm = format == 1;
    if (!(isFloat && bits == 32) && !(isPcm && (bits == 8 || bits == 16 || bits == 24 || bits == 32))) { error = "unsupported WAV encoding (use 16/24-bit PCM or float)"; return false; }
    const size_t bytes = (size_t) bits / 8, frame = bytes * (size_t) channels, frames = dataLen / frame;
    mono.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        double acc = 0;
        for (int c = 0; c < channels; ++c) {
            const uint8_t* p = data + i * frame + (size_t) c * bytes;
            double v;
            if (isFloat) { float f; std::memcpy(&f, p, 4); v = f; }
            else if (bits == 8) v = ((int) p[0] - 128) / 128.0;
            else if (bits == 16) v = (int16_t) le16(p) / 32768.0;
            else if (bits == 24) { int32_t s = (int32_t) ((uint32_t) p[0] << 8 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 24) >> 8; v = s / 8388608.0; }
            else v = (int32_t) le32(p) / 2147483648.0;
            acc += v;
        }
        mono[i] = (float) (acc / channels);
    }
    return true;
}

bool readWavFile(const std::string& path, std::vector<float>& mono, int& sampleRate, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open " + path; return false; }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return readWav(bytes, mono, sampleRate, error);
}

std::vector<uint8_t> writeWav16(const std::vector<float>& x, int sr) {
    std::vector<uint8_t> o;
    auto p32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) o.push_back((uint8_t) (v >> (8 * i))); };
    auto p16 = [&](uint16_t v) { o.push_back((uint8_t) v); o.push_back((uint8_t) (v >> 8)); };
    o.insert(o.end(), {'R', 'I', 'F', 'F'}); p32((uint32_t) (36 + x.size() * 2)); o.insert(o.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    p32(16); p16(1); p16(1); p32((uint32_t) sr); p32((uint32_t) sr * 2); p16(2); p16(16);
    o.insert(o.end(), {'d', 'a', 't', 'a'}); p32((uint32_t) (x.size() * 2));
    for (float f : x) p16((uint16_t) (int16_t) std::lround(std::max(-1.0f, std::min(1.0f, f)) * 32767.0f));
    return o;
}

} // namespace hhmg
