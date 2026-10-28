// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <obs.h>

// libobs for the integration tests: started on first use, so listing the tests at build time starts nothing, and shut
// down once the run ends. It registers OBS 30.0.2's own Video Delay (Async), compiled into the test, and test types for
// a camera, a mic, Render Delay and another filter type that can carry a recognised name.
namespace obs_env {

constexpr const char *camera_type = "camera_sync_test_camera";
constexpr const char *mic_type = "camera_sync_test_mic";
constexpr const char *delay_type = "async_delay_filter";
constexpr const char *render_delay_type = "gpu_delay";
constexpr const char *other_filter_type = "camera_sync_test_filter";

// Starts libobs the first time. False, with the reason in the test's output, when it cannot start: the case then runs
// nothing, and ALIGN-80 moves its checks to the rig.
bool ready();

// Shuts libobs down if it started. main calls it after the run.
void shutdown();

// A public source a test makes and releases when it ends.
class Source {
public:
	Source(const char *type, const char *name);
	~Source();
	Source(const Source &) = delete;
	Source &operator=(const Source &) = delete;

	obs_source_t *get() const { return source_; }
	std::string uuid() const;
	// Drops the test's reference, which destroys the source: it leaves OBS as a deleted source does.
	void remove();

	// Appends a filter of this type and name, with delay_ms set when the type has it.
	void add_filter(const char *type, const char *name, int64_t delay_ms = 0, bool enabled = true);
	// Every filter in chain order, as "<type> <name> <settings JSON> on|off".
	std::vector<std::string> filters() const;

private:
	obs_source_t *source_;
};

} // namespace obs_env
