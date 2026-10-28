// SPDX-License-Identifier: GPL-2.0-or-later
#include "alignment/write-steps.hpp"

namespace camera_sync {

namespace {

bool write_camera(SettingsPort &port, const SourceId &camera, const FilterState &value)
{
	return value.present ? port.write_filter(camera, value.delay_ms, value.enabled) : port.remove_filter(camera);
}

// Writes the step's target and reads it back.
bool land(SettingsPort &port, const Step &step)
{
	if (step.mic) {
		if (!port.write_sync_offset_ns(step.source, step.mic_target_ns))
			return false;
		const std::optional<int64_t> actual = port.read_sync_offset_ns(step.source);
		return actual && *actual == step.mic_target_ns;
	}
	if (!write_camera(port, step.source, step.camera_target))
		return false;
	const std::optional<FilterState> actual = port.read_filter(step.source);
	return actual && *actual == step.camera_target;
}

// Puts the step's found value back unless it already reads as found or the source is gone. A value that will not go
// back is recorded with what it reads as.
void put_back(SettingsPort &port, const Step &step, size_t index, StepsResult &result)
{
	if (step.mic) {
		const std::optional<int64_t> current = port.read_sync_offset_ns(step.source);
		if (!current || *current == step.mic_found_ns)
			return;
		const bool written = port.write_sync_offset_ns(step.source, step.mic_found_ns);
		const std::optional<int64_t> actual = port.read_sync_offset_ns(step.source);
		if (actual && (!written || *actual != step.mic_found_ns))
			result.restore_failures.push_back(RestoreFailure{index, FilterState{}, *actual});
		return;
	}
	const std::optional<FilterState> current = port.read_filter(step.source);
	if (!current || *current == step.camera_found)
		return;
	const bool written = write_camera(port, step.source, step.camera_found);
	const std::optional<FilterState> actual = port.read_filter(step.source);
	if (actual && (!written || *actual != step.camera_found))
		result.restore_failures.push_back(RestoreFailure{index, *actual, 0});
}

} // namespace

StepsResult write_steps(SettingsPort &port, const std::vector<Step> &steps)
{
	StepsResult result;
	for (size_t i = 0; i < steps.size(); i++) {
		result.written = i + 1;
		if (!land(port, steps[i])) {
			result.ok = false;
			result.failed = i;
			break;
		}
	}
	if (result.ok)
		return result;

	for (size_t i = *result.failed + 1; i-- > 0;)
		put_back(port, steps[i], i, result);
	return result;
}

} // namespace camera_sync
