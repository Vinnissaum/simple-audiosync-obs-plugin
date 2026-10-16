// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "detector/qr-payload.hpp"
#include "reading/pattern.hpp"

using camera_sync::frame_interval_ms;
using camera_sync::PatternParams;
using camera_sync::QrPayload;
using camera_sync::round_half_away;

// The spec's 25.6 s uses the unrounded q of 66.7 ms. The payload carries q in whole ms, so the test uses 66.
TEST_CASE("pattern half wrap is index_max times 3 q over 2")
{
	CHECK(PatternParams{884, 2, 66, 256}.half_wrap_ns() == 25344000000ull);
	CHECK(PatternParams{884, 2, 100, 128}.half_wrap_ns() == 19200000000ull);
}

TEST_CASE("pattern params come from a qr payload and compare all four fields")
{
	std::string text = "q=66,i=37,f=884,c=2,t=1,I=128";
	QrPayload payload;
	REQUIRE(payload.decode(text.data(), nullptr));

	const PatternParams params = PatternParams::from(payload);
	CHECK(params.f == 884);
	CHECK(params.c == 2);
	CHECK(params.q_ms == 66);
	CHECK(params.index_max == 128);

	const PatternParams same{884, 2, 66, 128};
	CHECK(params == same);
	CHECK_FALSE(params == PatternParams{885, 2, 66, 128});
	CHECK_FALSE(params == PatternParams{884, 3, 66, 128});
	CHECK_FALSE(params == PatternParams{884, 2, 67, 128});
	CHECK_FALSE(params == PatternParams{884, 2, 66, 256});
}

TEST_CASE("round_half_away rounds to the nearest whole ms with halves away from zero")
{
	CHECK(round_half_away(141.5) == 142);
	CHECK(round_half_away(-141.5) == -142);
	CHECK(round_half_away(141.49) == 141);
	CHECK(round_half_away(0.5) == 1);
	CHECK(round_half_away(-0.5) == -1);
	CHECK(round_half_away(0.0) == 0);
	CHECK(round_half_away(-0.49) == 0);
}

TEST_CASE("frame_interval_ms is 1000 fps_den over fps_num")
{
	// Within 0.001 ms, an absolute bound.
	CHECK(std::fabs(frame_interval_ms(30, 1) - 33.333) <= 0.001);
	CHECK(std::fabs(frame_interval_ms(60, 1) - 16.667) <= 0.001);
	CHECK(std::fabs(frame_interval_ms(60000, 1001) - 16.683) <= 0.001);
	CHECK(std::fabs(frame_interval_ms(30000, 1001) - 33.367) <= 0.001);
}
