// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>

namespace camera_sync {

// A camera or the mic, by its OBS source UUID.
using SourceId = std::string;

// Where a camera's reading stands: no sample yet, samples that do not agree yet, or a reading that can be trusted.
enum class CameraState { Looking, Settling, Stable };

} // namespace camera_sync
