// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "reading/types.hpp"

namespace camera_sync {

// What the catalog reads of one OBS source.
struct SourceInfo {
	SourceId id;
	std::string name;
	bool async_video = false; // a camera: a webcam, capture card, media or NDI source
	bool audio = false;       // a mic candidate
	bool in_program = false;  // in the current Program scene, nested scenes and groups included
};

// The selection a scene collection remembers in the plugin's module data.
struct SavedSelection {
	int version = 0;
	std::vector<SourceId> cameras;
	std::optional<SourceId> mic;
};

// The ticked cameras and the chosen mic.
struct Choice {
	std::vector<SourceId> cameras;
	std::optional<SourceId> mic;
};

// The version of the saved selection this plugin writes, and the only one it reads.
constexpr int saved_selection_version = 1;

// The selection a collection opens with. A saved selection of version 1 ticks and chooses the sources it names that
// still exist and nothing else. With no saved selection, or one of another version, every camera in the Program scene
// is ticked, and the mic is chosen when exactly one source has audio and that source is not a camera just ticked.
// Cameras come in the order of sources.
Choice initial_choice(const std::vector<SourceInfo> &sources, const std::optional<SavedSelection> &saved);

// The camera checklist: the async video sources other than the chosen mic, sorted by name ignoring case.
std::vector<SourceInfo> camera_list(const std::vector<SourceInfo> &sources, const std::optional<SourceId> &mic);

// The mic picker: the sources with audio other than the ticked cameras, sorted by name ignoring case.
std::vector<SourceInfo> mic_list(const std::vector<SourceInfo> &sources, const std::vector<SourceId> &ticked);

// The selection as the collection saves it: version 1, the cameras' UUIDs, and the mic's or none.
SavedSelection to_saved(const Choice &choice);

} // namespace camera_sync
