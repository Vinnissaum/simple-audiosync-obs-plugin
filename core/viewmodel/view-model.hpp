// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "alignment/types.hpp"
#include "reading/session.hpp"
#include "selection/selection-policy.hpp"

namespace camera_sync {

// The text of a locale key in the user's language. The dock passes OBS's lookup, tests pass a table.
using Translate = std::function<std::string(const char *key)>;

// One row of the camera checklist.
struct CameraRowView {
	SourceId id;
	std::string name;
	bool ticked = false;
	std::string status;  // empty unless the camera is ticked and the session reports it
	std::string details; // "received {n}, missed {m}" with the details toggle on; empty otherwise
};

// One entry of the mic picker.
struct MicView {
	SourceId id;
	std::string name;
	bool chosen = false;
};

// Everything the dock shows, worked out from a session snapshot, the catalog and the selection.
struct DockView {
	std::vector<CameraRowView> cameras; // the async video sources other than the chosen mic, in name order
	std::vector<MicView> mics;          // the sources with audio other than the ticked cameras, in name order
	std::string message;                // the message line; empty when there is nothing to say
	bool start_enabled = false;
	bool reset_enabled = false;
	bool apply_enabled = false;
	bool undo_enabled = false;
	std::string start_label; // "Stop" while measuring, "Start" otherwise
	bool show_details = false;
};

// A signed millisecond value as the dock writes it: "+142", "−60" with U+2212, "0".
std::string format_signed_ms(int64_t ms);

// A value Apply or Undo wrote, as the message line gives it: "60", "−200" with U+2212, "0" (ALIGN-54, ALIGN-55).
std::string format_value_ms(int64_t ms);

// The dock's view. `sources` is the catalog's list, `choice` the ticked cameras and the chosen mic, `details` whether
// the details toggle is on, and `alignment` what the last Apply or Undo left.
DockView build_view(const SessionSnapshot &snapshot, const std::vector<SourceInfo> &sources, const Choice &choice,
		    bool details, const Translate &translate, const AlignmentStatus &alignment = {});

// The on-air question for what Apply or Undo would change (ALIGN-61), and its second line, empty when no camera
// changes (ALIGN-62).
std::string confirmation_question(const Counts &counts, const Translate &translate);
std::string confirmation_detail(const Counts &counts, const Translate &translate);

} // namespace camera_sync
