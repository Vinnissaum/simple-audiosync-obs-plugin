// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "alignment/types.hpp"

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

// The Apply and Undo lines. `prefix` is "apply" or "undo", and each line starts with it (ALIGN-75). Values are ASCII,
// a minus as "-", like reading.stable's.

// A plugin filter as the lines write it: "none" when absent, "<ms>(off)" when disabled, "<ms>" otherwise.
std::string filter_value(const FilterState &filter);

// One value a plan changes, from the value found to the value it writes.
struct PlanEntry {
	std::string name;
	std::string from;
	std::string to;
};

// <prefix>.plan camera="<name>" delay_ms=<from>-><to> ... mic="<name>" sync_offset_ms=<from>-><to>
std::string alignment_plan(std::string_view prefix, const std::vector<PlanEntry> &cameras, const PlanEntry &mic);

// <prefix>.result ok written=<n> skipped=<m> duration_ms=<d>
std::string alignment_ok(std::string_view prefix, size_t written, size_t skipped, int64_t duration_ms);

// <prefix>.result failed step=camera:"<name>" restored=yes|no, or step=mic:"<name>"
std::string alignment_failed(std::string_view prefix, bool mic, std::string_view name, bool restored);

// restore.failed camera="<name>" intended_ms=<value> actual_ms=<value>
std::string restore_failed_camera(std::string_view name, const FilterState &intended, const FilterState &actual);

// restore.failed mic="<name>" intended_ns=<value> actual_ns=<value>
std::string restore_failed_mic(std::string_view name, int64_t intended_ns, int64_t actual_ns);

// <prefix>.refused reason=waiting camera="<name>"
std::string alignment_refused_waiting(std::string_view prefix, std::string_view camera);

// <prefix>.refused reason=source_gone source="<uuid>"
std::string alignment_refused_source_gone(std::string_view prefix, std::string_view source_id);

// <prefix>.refused reason=no_delay_filter
std::string alignment_refused_no_delay_filter(std::string_view prefix);

// <prefix>.refused reason=over_limit source="<name>" value_ms=<value>
std::string alignment_refused_over_limit(std::string_view prefix, std::string_view source, int64_t value_ms);

// <prefix>.refused reason=changed
std::string alignment_refused_changed(std::string_view prefix);

// <prefix>.<event>, for already_aligned and cancelled
std::string alignment_event(std::string_view prefix, std::string_view event);

} // namespace camera_sync
