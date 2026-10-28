// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "alignment/alignment.hpp"
#include "obs-env.hpp"
#include "obs/obs-settings-port.hpp"
#include "support/alignment-harness.hpp"

using camera_sync::Alignment;
using camera_sync::AlignmentRequest;
using camera_sync::CameraRow;
using camera_sync::CameraState;
using camera_sync::FilterState;
using camera_sync::ObsSettingsPort;
using camera_sync::OutcomeKind;
using camera_sync::SourceId;
using obs_env::Source;

namespace {

using Strings = std::vector<std::string>;

bool off_air()
{
	return false;
}

bool never(const camera_sync::Counts &)
{
	return false;
}

// The OBS port with a hook that runs once, before the first write to one source.
struct HookedPort : camera_sync::SettingsPort {
	ObsSettingsPort inner{off_air};
	SourceId before_writing;
	std::function<void()> hook;

	void fire(const SourceId &source)
	{
		if (hook && source == before_writing) {
			std::function<void()> once = std::move(hook);
			hook = nullptr;
			once();
		}
	}

	bool resolves(const SourceId &source) override { return inner.resolves(source); }
	bool delay_filter_available() override { return inner.delay_filter_available(); }
	std::optional<FilterState> read_filter(const SourceId &camera) override { return inner.read_filter(camera); }
	bool write_filter(const SourceId &camera, int64_t delay_ms, bool enabled) override
	{
		fire(camera);
		return inner.write_filter(camera, delay_ms, enabled);
	}
	bool remove_filter(const SourceId &camera) override
	{
		fire(camera);
		return inner.remove_filter(camera);
	}
	std::optional<int64_t> read_sync_offset_ns(const SourceId &mic) override
	{
		return inner.read_sync_offset_ns(mic);
	}
	bool write_sync_offset_ns(const SourceId &mic, int64_t ns) override
	{
		fire(mic);
		return inner.write_sync_offset_ns(mic, ns);
	}
	bool on_air() override { return inner.on_air(); }
};

// Two test cameras and a test mic in OBS, and an Alignment over the OBS port.
struct Rig {
	Source cam_a{obs_env::camera_type, "Integration Cam A"};
	Source cam_b{obs_env::camera_type, "Integration Cam B"};
	Source mic{obs_env::mic_type, "Integration Mic"};
	HookedPort port;
	FakeMeasuring measuring;
	Alignment alignment{port, measuring, [] { return uint64_t(0); }, nullptr};

	// Both cameras Stable at these readings, at F = 33.333 ms.
	AlignmentRequest request(int64_t a_ms, int64_t b_ms) const
	{
		AlignmentRequest request;
		request.cameras = {{cam_a.uuid(), "Cam A"}, {cam_b.uuid(), "Cam B"}};
		request.mic = camera_sync::NamedSource{mic.uuid(), "Mic"};
		request.session.state = camera_sync::SessionState::Measuring;
		request.session.frame_interval_ms = 33.333;
		for (const auto &reading : {std::make_pair(cam_a.uuid(), a_ms), std::make_pair(cam_b.uuid(), b_ms)}) {
			CameraRow row;
			row.id = reading.first;
			row.state = CameraState::Stable;
			row.reading_ms = reading.second;
			request.session.cameras.push_back(row);
		}
		request.rate = {30, 1};
		return request;
	}
};

std::string plugin_filter(int64_t ms)
{
	return "async_delay_filter Camera Sync Delay {\"delay_ms\":" + std::to_string(ms) + "} on";
}

} // namespace

TEST_CASE("apply and undo the first vector against libobs")
{
	if (!obs_env::ready())
		return;
	Rig rig;

	// ALIGN-77: Cam B gets a plugin filter at 60 ms, the mic 100 ms, Cam A nothing.
	REQUIRE(rig.alignment.apply(rig.request(100, 40), never).kind == OutcomeKind::Applied);
	CHECK(rig.cam_a.filters().empty());
	CHECK(rig.cam_b.filters() == Strings{plugin_filter(60)});
	CHECK(obs_source_get_sync_offset(rig.mic.get()) == 100000000);

	REQUIRE(rig.alignment.undo(never).kind == OutcomeKind::Undone);
	CHECK(rig.cam_a.filters().empty());
	CHECK(rig.cam_b.filters().empty());
	CHECK(obs_source_get_sync_offset(rig.mic.get()) == 0);
}

TEST_CASE("apply restores every value when a camera is removed before its write")
{
	if (!obs_env::ready())
		return;
	Rig rig;
	obs_source_set_sync_offset(rig.mic.get(), -200000000);

	// The hand-set negative vector writes Cam A, then Cam B, then the mic. Cam B leaves OBS after the reads, just
	// before its write.
	rig.port.before_writing = rig.cam_b.uuid();
	rig.port.hook = [&rig] {
		rig.cam_b.remove();
	};

	// ALIGN-78: Cam A's new filter is taken off again and the mic was never written.
	CHECK(rig.alignment.apply(rig.request(60, 10), never).kind == OutcomeKind::ApplyFailed);
	CHECK(rig.cam_a.filters().empty());
	CHECK(obs_source_get_sync_offset(rig.mic.get()) == -200000000);
	CHECK_FALSE(rig.alignment.status().undo_available);
}

TEST_CASE("apply and undo leave the user's filters unchanged and in order")
{
	if (!obs_env::ready())
		return;
	Rig rig;
	rig.cam_a.add_filter(obs_env::delay_type, "Delay", 500);
	rig.cam_a.add_filter(obs_env::other_filter_type, "Camera Sync Delay");
	rig.cam_a.add_filter(obs_env::render_delay_type, "Render Delay");
	const Strings user = rig.cam_a.filters();
	REQUIRE(user.size() == 3);

	// ALIGN-79: Cam A at -50 and Cam B at -120 give Cam A a plugin filter at 50, after the user's three.
	REQUIRE(rig.alignment.apply(rig.request(-50, -120), never).kind == OutcomeKind::Applied);
	Strings applied = user;
	applied.push_back(plugin_filter(50));
	CHECK(rig.cam_a.filters() == applied);

	REQUIRE(rig.alignment.undo(never).kind == OutcomeKind::Undone);
	CHECK(rig.cam_a.filters() == user);
}
