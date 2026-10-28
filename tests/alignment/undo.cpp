// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "support/alignment-harness.hpp"

using camera_sync::FilterState;
using camera_sync::LogLevel;
using camera_sync::Outcome;
using camera_sync::OutcomeKind;
using camera_sync::SessionState;

namespace {

using Strings = std::vector<std::string>;

const FilterState absent{};
constexpr int64_t ms = AlignmentHarness::ns_per_ms;

FilterState enabled(int64_t delay)
{
	return FilterState{true, delay, true};
}

FilterState disabled(int64_t delay)
{
	return FilterState{true, delay, false};
}

// The first vector applied: Cam B gets a new filter at 60 ms, the mic goes from 0 to 100 ms, Cam A is left.
void apply_first_vector(AlignmentHarness &h)
{
	h.read({100, 40});
	REQUIRE(h.apply().kind == OutcomeKind::Applied);
}

// The hand-set negative vector applied: Cam A 140 and Cam B 190, both new, the mic from -200 ms to 0.
void apply_negative_vector(AlignmentHarness &h)
{
	h.port.offsets["mic"] = -200 * ms;
	h.read({60, 10});
	REQUIRE(h.apply().kind == OutcomeKind::Applied);
}

// The writes made since `from` writes had been made.
Strings writes_since(const AlignmentHarness &h, size_t from)
{
	const Strings all = h.port.write_calls();
	return Strings(all.begin() + static_cast<std::ptrdiff_t>(from), all.end());
}

} // namespace

TEST_CASE("undo with no snapshot makes no port call")
{
	AlignmentHarness h;
	// ALIGN-31: before any Apply.
	h.undo();
	CHECK(h.port.calls.empty());

	// Apply, Undo, Undo: the second Undo has no snapshot.
	apply_first_vector(h);
	REQUIRE(h.undo().kind == OutcomeKind::Undone);
	const size_t calls = h.port.calls.size();
	h.undo();
	CHECK(h.port.calls.size() == calls);
	CHECK(h.alignment.status().last->kind == OutcomeKind::Undone);
}

TEST_CASE("undo refuses and keeps the snapshot when a source is gone or the delay filter is not available")
{
	AlignmentHarness h;
	apply_first_vector(h);
	const size_t writes = h.port.write_calls().size();
	std::string line;
	SUBCASE("Cam A gone")
	{
		h.port.forget("cam-a");
		line = "undo.refused reason=source_gone source=\"cam-a\"";
	}
	SUBCASE("the mic gone")
	{
		h.port.forget("mic");
		line = "undo.refused reason=source_gone source=\"mic\"";
	}
	SUBCASE("no delay filter")
	{
		h.port.filter_available = false;
		line = "undo.refused reason=no_delay_filter";
	}

	// ALIGN-33.
	CHECK(h.undo().kind == OutcomeKind::UndoFailed);
	CHECK(h.port.write_calls().size() == writes);
	CHECK(h.logged(LogLevel::Warning, line));
	CHECK(h.alignment.status().undo_available);
	CHECK(h.alignment.status().last->kind == OutcomeKind::UndoFailed);
}

TEST_CASE("undo reads each snapshot filter and the sync offset before writing")
{
	AlignmentHarness h;
	apply_first_vector(h);
	h.port.calls.clear();

	REQUIRE(h.undo().kind == OutcomeKind::Undone);
	// ALIGN-34.
	Strings reads;
	for (const std::string &call : h.calls_before_first_write())
		if (call.rfind("read_", 0) == 0)
			reads.push_back(call);
	CHECK(reads == Strings{"read_filter cam-a", "read_filter cam-b", "read_offset mic"});
}

TEST_CASE("undo on air asks with the counts it would change and a cancel keeps the snapshot")
{
	AlignmentHarness h;
	apply_first_vector(h);
	const size_t writes = h.port.write_calls().size();
	h.port.live = true;

	// ALIGN-35: one camera and the mic change back.
	CHECK(h.undo(false).kind == OutcomeKind::Cancelled);
	REQUIRE(h.asked.size() == 1);
	CHECK(h.asked[0].cameras == 1);
	CHECK(h.asked[0].mic);
	CHECK(h.port.write_calls().size() == writes);
	CHECK(h.logged(LogLevel::Info, "undo.cancelled"));
	CHECK(h.alignment.status().undo_available);
	CHECK(h.alignment.status().last->kind == OutcomeKind::Applied);

	CHECK(h.undo(true).kind == OutcomeKind::Undone);
	CHECK(h.asked.size() == 2);
	CHECK(h.port.filter("cam-b") == absent);
}

