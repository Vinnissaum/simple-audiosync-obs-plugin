// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "reading/types.hpp"

namespace camera_sync {

// A camera's plugin filter as OBS holds it: absent, or present with its delay and whether it is enabled. An absent
// filter is always the default value, so two absent filters compare equal.
struct FilterState {
	bool present = false;
	int64_t delay_ms = 0; // meaningful when present
	bool enabled = true;

	bool operator==(const FilterState &other) const
	{
		return present == other.present && delay_ms == other.delay_ms && enabled == other.enabled;
	}
	bool operator!=(const FilterState &other) const { return !(*this == other); }

	// What the filter delays the camera by: nothing when it is absent or disabled (ALIGN-03).
	int64_t effective_ms() const { return present && enabled ? delay_ms : 0; }
};

// A camera or the mic, with the name the log and the dock show.
struct NamedSource {
	SourceId id;
	std::string name;
};

// What the on-air question counts: the cameras whose value changes, and whether the mic's does (ALIGN-61).
struct Counts {
	size_t cameras = 0;
	bool mic = false;
};

enum class OutcomeKind {
	Waiting,
	AlreadyAligned,
	OverLimit,
	Cancelled,
	ApplyFailed,
	ApplyRestoreFailed,
	Applied,
	UndoFailed,
	UndoRestoreFailed,
	Undone,
};

struct NamedValue {
	std::string name;
	int64_t ms = 0;
};

// What an Apply or an Undo did.
struct Outcome {
	OutcomeKind kind = OutcomeKind::Waiting;
	std::string source;             // Waiting and OverLimit: the camera's or the mic's name
	std::vector<NamedValue> values; // Applied and Undone: every camera in order, then the mic
};

// What the dock reads of the alignment.
struct AlignmentStatus {
	bool undo_available = false;
	bool aligned_after_apply = false; // ALIGN-58: set by an Apply, cleared by Undo, Reset or a switch
	std::optional<Outcome> last;      // the message line's result; never Waiting or Cancelled
};

} // namespace camera_sync
