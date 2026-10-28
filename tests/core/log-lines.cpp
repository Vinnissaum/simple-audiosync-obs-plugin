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

namespace {

using camera_sync::FilterState;
using camera_sync::PlanEntry;

const FilterState absent{};
const FilterState disabled_60{true, 60, false};
const FilterState enabled_60{true, 60, true};

} // namespace

TEST_CASE("alignment_plan lists each camera and the mic with old and new values")
{
	using camera_sync::alignment_plan;
	using camera_sync::filter_value;

	// ALIGN-70, the first vector: Cam A has no filter, Cam B a disabled one at 60.
	CHECK(alignment_plan("apply",
			     {PlanEntry{"Cam A", filter_value(absent), "0"},
			      PlanEntry{"Cam B", filter_value(disabled_60), "60"}},
			     PlanEntry{"Shure MV7", "0", "100"}) ==
	      "apply.plan camera=\"Cam A\" delay_ms=none->0 camera=\"Cam B\" delay_ms=60(off)->60 mic=\"Shure MV7\" "
	      "sync_offset_ms=0->100");

	CHECK(filter_value(absent) == "none");
	CHECK(filter_value(disabled_60) == "60(off)");
	CHECK(filter_value(enabled_60) == "60");
	CHECK(filter_value(FilterState{true, 0, true}) == "0");
}

TEST_CASE("alignment_ok counts the written and skipped values and the duration")
{
	CHECK(camera_sync::alignment_ok("apply", 2, 1, 4) == "apply.result ok written=2 skipped=1 duration_ms=4");
	CHECK(camera_sync::alignment_ok("apply", 3, 0, 0) == "apply.result ok written=3 skipped=0 duration_ms=0");
}

TEST_CASE("alignment_failed names the failed step and whether it was restored")
{
	using camera_sync::alignment_failed;
	CHECK(alignment_failed("apply", false, "Cam B", true) == "apply.result failed step=camera:\"Cam B\" restored=yes");
	CHECK(alignment_failed("apply", true, "Shure MV7", false) ==
	      "apply.result failed step=mic:\"Shure MV7\" restored=no");
}

TEST_CASE("restore_failed lines give the intended and actual values")
{
	using camera_sync::filter_value;
	using camera_sync::restore_failed_camera;
	CHECK(restore_failed_camera("Cam B", FilterState{true, 0, true}, enabled_60) ==
	      "restore.failed camera=\"Cam B\" intended_ms=0 actual_ms=60");
	CHECK(restore_failed_camera("Cam B", absent, enabled_60) ==
	      "restore.failed camera=\"Cam B\" intended_ms=none actual_ms=60");
	CHECK(restore_failed_camera("Cam B", disabled_60, absent) ==
	      "restore.failed camera=\"Cam B\" intended_ms=60(off) actual_ms=none");
	CHECK(camera_sync::restore_failed_mic("Shure MV7", -50000000, 0) ==
	      "restore.failed mic=\"Shure MV7\" intended_ns=-50000000 actual_ns=0");
}

TEST_CASE("alignment_refused and alignment_event give each reason and event")
{
	using namespace camera_sync;
	CHECK(alignment_refused_waiting("apply", "Cam A") == "apply.refused reason=waiting camera=\"Cam A\"");
	CHECK(alignment_refused_source_gone("apply", "1b2c3d4e-0000-4000-8000-00000000000a") ==
	      "apply.refused reason=source_gone source=\"1b2c3d4e-0000-4000-8000-00000000000a\"");
	CHECK(alignment_refused_no_delay_filter("apply") == "apply.refused reason=no_delay_filter");
	CHECK(alignment_refused_over_limit("apply", "Cam B", 20001) ==
	      "apply.refused reason=over_limit source=\"Cam B\" value_ms=20001");
	CHECK(alignment_refused_changed("apply") == "apply.refused reason=changed");
	CHECK(alignment_event("apply", "already_aligned") == "apply.already_aligned");
	CHECK(alignment_event("apply", "cancelled") == "apply.cancelled");
}

TEST_CASE("the alignment lines take the undo prefix")
{
	using namespace camera_sync;
	// ALIGN-75: the same lines with undo. in place of apply.
	CHECK(alignment_plan("undo", {PlanEntry{"Cam B", "60", "none"}}, PlanEntry{"Shure MV7", "100", "0"}) ==
	      "undo.plan camera=\"Cam B\" delay_ms=60->none mic=\"Shure MV7\" sync_offset_ms=100->0");
	CHECK(alignment_ok("undo", 2, 1, 3) == "undo.result ok written=2 skipped=1 duration_ms=3");
	CHECK(alignment_failed("undo", false, "Cam B", true) == "undo.result failed step=camera:\"Cam B\" restored=yes");
	CHECK(alignment_refused_source_gone("undo", "cam-b") == "undo.refused reason=source_gone source=\"cam-b\"");
	CHECK(alignment_refused_no_delay_filter("undo") == "undo.refused reason=no_delay_filter");
	CHECK(alignment_refused_changed("undo") == "undo.refused reason=changed");
	CHECK(alignment_event("undo", "cancelled") == "undo.cancelled");
}

TEST_CASE("the alignment lines escape quotes in source names")
{
	using namespace camera_sync;
	// ALIGN-76, READ-88: a camera named Cam "B".
	const std::string name = "Cam \"B\"";
	const std::string quoted = "\"Cam \\\"B\\\"\"";
	CHECK(alignment_plan("apply", {PlanEntry{name, "none", "60"}}, PlanEntry{name, "0", "60"}) ==
	      "apply.plan camera=" + quoted + " delay_ms=none->60 mic=" + quoted + " sync_offset_ms=0->60");
	CHECK(alignment_failed("apply", false, name, true) ==
	      "apply.result failed step=camera:" + quoted + " restored=yes");
	CHECK(restore_failed_camera(name, absent, enabled_60) ==
	      "restore.failed camera=" + quoted + " intended_ms=none actual_ms=60");
	CHECK(restore_failed_mic(name, 0, 1) == "restore.failed mic=" + quoted + " intended_ns=0 actual_ns=1");
	CHECK(alignment_refused_waiting("apply", name) == "apply.refused reason=waiting camera=" + quoted);
	CHECK(alignment_refused_over_limit("apply", name, 20001) ==
	      "apply.refused reason=over_limit source=" + quoted + " value_ms=20001");
}
