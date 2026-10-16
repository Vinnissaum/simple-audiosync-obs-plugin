// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>

#include "log-lines.hpp"

using camera_sync::measure_reset;
using camera_sync::measure_start;
using camera_sync::measure_stop;
using camera_sync::reading_stable;

// Every expected line is the whole text: the "[camera-sync] " prefix is the plugin logger's, not the formatter's.
TEST_CASE("measure_start formats the cameras, the quoted mic and the canvas fps")
{
	CHECK(measure_start(3, "Shure MV7", 60, 1) == "measure.start cameras=3 mic=\"Shure MV7\" canvas_fps=60");

	// The canvas fps is fps_num / fps_den with up to two decimals, rounded, and trailing zeros trimmed.
	CHECK(measure_start(1, "Mic", 30, 1) == "measure.start cameras=1 mic=\"Mic\" canvas_fps=30");
	CHECK(measure_start(1, "Mic", 60000, 1001) == "measure.start cameras=1 mic=\"Mic\" canvas_fps=59.94");
	CHECK(measure_start(1, "Mic", 30000, 1001) == "measure.start cameras=1 mic=\"Mic\" canvas_fps=29.97");
	CHECK(measure_start(1, "Mic", 24000, 1001) == "measure.start cameras=1 mic=\"Mic\" canvas_fps=23.98");
	CHECK(measure_start(1, "Mic", 25, 2) == "measure.start cameras=1 mic=\"Mic\" canvas_fps=12.5");
}

TEST_CASE("measure_stop and measure_reset are the bare event names")
{
	CHECK(measure_stop() == "measure.stop");
	CHECK(measure_reset() == "measure.reset");
}

TEST_CASE("reading_stable formats the quoted camera, the signed reading and the spread")
{
	CHECK(reading_stable("Cam B", 142, 9) == "reading.stable camera=\"Cam B\" reading_ms=+142 spread_ms=9");
	CHECK(reading_stable("Cam B", -60, 9) == "reading.stable camera=\"Cam B\" reading_ms=-60 spread_ms=9");
	CHECK(reading_stable("Cam B", 0, 0) == "reading.stable camera=\"Cam B\" reading_ms=0 spread_ms=0");
}

// The test name carries no quote or backslash, which CTest's test list cannot hold.
TEST_CASE("log lines escape a double quote and a backslash in a source name with a backslash")
{
	CHECK(measure_start(2, "Shure \"MV7\"", 60, 1) ==
	      "measure.start cameras=2 mic=\"Shure \\\"MV7\\\"\" canvas_fps=60");
	CHECK(reading_stable("C:\\Cam \"B\"", 142, 9) ==
	      "reading.stable camera=\"C:\\\\Cam \\\"B\\\"\" reading_ms=+142 spread_ms=9");
	CHECK(camera_sync::quote("a\\\"b") == "\"a\\\\\\\"b\"");
}
