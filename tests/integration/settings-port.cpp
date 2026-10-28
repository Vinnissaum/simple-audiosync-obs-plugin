// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "obs-env.hpp"
#include "obs/obs-settings-port.hpp"

using camera_sync::FilterState;
using camera_sync::ObsSettingsPort;
using obs_env::Source;

namespace {

using Strings = std::vector<std::string>;

bool off_air()
{
	return false;
}

// The way Source::filters writes a filter.
std::string filter(const char *type, const char *name, const std::string &settings, bool enabled = true)
{
	return std::string(type) + " " + name + " " + settings + (enabled ? " on" : " off");
}

std::string delay(int64_t ms)
{
	return "{\"delay_ms\":" + std::to_string(ms) + "}";
}

// A camera carrying, in order, a user's Video Delay (Async) named "Delay", a filter of another type named "Camera
// Sync Delay", and a Render Delay.
void add_user_filters(Source &camera)
{
	camera.add_filter(obs_env::delay_type, "Delay", 500);
	camera.add_filter(obs_env::other_filter_type, "Camera Sync Delay");
	camera.add_filter(obs_env::render_delay_type, "Render Delay");
}

} // namespace

TEST_CASE("libobs starts with video delay async registered")
{
	if (!obs_env::ready())
		return;
	CHECK(obs_source_get_display_name("async_delay_filter") != nullptr);

	Source camera(obs_env::camera_type, "Smoke camera");
	ObsSettingsPort port(off_air);
	CHECK(port.delay_filter_available());
	REQUIRE(port.write_filter(camera.uuid(), 60, true));
	CHECK(port.read_filter(camera.uuid()) == FilterState{true, 60, true});
	CHECK(camera.filters() == Strings{filter("async_delay_filter", "Camera Sync Delay", delay(60))});
}

TEST_CASE("settings port creates the plugin filter last in the chain and updates it in place")
{
	if (!obs_env::ready())
		return;
	Source camera(obs_env::camera_type, "Create camera");
	camera.add_filter(obs_env::render_delay_type, "Render Delay");
	const std::string render = camera.filters().at(0);
	ObsSettingsPort port(off_air);

	// ALIGN-21, ALIGN-44: created after every filter already there, enabled, with delay_ms.
	CHECK(port.read_filter(camera.uuid()) == FilterState{});
	REQUIRE(port.write_filter(camera.uuid(), 60, true));
	CHECK(camera.filters() == Strings{render, filter("async_delay_filter", "Camera Sync Delay", delay(60))});

	// Written again, the same filter changes: no second one.
	REQUIRE(port.write_filter(camera.uuid(), 80, false));
	CHECK(camera.filters() == Strings{render, filter("async_delay_filter", "Camera Sync Delay", delay(80), false)});
	CHECK(port.read_filter(camera.uuid()) == FilterState{true, 80, false});

	// Removed: only the plugin's filter goes.
	REQUIRE(port.remove_filter(camera.uuid()));
	CHECK(camera.filters() == Strings{render});
	CHECK(port.read_filter(camera.uuid()) == FilterState{});
	// Removing it again is no failure.
	CHECK(port.remove_filter(camera.uuid()));
}

TEST_CASE("settings port answers an unknown uuid with nothing")
{
	if (!obs_env::ready())
		return;
	ObsSettingsPort port(off_air);
	const std::string nobody = "00000000-0000-4000-8000-000000000000";

	// ALIGN-41.
	CHECK_FALSE(port.resolves(nobody));
	CHECK_FALSE(port.read_filter(nobody));
	CHECK_FALSE(port.write_filter(nobody, 60, true));
	CHECK_FALSE(port.remove_filter(nobody));
	CHECK_FALSE(port.read_sync_offset_ns(nobody));
	CHECK_FALSE(port.write_sync_offset_ns(nobody, 1));

	Source camera(obs_env::camera_type, "Known camera");
	CHECK(port.resolves(camera.uuid()));
}

TEST_CASE("settings port takes the first plugin filter in chain order")
{
	if (!obs_env::ready())
		return;
	ObsSettingsPort port(off_air);

	SUBCASE("two plugin filters: the first is read and written")
	{
		Source camera(obs_env::camera_type, "Two filters camera");
		camera.add_filter(obs_env::delay_type, "Camera Sync Delay", 10);
		camera.add_filter(obs_env::delay_type, "Camera Sync Delay", 20);
		CHECK(port.read_filter(camera.uuid()) == FilterState{true, 10, true});
		REQUIRE(port.write_filter(camera.uuid(), 30, true));
		CHECK(camera.filters() == Strings{filter("async_delay_filter", "Camera Sync Delay", delay(30)),
						  filter("async_delay_filter", "Camera Sync Delay", delay(20))});
	}
	SUBCASE("only another type carries the name: no plugin filter, and a write adds one after it")
	{
		Source camera(obs_env::camera_type, "Other type camera");
		camera.add_filter(obs_env::other_filter_type, "Camera Sync Delay");
		const std::string other = camera.filters().at(0);
		CHECK(port.read_filter(camera.uuid()) == FilterState{});
		REQUIRE(port.write_filter(camera.uuid(), 40, true));
		CHECK(camera.filters() == Strings{other, filter("async_delay_filter", "Camera Sync Delay", delay(40))});
	}
}

TEST_CASE("settings port leaves the user's filters unchanged and in order")
{
	if (!obs_env::ready())
		return;
	Source camera(obs_env::camera_type, "User filters camera");
	add_user_filters(camera);
	const Strings user = camera.filters();
	REQUIRE(user.size() == 3);
	ObsSettingsPort port(off_air);

	// ALIGN-42, ALIGN-43: none of the three is the plugin's filter.
	CHECK(port.read_filter(camera.uuid()) == FilterState{});
	REQUIRE(port.write_filter(camera.uuid(), 60, true));
	Strings after = user;
	after.push_back(filter("async_delay_filter", "Camera Sync Delay", delay(60)));
	CHECK(camera.filters() == after);

	REQUIRE(port.remove_filter(camera.uuid()));
	CHECK(camera.filters() == user);
}

TEST_CASE("settings port writes delay_ms and the sync offset exactly")
{
	if (!obs_env::ready())
		return;
	Source camera(obs_env::camera_type, "Exact camera");
	Source mic(obs_env::mic_type, "Exact mic");
	ObsSettingsPort port(off_air);

	// ALIGN-44: delay_ms at both ends of its range, the offset to the nanosecond.
	REQUIRE(port.write_filter(camera.uuid(), 0, true));
	CHECK(port.read_filter(camera.uuid()) == FilterState{true, 0, true});
	REQUIRE(port.write_filter(camera.uuid(), 20000, true));
	CHECK(port.read_filter(camera.uuid()) == FilterState{true, 20000, true});

	for (int64_t ns : {int64_t(-200000000), int64_t(123456789), int64_t(0)}) {
		REQUIRE(port.write_sync_offset_ns(mic.uuid(), ns));
		CHECK(port.read_sync_offset_ns(mic.uuid()) == ns);
		CHECK(obs_source_get_sync_offset(mic.get()) == ns);
	}
}

TEST_CASE("settings port reports on air from the injected query")
{
	// ALIGN-45: the frontend's query is handed in; the port only asks it.
	bool live = false;
	ObsSettingsPort port([&live] { return live; });
	CHECK_FALSE(port.on_air());
	live = true;
	CHECK(port.on_air());
	CHECK_FALSE(ObsSettingsPort(nullptr).on_air());
}
