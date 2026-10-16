// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>

#include "selection/selection-policy.hpp"

namespace camera_sync {

// Reads the scene collection's sources for the selection rules. It decides nothing: which source is a camera or a mic,
// and what is ticked, is up to the functions in selection-policy.hpp. Call it on the UI thread.
class ObsCatalog {
public:
	// Every source OBS lists, with its UUID, its name, whether it has asynchronous video or audio, and whether it is in
	// the current Program scene, counting the scenes and groups inside it.
	std::vector<SourceInfo> list() const;
};

} // namespace camera_sync
