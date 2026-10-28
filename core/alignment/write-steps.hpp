// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "alignment/settings-port.hpp"
#include "alignment/types.hpp"

namespace camera_sync {

// One value to write: a camera's plugin filter or the mic's Sync Offset, with the value found there, which a failure
// puts back. A camera's absent target removes its plugin filter.
struct Step {
	bool mic = false;
	SourceId source;
	std::string name;
	FilterState camera_target, camera_found;
	int64_t mic_target_ns = 0, mic_found_ns = 0;
};

// A value that could not be put back, with what it read as afterwards.
struct RestoreFailure {
	size_t step = 0;
	FilterState camera_actual;
	int64_t mic_actual_ns = 0;
};

struct StepsResult {
	bool ok = true;
	size_t written = 0;           // steps attempted, the failed one included
	std::optional<size_t> failed; // the step that failed
	std::vector<RestoreFailure> restore_failures;
};

// Writes each step in order and reads it back at once. A write the port refuses, or one that reads back different,
// fails the step, and nothing more is written (ALIGN-24, ALIGN-25). Then every step written so far, the failed one
// included, since a write can half-land, is put back newest first: a value that still reads as found, or a source
// that is gone, needs nothing; a value that will not go back is recorded and the rest are still put back (ALIGN-26).
// Apply, Undo and both their restores run through this one routine.
StepsResult write_steps(SettingsPort &port, const std::vector<Step> &steps);

} // namespace camera_sync
