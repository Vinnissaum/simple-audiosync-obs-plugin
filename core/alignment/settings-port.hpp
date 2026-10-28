// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "alignment/types.hpp"

namespace camera_sync {

// The names a plugin filter may carry. The first is the name a new filter gets; a later product name is added to the
// list, never put in place of one, so the filters earlier versions made are still found (ALIGN-42).
inline constexpr std::array<const char *, 1> recognised_filter_names = {"Camera Sync Delay"};

// The only way the core reaches OBS's settings: a camera's plugin filter and the mic's Sync Offset. It has no operation
// that reaches another filter, property or source (ALIGN-41, Key decision 4). A camera's plugin filter is the first
// Video Delay (Async) filter on it, in chain order, whose name is on the recognised list.
class SettingsPort {
public:
	virtual ~SettingsPort() = default;
	// The source with this UUID exists.
	virtual bool resolves(const SourceId &source) = 0;
	// Video Delay (Async) is registered, so a filter of that type delays.
	virtual bool delay_filter_available() = 0;
	// The camera's plugin filter, absent or present; none when the camera is gone.
	virtual std::optional<FilterState> read_filter(const SourceId &camera) = 0;
	// Sets the plugin filter's delay and enabled state, creating it at the end of the chain when it is absent.
	virtual bool write_filter(const SourceId &camera, int64_t delay_ms, bool enabled) = 0;
	// Removes the plugin filter; true when there is none.
	virtual bool remove_filter(const SourceId &camera) = 0;
	// The mic's Sync Offset in nanoseconds; none when the mic is gone.
	virtual std::optional<int64_t> read_sync_offset_ns(const SourceId &mic) = 0;
	virtual bool write_sync_offset_ns(const SourceId &mic, int64_t ns) = 0;
	// Streaming or recording is active, a paused recording included.
	virtual bool on_air() = 0;
};

} // namespace camera_sync
