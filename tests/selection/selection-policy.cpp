// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "selection/selection-policy.hpp"

using camera_sync::camera_list;
using camera_sync::Choice;
using camera_sync::initial_choice;
using camera_sync::mic_list;
using camera_sync::SavedSelection;
using camera_sync::SourceId;
using camera_sync::SourceInfo;
using camera_sync::to_saved;

namespace {

enum Kind { Camera = 1, Audio = 2, CameraWithAudio = 3, Other = 0 };

SourceInfo source(const char *id, const char *name, int kind, bool in_program = false)
{
	return SourceInfo{id, name, (kind & Camera) != 0, (kind & Audio) != 0, in_program};
}

std::vector<std::string> names_of(const std::vector<SourceInfo> &list)
{
	std::vector<std::string> names;
	for (const SourceInfo &info : list)
		names.push_back(info.name);
	return names;
}

std::vector<SourceId> sorted(std::vector<SourceId> ids)
{
	std::sort(ids.begin(), ids.end());
	return ids;
}

// A collection with three cameras in Program (one inside a nested scene and one inside a group, which the catalog marks
// in_program), one camera off Program, a screen capture, Desktop Audio and a USB mic.
std::vector<SourceInfo> studio()
{
	return {
		source("u-cam-c", "Cam C", Camera, true),
		source("u-cam-b", "cam b", Camera, true),            // inside a nested scene
		source("u-cam-group", "Cam in group", Camera, true), // inside a group
		source("u-cam-off", "Cam off air", Camera, false),
		source("u-screen", "Screen", Other, true),
		source("u-desktop", "Desktop Audio", Audio),
		source("u-mic", "Shure MV7", Audio),
	};
}

} // namespace

TEST_CASE("camera_list returns the async video sources sorted by name ignoring case")
{
	const std::vector<SourceInfo> list = camera_list(studio(), std::nullopt);

	CHECK(names_of(list) == std::vector<std::string>{"cam b", "Cam C", "Cam in group", "Cam off air"});
}

TEST_CASE("mic_list returns the sources with audio sorted by name ignoring case")
{
	std::vector<SourceInfo> sources = studio();
	sources.push_back(source("u-zoom", "zoom H5", Audio));
	sources.push_back(source("u-webcam", "Brio", CameraWithAudio));

	const std::vector<SourceInfo> list = mic_list(sources, {});

	CHECK(names_of(list) == std::vector<std::string>{"Brio", "Desktop Audio", "Shure MV7", "zoom H5"});
}

TEST_CASE("initial_choice with no saved selection ticks every camera in the Program scene")
{
	const Choice choice = initial_choice(studio(), std::nullopt);

	// The cameras in nested scenes and groups count; the camera off Program and the screen capture do not.
	CHECK(sorted(choice.cameras) == std::vector<SourceId>{"u-cam-b", "u-cam-c", "u-cam-group"});
}

TEST_CASE("initial_choice with no saved selection chooses the mic when exactly one source has audio")
{
	const std::vector<SourceInfo> sources = {
		source("u-cam-a", "Cam A", Camera, true),
		source("u-mic", "Shure MV7", Audio),
	};

	CHECK(initial_choice(sources, std::nullopt).mic == std::optional<SourceId>("u-mic"));
}

TEST_CASE("initial_choice with no saved selection leaves the mic unset unless exactly one source has audio")
{
	std::vector<SourceInfo> sources = {source("u-cam-a", "Cam A", Camera, true)};

	SUBCASE("no source with audio") {}
	SUBCASE("two sources with audio")
	{
		sources.push_back(source("u-desktop", "Desktop Audio", Audio));
		sources.push_back(source("u-mic", "Shure MV7", Audio));
	}
	SUBCASE("a mic and a camera with its own audio")
	{
		sources.push_back(source("u-mic", "Shure MV7", Audio));
		sources.push_back(source("u-webcam", "Brio", CameraWithAudio, false));
	}

	CHECK_FALSE(initial_choice(sources, std::nullopt).mic.has_value());
}

TEST_CASE("initial_choice with a version 1 selection ticks and chooses only the sources that still exist")
{
	const SavedSelection saved{1, {"u-cam-off", "u-gone", "u-cam-b"}, SourceId("u-desktop")};

	const Choice choice = initial_choice(studio(), saved);

	// Nothing is preselected: the Program cameras the selection does not name stay unticked, and the mic is the saved
	// one although two sources have audio.
	CHECK(sorted(choice.cameras) == std::vector<SourceId>{"u-cam-b", "u-cam-off"});
	CHECK(choice.mic == std::optional<SourceId>("u-desktop"));
}

