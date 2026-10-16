// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "viewmodel/view-model.hpp"

using camera_sync::build_view;
using camera_sync::CameraRow;
using camera_sync::CameraRowView;
using camera_sync::CameraState;
using camera_sync::Choice;
using camera_sync::DockView;
using camera_sync::format_signed_ms;
using camera_sync::SessionSnapshot;
using camera_sync::SourceInfo;

namespace {

// The en-US text the spec writes out, so a row is compared with the words the user reads.
std::string en_us(const char *key)
{
	static const std::map<std::string, std::string> text = {
		{"CameraSync.Row.Looking", "Looking for the pattern"},
		{"CameraSync.Row.Settling", "{value} ms - settling"},
		{"CameraSync.Row.VideoLate", "{value} ms - video late"},
		{"CameraSync.Row.VideoEarly", "{value} ms - video early"},
		{"CameraSync.Row.InSync", "0 ms - in sync"},
	};
	const auto found = text.find(key);
	return found == text.end() ? std::string(key) : found->second;
}

const std::string minus = "\xE2\x88\x92"; // U+2212

SourceInfo camera(const char *id, const char *name)
{
	return SourceInfo{id, name, true, false, true};
}

CameraRow row(const char *id, CameraState state, int64_t live_ms = 0, int64_t reading_ms = 0)
{
	CameraRow result;
	result.id = id;
	result.state = state;
	result.live_ms = live_ms;
	result.reading_ms = reading_ms;
	return result;
}

// One ticked camera "cam" measured as `measured`: its row's status.
std::string status_of(const CameraRow &measured)
{
	SessionSnapshot snapshot;
	snapshot.cameras = {measured};
	const DockView view = build_view(snapshot, {camera("cam", "Cam")}, Choice{{"cam"}, std::nullopt}, false, en_us);
	REQUIRE(view.cameras.size() == 1);
	return view.cameras[0].status;
}

} // namespace

TEST_CASE("format signed ms writes the sign the dock shows")
{
	CHECK(format_signed_ms(142) == "+142");
	CHECK(format_signed_ms(-60) == minus + "60");
	CHECK(format_signed_ms(0) == "0");
	// The minus is U+2212, bytes E2 88 92, and not the ASCII hyphen.
	CHECK(format_signed_ms(-60).substr(0, 3) == "\xE2\x88\x92");
	CHECK(format_signed_ms(-60).find('-') == std::string::npos);
}

TEST_CASE("a camera that is looking reads looking for the pattern")
{
	CHECK(status_of(row("cam", CameraState::Looking)) == "Looking for the pattern");
}

TEST_CASE("a settling camera shows its live value and not its reading")
{
	CHECK(status_of(row("cam", CameraState::Settling, 142, 7)) == "+142 ms - settling");

	// The spec asks for an explicit sign: a negative live value takes the minus sign, as a reading does.
	CHECK(status_of(row("cam", CameraState::Settling, -60, 7)) == minus + "60 ms - settling");
	CHECK(status_of(row("cam", CameraState::Settling, 0, 7)) == "0 ms - settling");
}

TEST_CASE("a stable camera above zero reads video late")
{
	CHECK(status_of(row("cam", CameraState::Stable, 99, 142)) == "+142 ms - video late");
}

TEST_CASE("a stable camera below zero reads video early with the minus sign")
{
	CHECK(status_of(row("cam", CameraState::Stable, 99, -60)) == minus + "60 ms - video early");
}

TEST_CASE("a stable camera at zero reads in sync")
{
	CHECK(status_of(row("cam", CameraState::Stable, 99, 0)) == "0 ms - in sync");
}

TEST_CASE("an unticked camera shows no status even when the session holds a row for it")
{
	SessionSnapshot snapshot;
	snapshot.cameras = {row("a", CameraState::Stable, 0, 142), row("b", CameraState::Stable, 0, 142)};
	const DockView view = build_view(snapshot, {camera("a", "Cam A"), camera("b", "Cam B")},
					 Choice{{"a"}, std::nullopt}, false, en_us);

	REQUIRE(view.cameras.size() == 2);
	CHECK(view.cameras[0].ticked);
	CHECK(view.cameras[0].status == "+142 ms - video late");
	CHECK_FALSE(view.cameras[1].ticked);
	CHECK(view.cameras[1].status == "");
}

TEST_CASE("a ticked camera the session dropped shows no status")
{
	SessionSnapshot snapshot;
	snapshot.cameras = {row("b", CameraState::Looking)};
	const DockView view = build_view(snapshot, {camera("a", "Cam A"), camera("b", "Cam B")},
					 Choice{{"a", "b"}, std::nullopt}, false, en_us);

	REQUIRE(view.cameras.size() == 2);
	CHECK(view.cameras[0].status == "");
	CHECK(view.cameras[1].status == "Looking for the pattern");
}

TEST_CASE("rows come in name order and the chosen mic is not one")
{
	SourceInfo mic = camera("mic", "A Mic");
	mic.audio = true;
	const DockView view = build_view(SessionSnapshot{}, {camera("c", "Cam C"), mic, camera("b", "cam b")},
					 Choice{{}, std::string("mic")}, false, en_us);

	REQUIRE(view.cameras.size() == 2);
	CHECK(view.cameras[0].name == "cam b");
	CHECK(view.cameras[0].id == "b");
	CHECK(view.cameras[1].name == "Cam C");
}
