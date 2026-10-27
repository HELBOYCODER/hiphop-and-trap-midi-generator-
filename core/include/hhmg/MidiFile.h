// Standard MIDI File (format 1) writer.
#pragma once
#include "Generator.h"
#include <cstdint>
#include <string>
#include <vector>

namespace hhmg {

std::vector<uint8_t> writeMidi(const Result& r);
bool writeMidiFile(const Result& r, const std::string& path);

} // namespace hhmg
