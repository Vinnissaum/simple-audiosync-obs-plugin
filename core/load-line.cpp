// SPDX-License-Identifier: GPL-2.0-or-later
#include "load-line.hpp"

namespace camera_sync {

std::string format_version(uint32_t packed)
{
	const uint32_t major = packed >> 24;
	const uint32_t minor = (packed >> 16) & 0xff;
	const uint32_t patch = packed & 0xffff;
	return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::string load_line(std::string_view version, uint32_t built, std::string_view running, std::string_view quirc)
{
	std::string line = "load version=";
	line += version;
	line += " obs_built=" + format_version(built);
	line += " obs_running=";
	line += running;
	line += " quirc=";
	line += quirc;
	return line;
}

} // namespace camera_sync
