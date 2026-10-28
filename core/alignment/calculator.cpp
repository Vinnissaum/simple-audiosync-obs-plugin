// SPDX-License-Identifier: GPL-2.0-or-later
#include "alignment/calculator.hpp"

#include <algorithm>

#include "reading/pattern.hpp"

namespace camera_sync {

std::vector<Reading> readings_of(const std::vector<SourceId> &cameras, const std::vector<CameraRow> &rows)
{
	std::vector<Reading> readings;
	for (const SourceId &camera : cameras) {
		Reading reading;
		for (const CameraRow &row : rows) {
			if (row.id == camera && row.state == CameraState::Stable)
				reading = Reading{true, row.reading_ms};
		}
		readings.push_back(reading);
	}
	return readings;
}

Precheck precheck(const std::vector<Reading> &readings, double frame_interval_ms)
{
	for (size_t i = 0; i < readings.size(); i++) {
		if (!readings[i].stable)
			return Precheck{PrecheckKind::Waiting, i};
	}
	for (const Reading &reading : readings) {
		const double ms = static_cast<double>(reading.ms);
		if (ms < -frame_interval_ms || ms > frame_interval_ms)
			return Precheck{PrecheckKind::Ready, 0};
	}
	return Precheck{PrecheckKind::AlreadyAligned, 0};
}

PlanResult compute_plan(const std::vector<int64_t> &readings_ms, const std::vector<FilterState> &filters,
			int64_t sync_offset_ns)
{
	Plan plan;
	plan.old_offset_ms = round_half_away(static_cast<double>(sync_offset_ns) / 1e6);

	// Delta is the least that leaves no value negative (ALIGN-04).
	plan.delta_ms = -plan.old_offset_ms;
	for (size_t i = 0; i < readings_ms.size(); i++)
		plan.delta_ms = std::max(plan.delta_ms, readings_ms[i] - filters[i].effective_ms());

	PlanResult result;
	for (size_t i = 0; i < readings_ms.size(); i++) {
		CameraPlan camera;
		camera.old_ms = filters[i].effective_ms();
		camera.new_ms = camera.old_ms + plan.delta_ms - readings_ms[i];
		camera.changed = camera.new_ms != camera.old_ms;
		if (camera.new_ms > max_value_ms && !result.over_limit) {
			result.over_limit = i;
			result.over_limit_ms = camera.new_ms;
		}
		plan.cameras.push_back(camera);
	}
	plan.new_offset_ms = plan.old_offset_ms + plan.delta_ms;
	plan.mic_changed = plan.new_offset_ms != plan.old_offset_ms;
	if (plan.new_offset_ms > max_value_ms && !result.over_limit) {
		result.over_limit = readings_ms.size();
		result.over_limit_ms = plan.new_offset_ms;
	}

	if (!result.over_limit)
		result.plan = std::move(plan);
	return result;
}

} // namespace camera_sync
