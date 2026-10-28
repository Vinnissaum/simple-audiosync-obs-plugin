// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "alignment/alignment.hpp"

using camera_sync::AlignmentRequest;
using camera_sync::Choice;
using camera_sync::FrameRate;
using camera_sync::request_for;
using camera_sync::SessionSnapshot;
using camera_sync::SourceInfo;

TEST_CASE("the request lists the ticked cameras in checklist order with their names")
{
	// The checklist sorts by name ignoring case; the choice keeps the order the cameras were ticked in.
	std::vector<SourceInfo> sources;
	sources.push_back(SourceInfo{"c", "Cam C", true, false, true});
	sources.push_back(SourceInfo{"b", "cam b", true, false, true});
	sources.push_back(SourceInfo{"a", "Brio", true, false, true});
	sources.push_back(SourceInfo{"x", "Not ticked", true, false, true});
	sources.push_back(SourceInfo{"mic", "Shure MV7", false, true, false});
	SessionSnapshot session;
	session.frame_interval_ms = 33.333;

	const AlignmentRequest request =
		request_for(sources, Choice{{"c", "a", "b"}, std::string("mic")}, session, FrameRate{60000, 1001});
	REQUIRE(request.cameras.size() == 3);
	CHECK(request.cameras[0].id == "a");
	CHECK(request.cameras[0].name == "Brio");
	CHECK(request.cameras[1].id == "b");
	CHECK(request.cameras[1].name == "cam b");
	CHECK(request.cameras[2].id == "c");
	REQUIRE(request.mic);
	CHECK(request.mic->id == "mic");
	CHECK(request.mic->name == "Shure MV7");
	CHECK(request.session.frame_interval_ms == 33.333);
	CHECK(request.rate.fps_num == 60000);
	CHECK(request.rate.fps_den == 1001);

	// No mic chosen: none in the request.
	CHECK_FALSE(request_for(sources, Choice{{"a"}, std::nullopt}, session, FrameRate{}).mic);
}