TEST_CASE("undo restores the sync offset to the nanosecond and removes a filter apply created")
{
	AlignmentHarness h;
	apply_negative_vector(h);
	REQUIRE(h.port.filter("cam-a") == enabled(140));
	REQUIRE(h.port.offsets.at("mic") == 0);

	// ALIGN-36.
	CHECK(h.undo().kind == OutcomeKind::Undone);
	CHECK(h.port.offsets.at("mic") == -200000000);
	CHECK(h.port.filter("cam-a") == absent);
	CHECK(h.port.filter("cam-b") == absent);

	// A sub-millisecond offset comes back exactly too.
	AlignmentHarness sub;
	sub.port.offsets["mic"] = -200400123;
	sub.read({60, 10});
	REQUIRE(sub.apply().kind == OutcomeKind::Applied);
	CHECK(sub.undo().kind == OutcomeKind::Undone);
	CHECK(sub.port.offsets.at("mic") == -200400123);
}

TEST_CASE("undo recreates a filter deleted by hand and leaves one the user removed or renamed")
{
	AlignmentHarness h;
	h.port.filters["cam-a"] = disabled(120);
	h.read({100, 40});
	REQUIRE(h.apply().kind == OutcomeKind::Applied); // Cam A unchanged, Cam B created at 60, the mic to 100 ms

	// By hand: Cam A's filter, there before Apply, is deleted; Cam B's, which Apply made, is deleted or renamed, which
	// the port reads the same way.
	h.port.filters.erase("cam-a");
	h.port.filters.erase("cam-b");
	const size_t writes = h.port.write_calls().size();

	// ALIGN-36: Cam A's comes back as it was; Cam B's gets no step.
	CHECK(h.undo().kind == OutcomeKind::Undone);
	CHECK(writes_since(h, writes) == Strings{"write_filter cam-a 120 off", "write_offset mic 0"});
	CHECK(h.port.filter("cam-a") == disabled(120));
	CHECK(h.port.filter("cam-b") == absent);
}

TEST_CASE("undo writes only the values that differ and overwrites hand edits")
{
	SUBCASE("only Cam B and the mic changed")
	{
		AlignmentHarness h;
		apply_first_vector(h);
		const size_t writes = h.port.write_calls().size();
		// ALIGN-37: two steps, Cam A untouched.
		CHECK(h.undo().kind == OutcomeKind::Undone);
		CHECK(writes_since(h, writes) == Strings{"remove_filter cam-b", "write_offset mic 0"});
	}
	SUBCASE("a delay set by hand after Apply")
	{
		AlignmentHarness h;
		h.port.filters["cam-a"] = enabled(10);
		h.read({100, 40});
		REQUIRE(h.apply().kind == OutcomeKind::Applied); // Cam A 10 to 0, Cam B 50, the mic 90 ms
		h.port.filters["cam-a"] = enabled(35);
		h.port.offsets["mic"] = 75 * ms;
		const size_t writes = h.port.write_calls().size();

		CHECK(h.undo().kind == OutcomeKind::Undone);
		CHECK(writes_since(h, writes) ==
		      Strings{"write_filter cam-a 10 on", "remove_filter cam-b", "write_offset mic 0"});
		CHECK(h.port.filter("cam-a") == enabled(10));
	}
}

