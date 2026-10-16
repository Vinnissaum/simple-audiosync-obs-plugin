// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace camera_sync {

// The measuring log lines: one line per event in key=value form, so a support log can be searched. Each function
// returns the text after the "[camera-sync] " prefix, which the plugin's logger adds.

// A source name in double quotes, with a double quote in it written \" and a backslash written \\.
std::string quote(std::string_view name);

// measure.start cameras=<n> mic="<name>" canvas_fps=<fps>
std::string measure_start(size_t cameras, std::string_view mic, uint32_t fps_num, uint32_t fps_den);

// measure.stop
std::string measure_stop();

// measure.reset
std::string measure_reset();

// reading.stable camera="<name>" reading_ms=<signed integer> spread_ms=<integer>
std::string reading_stable(std::string_view camera, int64_t reading_ms, int64_t spread_ms);

} // namespace camera_sync
