// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include "support/alignment-harness.hpp"

using camera_sync::AlignmentStatus;
using camera_sync::CameraState;
using camera_sync::FilterState;
using camera_sync::OutcomeKind;

namespace {

constexpr int64_t ms = AlignmentHarness::ns_per_ms;

// The kind of the last result, or Waiting when there is none.
OutcomeKind last(const AlignmentHarness &h)
{
	const AlignmentStatus status = h.alignment.status();
	return status.last ? status.last->kind : OutcomeKind::Waiting;
}

} // namespace

TEST_CASE("a failed apply with a clean restore keeps the earlier undo and aligned flag")
{
	AlignmentHarness h;
	h.read({100, 40});
	REQUIRE(h.apply().kind == OutcomeKind::Applied); // Cam B 60, the mic 100 ms

	h.read({50, 30});
	h.port.refuse_writes = {h.port.writes + 1};
	// ALIGN-30.
	CHECK(h.apply().kind == OutcomeKind::ApplyFailed);
	CHECK(h.alignment.status().undo_available);
	CHECK(h.alignment.status().aligned_after_apply);

	// Undo still puts back the values from before the first Apply.
	CHECK(h.undo().kind == OutcomeKind::Undone);
	CHECK(h.port.filter("cam-b") == FilterState{});
	CHECK(h.port.offsets.at("mic") == 0);
}

TEST_CASE("the undo snapshot is dropped by a collection switch and replaced by a later apply")
{
	AlignmentHarness h;
	h.read({100, 40});
	REQUIRE(h.apply().kind == OutcomeKind::Applied); // Cam B 60, the mic 100 ms

	SUBCASE("a collection switch")
	{
		// ALIGN-32: the snapshot, the flag and the last result go; Undo then does nothing.
		h.alignment.collection_changing();
		CHECK_FALSE(h.alignment.status().undo_available);
		CHECK_FALSE(h.alignment.status().aligned_after_apply);
		CHECK_FALSE(h.alignment.status().last);
		const size_t calls = h.port.calls.size();
		h.undo();
		CHECK(h.port.calls.size() == calls);
	}
	SUBCASE("a later Apply")
	{
		h.read({50, 30});
		REQUIRE(h.apply().kind == OutcomeKind::Applied); // Cam B 80, the mic 150 ms
		CHECK(h.undo().kind == OutcomeKind::Undone);
		CHECK(h.port.filter("cam-b") == FilterState{true, 60, true});
		CHECK(h.port.offsets.at("mic") == 100 * ms);
	}
	SUBCASE("a successful Undo")
	{
		REQUIRE(h.undo().kind == OutcomeKind::Undone);
		CHECK_FALSE(h.alignment.status().undo_available);
	}
}

TEST_CASE("reset keeps the undo snapshot and drops the aligned flag and last result")
{
	AlignmentHarness h;
	h.read({100, 40});
	REQUIRE(h.apply().kind == OutcomeKind::Applied);

	h.alignment.reset();
	CHECK(h.alignment.status().undo_available);
	CHECK_FALSE(h.alignment.status().aligned_after_apply);
	CHECK_FALSE(h.alignment.status().last);
	CHECK(h.undo().kind == OutcomeKind::Undone);
}

TEST_CASE("the last result lasts until the next result, reset or collection switch")
{
	// ALIGN-57: every result but "waiting" and "cancelled" replaces the last one.
	AlignmentHarness h;
	CHECK_FALSE(h.alignment.status().last);

	h.read({1, 2});
	h.apply();
	CHECK(last(h) == OutcomeKind::AlreadyAligned);

	h.read({100, 40});
	h.request.session.cameras[0].state = CameraState::Settling;
	h.apply();
	CHECK(last(h) == OutcomeKind::AlreadyAligned);

	h.read({0, -20001});
	h.apply();
	CHECK(last(h) == OutcomeKind::OverLimit);

	h.read({100, 40});
	h.port.filter_available = false;
	h.apply();
	CHECK(last(h) == OutcomeKind::ApplyFailed);
	h.port.filter_available = true;

	h.apply();
	CHECK(last(h) == OutcomeKind::Applied);
	CHECK(h.alignment.status().last->values.size() == 3);

	h.port.live = true;
	h.read({50, 30});
	h.apply(false);
	CHECK(last(h) == OutcomeKind::Applied);
	h.undo(false);
	CHECK(last(h) == OutcomeKind::Applied);
	h.port.live = false;

	h.undo();
	CHECK(last(h) == OutcomeKind::Undone);

	h.alignment.reset();
	CHECK_FALSE(h.alignment.status().last);

	h.read({100, 40});
	h.apply();
	CHECK(last(h) == OutcomeKind::Applied);
	h.alignment.collection_changing();
	CHECK_FALSE(h.alignment.status().last);
}
