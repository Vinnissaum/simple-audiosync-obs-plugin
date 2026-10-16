// SPDX-License-Identifier: GPL-2.0-or-later
#include "log-lines.hpp"

namespace camera_sync {

namespace {

// The spec gives the canvas fps no format. It is fps_num / fps_den rounded to two decimals with trailing zeros
// trimmed, as OBS names its rates: 30 gives "30", 60000/1001 "59.94", 24000/1001 "23.98" and 25/2 "12.5".
std::string format_fps(uint32_t fps_num, uint32_t fps_den)
{
	const uint64_t hundredths = (uint64_t(fps_num) * 200 + fps_den) / (uint64_t(fps_den) * 2);
	std::string text = std::to_string(hundredths / 100);
	const uint64_t fraction = hundredths % 100;
	if (fraction % 10 != 0)
		text += "." + std::to_string(fraction / 10) + std::to_string(fraction % 10);
	else if (fraction != 0)
		text += "." + std::to_string(fraction / 10);
	return text;
}

} // namespace

std::string quote(std::string_view name)
{
	std::string text = "\"";
	for (char c : name) {
		if (c == '"' || c == '\\')
			text += '\\';
		text += c;
	}
	text += '"';
	return text;
}

std::string measure_start(size_t cameras, std::string_view mic, uint32_t fps_num, uint32_t fps_den)
{
	return "measure.start cameras=" + std::to_string(cameras) + " mic=" + quote(mic) +
	       " canvas_fps=" + format_fps(fps_num, fps_den);
}

std::string measure_stop()
{
	return "measure.stop";
}

std::string measure_reset()
{
	return "measure.reset";
}

std::string reading_stable(std::string_view camera, int64_t reading_ms, int64_t spread_ms)
{
	// The sign is ASCII "+" or "-": the log is read by tools and people searching it, not shown in the dock, which
	// writes a minus as U+2212. Zero has no sign.
	const std::string reading = (reading_ms > 0 ? "+" : "") + std::to_string(reading_ms);
	return "reading.stable camera=" + quote(camera) + " reading_ms=" + reading +
	       " spread_ms=" + std::to_string(spread_ms);
}

} // namespace camera_sync
