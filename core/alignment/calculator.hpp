// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "alignment/types.hpp"
#include "reading/estimator.hpp"

namespace camera_sync {

// The most OBS lets a camera's delay or the mic's Sync Offset be, in ms.
inline constexpr int64_t max_value_ms = 20000;

// A ticked camera's reading as Apply sees it.
struct Reading {
	bool stable = false;
	int64_t ms = 0; // meaningful when stable
};

// Each camera's reading, in the order of `cameras`. A camera the session has no row for is not Stable.
std::vector<Reading> readings_of(const std::vector<SourceId> &cameras, const std::vector<CameraRow> &rows);

// A reading r is within one frame of the mic when -F <= r <= F.
bool within_one_frame(int64_t reading_ms, double frame_interval_ms);

enum class PrecheckKind { Ready, Waiting, AlreadyAligned };

struct Precheck {
	PrecheckKind kind = PrecheckKind::Ready;
	size_t camera = 0; // Waiting: the index of the first camera that is not Stable
};

// Whether Apply can go ahead, decided from the readings alone, so nothing is read from OBS before it says yes: Waiting
// while a camera is not Stable (ALIGN-01), then AlreadyAligned when every reading r has -F <= r <= F (ALIGN-02).
Precheck precheck(const std::vector<Reading> &readings, double frame_interval_ms);

struct CameraPlan {
	int64_t old_ms = 0; // the effective delay
	int64_t new_ms = 0;
	bool changed = false;
};

struct Plan {
	int64_t delta_ms = 0;
	std::vector<CameraPlan> cameras; // in the readings' order
	int64_t old_offset_ms = 0;       // s, rounded from its nanoseconds
	int64_t new_offset_ms = 0;
	bool mic_changed = false;
};

struct PlanResult {
	std::optional<Plan> plan;
	std::optional<size_t> over_limit; // the first source over max_value_ms; the camera count names the mic
	int64_t over_limit_ms = 0;
};

// The alignment for these readings, each camera's plugin filter and the mic's Sync Offset (ALIGN-03 to ALIGN-10):
// delta is the largest of every (r - d) and -s, each camera gets d + delta - r and the mic s + delta.
PlanResult compute_plan(const std::vector<int64_t> &readings_ms, const std::vector<FilterState> &filters,
			int64_t sync_offset_ns);

} // namespace camera_sync
