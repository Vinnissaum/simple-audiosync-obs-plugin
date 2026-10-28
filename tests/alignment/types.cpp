// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>

#include "alignment/settings-port.hpp"
#include "alignment/types.hpp"

using camera_sync::FilterState;
using camera_sync::recognised_filter_names;

TEST_CASE("filter state takes its effective delay and compares every field")
{
	// ALIGN-03: a disabled or absent filter delays nothing.
	CHECK(FilterState{}.effective_ms() == 0);
	CHECK(FilterState{false, 60, true}.effective_ms() == 0);
	CHECK(FilterState{true, 60, true}.effective_ms() == 60);
	CHECK(FilterState{true, 60, false}.effective_ms() == 0);

	const FilterState filter{true, 60, true};
	CHECK(filter == FilterState{true, 60, true});
	CHECK_FALSE(filter == FilterState{false, 60, true});
	CHECK_FALSE(filter == FilterState{true, 61, true});
	CHECK_FALSE(filter == FilterState{true, 60, false});
	CHECK(filter != FilterState{true, 60, false});
}

TEST_CASE("the recognised filter names hold only camera sync delay")
{
	// ALIGN-42: the first name is the one a new filter gets.
	REQUIRE(recognised_filter_names.size() == 1);
	CHECK(std::string(recognised_filter_names[0]) == "Camera Sync Delay");
}