TEST_CASE("undo that fails a write at each position puts back the values from after apply")
{
	const std::string steps[] = {"camera:\"Cam A\"", "camera:\"Cam B\"", "mic:\"Shure MV7\""};
	for (size_t position = 0; position < 3; position++) {
		for (bool refuse : {true, false}) {
			CAPTURE(position);
			CAPTURE(refuse);
			AlignmentHarness h;
			apply_negative_vector(h);
			if (refuse)
				h.port.refuse_writes = {h.port.writes + position};
			else
				h.port.misread_after = {h.port.writes + position};

			// ALIGN-38.
			CHECK(h.undo().kind == OutcomeKind::UndoFailed);
			CHECK(h.port.filter("cam-a") == enabled(140));
			CHECK(h.port.filter("cam-b") == enabled(190));
			CHECK(h.port.offsets.at("mic") == 0);
			const std::string failed = "undo.result failed step=" + steps[position] + " restored=yes";
			CHECK(h.logged(LogLevel::Warning, failed));
			CHECK(h.alignment.status().undo_available);
			CHECK(h.alignment.status().last->kind == OutcomeKind::UndoFailed);
		}
	}
}

TEST_CASE("undo whose restore fails reports restore failed and drops the snapshot")
{
	AlignmentHarness h;
	apply_negative_vector(h);
	// Undo removes Cam A's filter, removes Cam B's, then the mic's write fails, and putting Cam B's filter back fails.
	h.port.refuse_writes = {h.port.writes + 2, h.port.writes + 3};

	// ALIGN-39.
	CHECK(h.undo().kind == OutcomeKind::UndoRestoreFailed);
	CHECK(h.logged(LogLevel::Warning, "undo.result failed step=mic:\"Shure MV7\" restored=no"));
	CHECK(h.logged(LogLevel::Warning, "restore.failed camera=\"Cam B\" intended_ms=190 actual_ms=none"));
	CHECK(h.port.filter("cam-a") == enabled(140));
	CHECK_FALSE(h.alignment.status().undo_available);
	CHECK_FALSE(h.alignment.status().aligned_after_apply);
	CHECK(h.alignment.status().last->kind == OutcomeKind::UndoRestoreFailed);
}

TEST_CASE("undo that lands drops the snapshot and clears the readings without starting")
{
	AlignmentHarness h;
	h.port.filters["cam-a"] = disabled(120);
	h.port.offsets["mic"] = -200500000; // s rounds to -201 ms
	SUBCASE("measuring") {}
	SUBCASE("stopped")
	{
		h.request.session.state = SessionState::Stopped;
	}
	h.read({60, 10});
	REQUIRE(h.apply().kind == OutcomeKind::Applied); // delta 201: Cam A 141, Cam B 191, the mic 0
	h.measuring.calls.clear();

	// ALIGN-40: the restored values in Apply's order, 0 for a removed filter, the mic rounded half away from zero.
	const Outcome outcome = h.undo();
	CHECK(outcome.kind == OutcomeKind::Undone);
	REQUIRE(outcome.values.size() == 3);
	CHECK(outcome.values[0].name == "Cam A");
	CHECK(outcome.values[0].ms == 120);
	CHECK(outcome.values[1].name == "Cam B");
	CHECK(outcome.values[1].ms == 0);
	CHECK(outcome.values[2].name == "Shure MV7");
	CHECK(outcome.values[2].ms == -201);
	CHECK(h.port.filter("cam-a") == disabled(120));
	CHECK(h.port.offsets.at("mic") == -200500000);
	CHECK_FALSE(h.alignment.status().undo_available);
	CHECK_FALSE(h.alignment.status().aligned_after_apply);
	CHECK(h.alignment.status().last->kind == OutcomeKind::Undone);
	// The session keeps its state: readings cleared, never started.
	CHECK(h.measuring.calls == Strings{"clear"});
}

TEST_CASE("undo logs its plan and result with the undo prefix")
{
	AlignmentHarness h;
	apply_first_vector(h);
	h.lines.clear();
	h.port.on_write = [&] {
		h.now_ns += 3 * ms;
	};

	// ALIGN-75: the new values are the snapshot's.
	REQUIRE(h.undo().kind == OutcomeKind::Undone);
	const std::vector<std::string> info = h.logged(LogLevel::Info);
	REQUIRE(info.size() == 2);
	CHECK(info[0] == "undo.plan camera=\"Cam A\" delay_ms=none->none camera=\"Cam B\" delay_ms=60->none "
			 "mic=\"Shure MV7\" sync_offset_ms=100->0");
	CHECK(info[1] == "undo.result ok written=2 skipped=1 duration_ms=6");
}
