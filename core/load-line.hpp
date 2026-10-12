// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace camera_sync {

// Turns a libobs packed version (major << 24 | minor << 16 | patch) into "major.minor.patch".
std::string format_version(uint32_t packed);

// The text after the "[camera-sync] " prefix of the line the plugin logs when OBS loads it.
// built is the libobs version compiled against, packed. running is the running OBS's version string.
std::string load_line(std::string_view version, uint32_t built, std::string_view running, std::string_view quirc);

} // namespace camera_sync
