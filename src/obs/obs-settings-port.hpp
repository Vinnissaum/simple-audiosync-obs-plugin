// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "alignment/settings-port.hpp"

namespace camera_sync {

// The settings port over libobs. A camera's plugin filter is the first Video Delay (Async) filter in its chain whose
// name is on the recognised list; no other filter, property or source is read or changed (ALIGN-41 to ALIGN-46).
// Whether OBS is on air comes from the frontend, handed in as a function, so the port links without the frontend and
// the integration tests run the real class.
class ObsSettingsPort : public SettingsPort {
public:
	explicit ObsSettingsPort(std::function<bool()> on_air);

	bool resolves(const SourceId &source) override;
	bool delay_filter_available() override;
	std::optional<FilterState> read_filter(const SourceId &camera) override;
	bool write_filter(const SourceId &camera, int64_t delay_ms, bool enabled) override;
	bool remove_filter(const SourceId &camera) override;
	std::optional<int64_t> read_sync_offset_ns(const SourceId &mic) override;
	bool write_sync_offset_ns(const SourceId &mic, int64_t ns) override;
	bool on_air() override;

private:
	std::function<bool()> on_air_;
};

} // namespace camera_sync