TEST_CASE("initial_choice with a version 1 selection whose sources are all gone preselects nothing")
{
	const std::vector<SourceInfo> sources = {
		source("u-cam-a", "Cam A", Camera, true),
		source("u-mic", "Shure MV7", Audio),
	};

	SUBCASE("saved cameras and mic gone")
	{
		const Choice choice = initial_choice(sources, SavedSelection{1, {"u-gone"}, SourceId("u-gone-mic")});
		CHECK(choice.cameras.empty());
		CHECK_FALSE(choice.mic.has_value());
	}
	SUBCASE("nothing saved")
	{
		const Choice choice = initial_choice(sources, SavedSelection{1, {}, std::nullopt});
		CHECK(choice.cameras.empty());
		CHECK_FALSE(choice.mic.has_value());
	}
}

TEST_CASE("initial_choice treats a selection of another version, or none, as no saved selection")
{
	const std::vector<SourceInfo> sources = {
		source("u-cam-a", "Cam A", Camera, true),
		source("u-cam-b", "Cam B", Camera, false),
		source("u-mic", "Shure MV7", Audio),
	};
	std::optional<SavedSelection> saved;

	SUBCASE("version 0")
	{
		saved = SavedSelection{0, {"u-cam-b"}, std::nullopt};
	}
	SUBCASE("version 2")
	{
		saved = SavedSelection{2, {"u-cam-b"}, std::nullopt};
	}
	SUBCASE("absent") {}

	const Choice choice = initial_choice(sources, saved);
	CHECK(choice.cameras == std::vector<SourceId>{"u-cam-a"});
	CHECK(choice.mic == std::optional<SourceId>("u-mic"));
}

TEST_CASE("mic_list leaves out the ticked cameras and camera_list leaves out the chosen mic")
{
	const std::vector<SourceInfo> sources = {
		source("u-brio", "Brio", CameraWithAudio, true),
		source("u-cam-b", "Cam B", Camera, true),
		source("u-mic", "Shure MV7", Audio),
		source("u-webcam", "Webcam", CameraWithAudio, false),
	};

	CHECK(names_of(mic_list(sources, {"u-brio"})) == std::vector<std::string>{"Shure MV7", "Webcam"});
	CHECK(names_of(mic_list(sources, {})) == std::vector<std::string>{"Brio", "Shure MV7", "Webcam"});
	CHECK(names_of(camera_list(sources, SourceId("u-webcam"))) == std::vector<std::string>{"Brio", "Cam B"});
	CHECK(names_of(camera_list(sources, std::nullopt)) == std::vector<std::string>{"Brio", "Cam B", "Webcam"});
}

TEST_CASE("initial_choice does not choose a camera it ticks as the mic")
{
	// The only source with audio is a camera in Program: it is ticked, so it is not also the mic (READ-60).
	const std::vector<SourceInfo> sources = {
		source("u-brio", "Brio", CameraWithAudio, true),
		source("u-cam-b", "Cam B", Camera, true),
	};

	const Choice choice = initial_choice(sources, std::nullopt);

	CHECK(sorted(choice.cameras) == std::vector<SourceId>{"u-brio", "u-cam-b"});
	CHECK_FALSE(choice.mic.has_value());
}

TEST_CASE("to_saved writes version 1 with the camera UUIDs and the mic or null")
{
	SUBCASE("with a mic")
	{
		const SavedSelection saved = to_saved(Choice{{"u-cam-a", "u-cam-b"}, SourceId("u-mic")});
		CHECK(saved.version == 1);
		CHECK(saved.cameras == std::vector<SourceId>{"u-cam-a", "u-cam-b"});
		CHECK(saved.mic == std::optional<SourceId>("u-mic"));
	}
	SUBCASE("without a mic")
	{
		const SavedSelection saved = to_saved(Choice{{"u-cam-a"}, std::nullopt});
		CHECK(saved.version == 1);
		CHECK(saved.cameras == std::vector<SourceId>{"u-cam-a"});
		CHECK_FALSE(saved.mic.has_value());
	}
}
