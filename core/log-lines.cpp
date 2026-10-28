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

std::string filter_value(const FilterState &filter)
{
	if (!filter.present)
		return "none";
	return std::to_string(filter.delay_ms) + (filter.enabled ? "" : "(off)");
}

std::string alignment_plan(std::string_view prefix, const std::vector<PlanEntry> &cameras, const PlanEntry &mic)
{
	std::string text = std::string(prefix) + ".plan";
	for (const PlanEntry &camera : cameras)
		text += " camera=" + quote(camera.name) + " delay_ms=" + camera.from + "->" + camera.to;
	return text + " mic=" + quote(mic.name) + " sync_offset_ms=" + mic.from + "->" + mic.to;
}

std::string alignment_ok(std::string_view prefix, size_t written, size_t skipped, int64_t duration_ms)
{
	return std::string(prefix) + ".result ok written=" + std::to_string(written) +
	       " skipped=" + std::to_string(skipped) + " duration_ms=" + std::to_string(duration_ms);
}

std::string alignment_failed(std::string_view prefix, bool mic, std::string_view name, bool restored)
{
	return std::string(prefix) + ".result failed step=" + (mic ? "mic:" : "camera:") + quote(name) +
	       " restored=" + (restored ? "yes" : "no");
}

std::string restore_failed_camera(std::string_view name, const FilterState &intended, const FilterState &actual)
{
	return "restore.failed camera=" + quote(name) + " intended_ms=" + filter_value(intended) +
	       " actual_ms=" + filter_value(actual);
}

std::string restore_failed_mic(std::string_view name, int64_t intended_ns, int64_t actual_ns)
{
	return "restore.failed mic=" + quote(name) + " intended_ns=" + std::to_string(intended_ns) +
	       " actual_ns=" + std::to_string(actual_ns);
}

std::string alignment_refused_waiting(std::string_view prefix, std::string_view camera)
{
	return std::string(prefix) + ".refused reason=waiting camera=" + quote(camera);
}

std::string alignment_refused_source_gone(std::string_view prefix, std::string_view source_id)
{
	return std::string(prefix) + ".refused reason=source_gone source=" + quote(source_id);
}

std::string alignment_refused_no_delay_filter(std::string_view prefix)
{
	return std::string(prefix) + ".refused reason=no_delay_filter";
}

std::string alignment_refused_over_limit(std::string_view prefix, std::string_view source, int64_t value_ms)
{
	return std::string(prefix) + ".refused reason=over_limit source=" + quote(source) +
	       " value_ms=" + std::to_string(value_ms);
}

std::string alignment_refused_changed(std::string_view prefix)
{
	return std::string(prefix) + ".refused reason=changed";
}

std::string alignment_event(std::string_view prefix, std::string_view event)
{
	return std::string(prefix) + "." + std::string(event);
}

} // namespace camera_sync
