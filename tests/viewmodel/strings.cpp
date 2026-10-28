// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <fstream>
#include <map>
#include <string>

#include "viewmodel/strings.hpp"

using camera_sync::strings::fill;

namespace {

// The ini's "Key=\"value\"" lines. Blank lines and comments are skipped.
std::map<std::string, std::string> read_locale()
{
	std::ifstream file(CAMERA_SYNC_LOCALE_FILE);
	REQUIRE_MESSAGE(file.is_open(), "cannot open " CAMERA_SYNC_LOCALE_FILE);
	std::map<std::string, std::string> entries;
	std::string line;
	while (std::getline(file, line)) {
		const size_t equals = line.find('=');
		if (equals == std::string::npos || line[0] == '#' || line[0] == ';')
			continue;
		std::string value = line.substr(equals + 1);
		if (!value.empty() && value.back() == '\r')
			value.pop_back();
		REQUIRE(value.size() >= 2);
		REQUIRE(value.front() == '"');
		REQUIRE(value.back() == '"');
		entries[line.substr(0, equals)] = value.substr(1, value.size() - 2);
	}
	return entries;
}

} // namespace

TEST_CASE("locale file and the view-model's keys list the same strings")
{
	const std::map<std::string, std::string> locale = read_locale();

	SUBCASE("every key the view-model uses is in the file")
	{
		for (const char *key : camera_sync::strings::all_keys)
			CHECK_MESSAGE(locale.count(key) == 1, "missing from the ini: ", key);
	}

	SUBCASE("every CameraSync key in the file except the dock title is a key the view-model uses")
	{
		for (const auto &entry : locale) {
			if (entry.first.rfind("CameraSync.", 0) != 0 || entry.first == "CameraSync.Dock.Title")
				continue;
			bool used = false;
			for (const char *key : camera_sync::strings::all_keys)
				used = used || entry.first == key;
			CHECK_MESSAGE(used, "unused by the view-model: ", entry.first);
		}
	}
}

TEST_CASE("locale file holds the design's en-US text with a plain hyphen")
{
	const std::map<std::string, std::string> locale = read_locale();
	const std::map<std::string, std::string> expected = {
		{"CameraSync.Row.Looking", "Looking for the pattern"},
		{"CameraSync.Row.Settling", "{value} ms - settling"},
		{"CameraSync.Row.VideoLate", "{value} ms - video late"},
		{"CameraSync.Row.VideoEarly", "{value} ms - video early"},
		{"CameraSync.Row.InSync", "0 ms - in sync"},
		{"CameraSync.Row.Details", "received {n}, missed {m}"},
		{"CameraSync.Message.Pick", "Pick at least one camera and the mic"},
		{"CameraSync.Message.NoSound", "No pattern sound from {mic} - bring the phone closer or unmute it"},
		{"CameraSync.Message.MicTap", "Could not listen to {mic}"},
		{"CameraSync.Button.Start", "Start"},
		{"CameraSync.Button.Stop", "Stop"},
		{"CameraSync.Button.Reset", "Reset"},
		{"CameraSync.Label.Mic", "Mic"},
		{"CameraSync.Label.Pattern", "Pattern files"},
		{"CameraSync.Label.Details", "Details"},
	};

	for (const auto &entry : expected) {
		const auto found = locale.find(entry.first);
		REQUIRE_MESSAGE(found != locale.end(), "missing from the ini: ", entry.first);
		CHECK(found->second == entry.second);
	}

	// The minus sign U+2212 (bytes E2 88 92) is formatted by the code and never written in the ini.
	for (const auto &entry : locale)
		CHECK_MESSAGE(entry.second.find("\xE2\x88\x92") == std::string::npos, "U+2212 in ", entry.first);
}

TEST_CASE("fill replaces named placeholders once and leaves the rest")
{
	SUBCASE("a named placeholder is replaced")
	{
		CHECK(fill("{value} ms - settling", {{"value", "+142"}}) == "+142 ms - settling");
	}

	SUBCASE("several placeholders are replaced wherever they stand")
	{
		CHECK(fill("received {n}, missed {m}", {{"m", "2"}, {"n", "12"}}) == "received 12, missed 2");
	}

	SUBCASE("an unknown placeholder is left as written")
	{
		CHECK(fill("No pattern sound from {mic} {other}", {{"mic", "Shure MV7"}}) ==
		      "No pattern sound from Shure MV7 {other}");
	}

	SUBCASE("a value containing a brace is not expanded again")
	{
		CHECK(fill("{a} and {b}", {{"a", "{b}"}, {"b", "x"}}) == "{b} and x");
		CHECK(fill("{name}", {{"name", "{name}"}}) == "{name}");
	}
}

TEST_CASE("locale file holds the en-US text of every alignment string")
{
	// ALIGN-63: the plan's text, character for character; the check mark is U+2713 (bytes E2 9C 93).
	const std::map<std::string, std::string> locale = read_locale();
	const std::map<std::string, std::string> expected = {
		{"CameraSync.Button.Apply", "Apply"},
		{"CameraSync.Button.Undo", "Undo"},
		{"CameraSync.Button.Cancel", "Cancel"},
		{"CameraSync.Row.Aligned", "\xE2\x9C\x93 {status}"},
		{"CameraSync.Message.Waiting", "Waiting for {camera}"},
		{"CameraSync.Message.AlreadyAligned", "Already aligned"},
		{"CameraSync.Message.OverLimit", "{camera} needs more delay than OBS allows"},
		{"CameraSync.Message.ApplyFailed", "Apply failed - nothing changed"},
		{"CameraSync.Message.ApplyRestoreFailed", "Apply failed - check the log"},
		{"CameraSync.Message.UndoFailed", "Undo failed - nothing changed"},
		{"CameraSync.Message.UndoRestoreFailed", "Undo failed - check the log"},
		{"CameraSync.Message.Applied", "Applied - {values}"},
		{"CameraSync.Message.Undone", "Undone - {values}"},
		{"CameraSync.Message.Value", "{name} {value} ms"},
		{"CameraSync.Message.Aligned", "Aligned"},
		{"CameraSync.Question.CamerasAndMic", "Shift {n} cameras and the mic while on air?"},
		{"CameraSync.Question.CameraAndMic", "Shift 1 camera and the mic while on air?"},
		{"CameraSync.Question.Cameras", "Shift {n} cameras while on air?"},
		{"CameraSync.Question.Camera", "Shift 1 camera while on air?"},
		{"CameraSync.Question.Mic", "Shift the mic while on air?"},
		{"CameraSync.Question.Pause", "Cameras whose delay changes pause until their new delay fills."},
	};

	for (const auto &entry : expected) {
		const auto found = locale.find(entry.first);
		REQUIRE_MESSAGE(found != locale.end(), "missing from the ini: ", entry.first);
		CHECK(found->second == entry.second);
	}
}
