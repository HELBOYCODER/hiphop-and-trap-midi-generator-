// WAV reader (PCM 8/16/24/32-bit and float32, any channel count, WAVE_FORMAT_EXTENSIBLE) -> mono float.
// Other formats (mp3, m4a, flac ...) are decoded by the host application (JUCE / Android MediaCodec).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace hhmg {

bool readWav(const std::vector<uint8_t>& bytes, std::vector<float>& mono, int& sampleRate, std::string& error);
bool readWavFile(const std::string& path, std::vector<float>& mono, int& sampleRate, std::string& error);
std::vector<uint8_t> writeWav16(const std::vector<float>& mono, int sampleRate); // for tests / tools

} // namespace hhmg
