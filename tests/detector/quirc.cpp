// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <quirc.h>

TEST_CASE("quirc creates a decoder, sizes it to 640x360 and destroys it")
{
	struct quirc *decoder = quirc_new();
	REQUIRE(decoder != nullptr);

	CHECK(quirc_resize(decoder, 640, 360) == 0);

	int width = 0;
	int height = 0;
	CHECK(quirc_begin(decoder, &width, &height) != nullptr);
	CHECK(width == 640);
	CHECK(height == 360);

	quirc_destroy(decoder);
}
