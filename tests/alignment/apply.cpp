// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "support/alignment-harness.hpp"

using camera_sync::CameraState;
using camera_sync::Counts;
using camera_sync::FilterState;
using camera_sync::LogLevel;
using camera_sync::Outcome;
using camera_sync::OutcomeKind;
using camera_sync::SessionState;

namespace {

using Strings = std::vector<std::string>;

const FilterState absent{};

FilterState enabled(int64_t ms)
{
	return FilterState{true, ms, true};
}

FilterState disabled(int64_t ms)
{
	return FilterState{true, ms, false};
}

bool same_counts(const Counts &a, const Counts &b)
{
	return a.cameras == b.cameras && a.mic == b.mic;
}

size_t index_of(const Strings &calls, const std::string &call)
{
	return static_cast<size_t>(std::find(calls.begin(), calls.end(), call) - calls.begin());
}

size_t first_write(const Strings &calls)
{
	for (size_t i = 0; i < calls.size(); i++)
		if (calls[i].rfind("write_", 0) == 0 || calls[i].rfind("remove_", 0) == 0)
			return i;
	return calls.size();
}

} // namespace

TEST_CASE("apply with no mic, no camera or a camera not stable makes no port call")
{
	AlignmentHarness h;
	h.read({100, 40});
	std::string named;
	SUBCASE("no mic")
	{
		h.request.mic.reset();
	}
	SUBCASE("no camera")
	{
		h.request.cameras.clear();
		h.request.session.cameras.clear();
	}
	SUBCASE("Cam B Settling")
	{
		h.request.session.cameras[1].state = CameraState::Settling;
		named = "Cam B";
	}
	SUBCASE("Cam A Looking, as when the session has no row for it")
	{
		h.request.session.cameras.erase(h.request.session.cameras.begin());
		named = "Cam A";
	}

	// ALIGN-11: nothing resolved, read or written; "waiting" names the first camera that is not Stable.
	const Outcome outcome = h.apply();
	CHECK(outcome.kind == OutcomeKind::Waiting);
	CHECK(outcome.source == named);
	CHECK(h.port.calls.empty());
	CHECK(h.asked.empty());
	CHECK(h.measuring.calls.empty());
	CHECK_FALSE(h.alignment.status().last);
}

TEST_CASE("apply when already aligned makes no port call and reports it")
{
	AlignmentHarness h;
	h.port.offsets["mic"] = -200 * AlignmentHarness::ns_per_ms;
	h.read({20, -30});

	// ALIGN-12: decided before anything is read, so a hand-set negative offset stays.
	const Outcome outcome = h.apply();
	CHECK(outcome.kind == OutcomeKind::AlreadyAligned);
	CHECK(h.port.calls.empty());
	CHECK(h.port.offsets.at("mic") == -200 * AlignmentHarness::ns_per_ms);
	REQUIRE(h.alignment.status().last);
	CHECK(h.alignment.status().last->kind == OutcomeKind::AlreadyAligned);
}

TEST_CASE("apply refuses with nothing written when a camera or the mic is gone")
{
	AlignmentHarness h;
	h.read({100, 40});
	std::string gone;
	SUBCASE("Cam B")
	{
		gone = "cam-b";
	}
	SUBCASE("the mic")
	{
		gone = "mic";
	}
	h.port.forget(gone);

	// ALIGN-13.
	CHECK(h.apply().kind == OutcomeKind::ApplyFailed);
	CHECK(h.port.write_calls().empty());
	CHECK(h.logged(LogLevel::Warning, "apply.refused reason=source_gone source=\"" + gone + "\""));
	CHECK(h.alignment.status().last->kind == OutcomeKind::ApplyFailed);
	CHECK_FALSE(h.alignment.status().undo_available);
}

TEST_CASE("apply refuses with nothing written when the delay filter is not available")
{
	AlignmentHarness h;
	h.port.filter_available = false;
	SUBCASE("a plan that creates Cam B's filter")
	{
		h.read({100, 40});
	}
	SUBCASE("a plan that only changes Cam B's existing filter")
	{
		h.port.filters["cam-b"] = enabled(10);
		h.read({100, 50});
	}

	// ALIGN-14: a filter of an unloaded type delays nothing, so even an existing one is not written.
	CHECK(h.apply().kind == OutcomeKind::ApplyFailed);
	CHECK(h.port.write_calls().empty());
	CHECK(h.logged(LogLevel::Warning, "apply.refused reason=no_delay_filter"));
}

TEST_CASE("apply reads every filter and the sync offset once before the first write")
{
	AlignmentHarness h;
	h.port.filters["cam-a"] = enabled(60);
	h.read({100, 40});

	CHECK(h.apply().kind == OutcomeKind::Applied);

	// ALIGN-15: each read once, in request order, every one before the first write.
	Strings reads;
	for (const std::string &call : h.calls_before_first_write())
		if (call.rfind("read_", 0) == 0)
			reads.push_back(call);
	CHECK(reads == Strings{"read_filter cam-a", "read_filter cam-b", "read_offset mic"});

	// The plan comes from what was read: Cam A's enabled 60 ms makes delta 40, which takes Cam A to 0 and leaves Cam B.
	CHECK(h.port.write_calls() == Strings{"write_filter cam-a 0 on", "write_offset mic 40000000"});
}

TEST_CASE("apply over the limit writes nothing and names the source")
{
	AlignmentHarness h;
	std::string named;
	int64_t value = 0;
	SUBCASE("a camera")
	{
		h.read({0, -20001});
		named = "Cam B";
		value = 20001;
	}
	SUBCASE("the mic")
	{
		h.request.cameras.resize(1);
		h.read({20005});
		named = "Shure MV7";
		value = 20005;
	}

	// ALIGN-16.
	const Outcome outcome = h.apply();
	CHECK(outcome.kind == OutcomeKind::OverLimit);
	CHECK(outcome.source == named);
	CHECK(h.port.write_calls().empty());
	CHECK(h.logged(LogLevel::Warning, "apply.refused reason=over_limit source=\"" + named +
						  "\" value_ms=" + std::to_string(value)));
	CHECK(h.alignment.status().last->kind == OutcomeKind::OverLimit);
}

TEST_CASE("apply asks only on air after the reads and before any write")
{
	AlignmentHarness h;
	SUBCASE("off air")
	{
		h.read({100, 40});
		CHECK(h.apply().kind == OutcomeKind::Applied);
		CHECK(h.asked.empty());
	}
	SUBCASE("on air, the first vector")
	{
		h.port.live = true;
		h.read({100, 40});
		CHECK(h.apply().kind == OutcomeKind::Applied);
		REQUIRE(h.asked.size() == 1);
		CHECK(same_counts(h.asked[0], Counts{1, true}));

		// ALIGN-17: asked after the reads and before any write.
		const Strings &calls = h.port.calls;
		const size_t question = index_of(calls, "question");
		CHECK(index_of(calls, "read_filter cam-a") < question);
		CHECK(index_of(calls, "read_filter cam-b") < question);
		CHECK(index_of(calls, "read_offset mic") < question);
		CHECK(question < first_write(calls));
	}
	SUBCASE("on air, the second vector")
	{
		h.port.live = true;
		h.read({-50, -120});
		CHECK(h.apply().kind == OutcomeKind::Applied);
		REQUIRE(h.asked.size() == 1);
		CHECK(same_counts(h.asked[0], Counts{2, false}));
	}
}

TEST_CASE("apply cancelled on air writes nothing and keeps the last result and undo")
{
	AlignmentHarness h;
	h.read({100, 40});
	REQUIRE(h.apply().kind == OutcomeKind::Applied); // Cam B 60, the mic 100 ms

	h.port.live = true;
	h.read({50, 30});
	const size_t writes = h.port.write_calls().size();

	// ALIGN-18.
	CHECK(h.apply(false).kind == OutcomeKind::Cancelled);
	CHECK(h.port.write_calls().size() == writes);
	CHECK(h.logged(LogLevel::Info, "apply.cancelled"));
	CHECK(h.alignment.status().last->kind == OutcomeKind::Applied);
	CHECK(h.alignment.status().undo_available);
	CHECK(h.measuring.calls == Strings{"clear"});

	// The snapshot is still the first Apply's: Undo puts back the values from before it.
	h.port.live = false;
	CHECK(h.undo().kind == OutcomeKind::Undone);
	CHECK(h.port.filter("cam-b") == absent);
	CHECK(h.port.offsets.at("mic") == 0);
}

TEST_CASE("apply after a yes refuses when a value changed or a source went while asking")
{
	AlignmentHarness h;
	h.port.live = true;
	h.read({100, 40});
	std::string line;
	SUBCASE("Cam A's delay")
	{
		h.while_asking = [&] { h.port.filters["cam-a"] = enabled(30); };
		line = "apply.refused reason=changed";
	}
	SUBCASE("the mic's offset")
	{
		h.while_asking = [&] { h.port.offsets["mic"] = 5 * AlignmentHarness::ns_per_ms; };
		line = "apply.refused reason=changed";
	}
	SUBCASE("Cam B's filter enabled state")
	{
		h.port.filters["cam-b"] = disabled(10);
		h.while_asking = [&] { h.port.filters["cam-b"] = enabled(10); };
		line = "apply.refused reason=changed";
	}
	SUBCASE("Cam B gone")
	{
		h.while_asking = [&] { h.port.forget("cam-b"); };
		line = "apply.refused reason=source_gone source=\"cam-b\"";
	}

	// ALIGN-19.
	CHECK(h.apply().kind == OutcomeKind::ApplyFailed);
	CHECK(h.asked.size() == 1);
	CHECK(h.port.write_calls().empty());
	CHECK(h.logged(LogLevel::Warning, line));
	CHECK(h.measuring.calls.empty());
}

TEST_CASE("apply after a yes refuses when the collection switched while asking")
{
	AlignmentHarness h;
	h.port.live = true;
	h.read({100, 40});
	// A duplicated collection has sources with the same UUIDs and values, so only the switch itself tells.
	h.while_asking = [&] { h.alignment.collection_changing(); };

	CHECK(h.apply().kind == OutcomeKind::ApplyFailed);
	CHECK(h.port.write_calls().empty());
	CHECK(h.logged(LogLevel::Warning, "apply.refused reason=changed"));
}

TEST_CASE("apply after a yes writes the plan the user saw when only a reading changed")
{
	AlignmentHarness h;
	h.port.live = true;
	h.read({100, 40});
	// The dock's refresh runs inside the question; a new reading for Cam B does not change the plan.
	h.while_asking = [&] { h.request.session.cameras[1].reading_ms = 90; };

	CHECK(h.apply().kind == OutcomeKind::Applied);
	CHECK(h.port.write_calls() == Strings{"write_filter cam-b 60 on", "write_offset mic 100000000"});
}

TEST_CASE("apply writes only the changed values with the cameras in order before the mic")
{
	SUBCASE("the first vector: Cam A unchanged and without a filter, so no call for it")
	{
		AlignmentHarness h;
		h.read({100, 40});
		CHECK(h.apply().kind == OutcomeKind::Applied);
		CHECK(h.port.write_calls() == Strings{"write_filter cam-b 60 on", "write_offset mic 100000000"});
		for (const std::string &call : h.port.calls)
			CHECK((call.find("cam-a") == std::string::npos || call.rfind("read_", 0) == 0 ||
			       call.rfind("resolves", 0) == 0));
	}
	SUBCASE("three cameras in the request's order, the mic unchanged")
	{
		AlignmentHarness h;
		h.port.camera("cam-c");
		h.request.cameras = {{"cam-c", "Brio"}, {"cam-a", "Cam A"}, {"cam-b", "Cam B"}};
		h.read({-10, -50, -120});
		CHECK(h.apply().kind == OutcomeKind::Applied);
		CHECK(h.port.write_calls() ==
		      Strings{"write_filter cam-c 10 on", "write_filter cam-a 50 on", "write_filter cam-b 120 on"});
	}
}

TEST_CASE("apply creates the plugin filter enabled with the new delay on a camera without one")
{
	AlignmentHarness h;
	h.read({100, 40});
	CHECK(h.apply().kind == OutcomeKind::Applied);
	// ALIGN-21: the port creates it at the end of the chain under the recognised name.
	CHECK(index_of(h.port.calls, "write_filter cam-b 60 on") < h.port.calls.size());
	CHECK(h.port.filter("cam-b") == enabled(60));
}

TEST_CASE("apply sets an existing plugin filter and enables it without removing it")
{
	AlignmentHarness h;
	FilterState after;
	std::string write;
	SUBCASE("enabled at 200, new 80: the TDD's over-delay vector")
	{
		h.port.filters["cam-a"] = enabled(200);
		h.port.offsets["mic"] = 100 * AlignmentHarness::ns_per_ms;
		h.read({150, 30});
		write = "write_filter cam-a 80 on";
		after = enabled(80);
	}
	SUBCASE("enabled at 200, new 0: set to 0 and kept")
	{
		h.port.filters["cam-a"] = enabled(200);
		h.read({300, 40});
		write = "write_filter cam-a 0 on";
		after = enabled(0);
	}
	SUBCASE("disabled at 60, new 60: enabled")
	{
		h.port.filters["cam-a"] = disabled(60);
		h.read({40, 100});
		write = "write_filter cam-a 60 on";
		after = enabled(60);
	}

	// ALIGN-22.
	CHECK(h.apply().kind == OutcomeKind::Applied);
	CHECK(h.port.write_calls().front() == write);
	CHECK(h.port.filter("cam-a") == after);
	for (const std::string &call : h.port.calls)
		CHECK(call.rfind("remove_filter", 0) != 0);
}

TEST_CASE("apply writes the mic's new sync offset in nanoseconds")
{
	AlignmentHarness h;
	h.read({100, 40});
	CHECK(h.apply().kind == OutcomeKind::Applied);
	// ALIGN-23: s' = 100 ms.
	CHECK(h.port.write_calls().back() == "write_offset mic 100000000");
	CHECK(h.port.offsets.at("mic") == 100000000);
}

TEST_CASE("apply that fails a write at each position puts every value back and reports failed")
{
	// The hand-set negative vector changes all three values: Cam A 140, Cam B 190, the mic -200 to 0 ms.
	const std::string steps[] = {"camera:\"Cam A\"", "camera:\"Cam B\"", "mic:\"Shure MV7\""};
	for (size_t position = 0; position < 3; position++) {
		for (bool refuse : {true, false}) {
			CAPTURE(position);
			CAPTURE(refuse);
			AlignmentHarness h;
			h.port.offsets["mic"] = -200 * AlignmentHarness::ns_per_ms;
			h.read({60, 10});
			if (refuse)
				h.port.refuse_writes = {position};
			else
				h.port.misread_after = {position};

			// ALIGN-25, ALIGN-72.
			CHECK(h.apply().kind == OutcomeKind::ApplyFailed);
			CHECK(h.port.filter("cam-a") == absent);
			CHECK(h.port.filter("cam-b") == absent);
			CHECK(h.port.offsets.at("mic") == -200 * AlignmentHarness::ns_per_ms);
			CHECK(h.logged(LogLevel::Warning, "apply.result failed step=" + steps[position] + " restored=yes"));
			CHECK(h.logged(LogLevel::Warning).size() == 1);
			CHECK(h.alignment.status().last->kind == OutcomeKind::ApplyFailed);
			CHECK_FALSE(h.alignment.status().undo_available);
			CHECK_FALSE(h.alignment.status().aligned_after_apply);
		}
	}
}

TEST_CASE("apply whose restore fails reports restore failed and drops undo")
{
	AlignmentHarness h;
	h.read({100, 40});
	REQUIRE(h.apply().kind == OutcomeKind::Applied); // Cam B 60, the mic 100 ms
	REQUIRE(h.alignment.status().undo_available);

	// The next Apply takes Cam B to 80 and the mic to 150. The mic's write fails, then putting Cam B back fails.
	h.read({50, 30});
	h.port.refuse_writes = {h.port.writes + 1, h.port.writes + 2};

	// ALIGN-26, ALIGN-73.
	CHECK(h.apply().kind == OutcomeKind::ApplyRestoreFailed);
	CHECK(h.logged(LogLevel::Warning, "apply.result failed step=mic:\"Shure MV7\" restored=no"));
	CHECK(h.logged(LogLevel::Warning, "restore.failed camera=\"Cam B\" intended_ms=60 actual_ms=80"));
	CHECK_FALSE(h.alignment.status().undo_available);
	CHECK_FALSE(h.alignment.status().aligned_after_apply);
	CHECK(h.alignment.status().last->kind == OutcomeKind::ApplyRestoreFailed);
}

TEST_CASE("apply that lands keeps the restore point for undo and reports the new values")
{
	AlignmentHarness h;
	h.port.filters["cam-a"] = disabled(60);
	h.port.offsets["mic"] = 400123; // rounds to 0 ms
	h.read({100, 40});

	// ALIGN-27: every ticked camera's new delay in order, then the mic's new offset.
	const Outcome outcome = h.apply();
	CHECK(outcome.kind == OutcomeKind::Applied);
	REQUIRE(outcome.values.size() == 3);
	CHECK(outcome.values[0].name == "Cam A");
	CHECK(outcome.values[0].ms == 0);
	CHECK(outcome.values[1].name == "Cam B");
	CHECK(outcome.values[1].ms == 60);
	CHECK(outcome.values[2].name == "Shure MV7");
	CHECK(outcome.values[2].ms == 100);
	CHECK(h.alignment.status().undo_available);
	CHECK(h.alignment.status().aligned_after_apply);
	CHECK(h.alignment.status().last->kind == OutcomeKind::Applied);

	// The snapshot holds what was found, exactly: Undo puts the mic back to the nanosecond and leaves Cam A's
	// disabled filter as it was.
	CHECK(h.undo().kind == OutcomeKind::Undone);
	CHECK(h.port.filter("cam-a") == disabled(60));
	CHECK(h.port.filter("cam-b") == absent);
	CHECK(h.port.offsets.at("mic") == 400123);
}

TEST_CASE("apply that lands clears the readings and starts a stopped session at the request rate")
{
	AlignmentHarness h;
	h.read({100, 40});
	SUBCASE("measuring")
	{
		CHECK(h.apply().kind == OutcomeKind::Applied);
		CHECK(h.measuring.calls == Strings{"clear"});
	}
	SUBCASE("stopped")
	{
		h.request.session.state = SessionState::Stopped;
		h.request.rate = {60000, 1001};
		CHECK(h.apply().kind == OutcomeKind::Applied);
		// Clear first: a Start in Stopped keeps every reading.
		CHECK(h.measuring.calls == Strings{"clear", "resume 60000/1001"});
	}
}

TEST_CASE("apply that fails after a write clears the readings and starts nothing")
{
	AlignmentHarness h;
	h.request.session.state = SessionState::Stopped;
	h.read({100, 40});
	h.port.refuse_writes = {1};

	// ALIGN-29.
	CHECK(h.apply().kind == OutcomeKind::ApplyFailed);
	CHECK(h.measuring.calls == Strings{"clear"});
}

TEST_CASE("apply refused before any write leaves the readings alone")
{
	AlignmentHarness h;
	h.read({100, 40});
	SUBCASE("waiting")
	{
		h.request.session.cameras[0].state = CameraState::Settling;
	}
	SUBCASE("already aligned")
	{
		h.read({1, 2});
	}
	SUBCASE("a source gone")
	{
		h.port.forget("cam-a");
	}
	SUBCASE("no delay filter")
	{
		h.port.filter_available = false;
	}
	SUBCASE("over the limit")
	{
		h.read({0, -30000});
	}
	bool yes = true;
	SUBCASE("cancelled")
	{
		h.port.live = true;
		yes = false;
	}
	SUBCASE("changed while asking")
	{
		h.port.live = true;
		h.while_asking = [&] { h.port.offsets["mic"] = 1; };
	}
	h.apply(yes);

	// ALIGN-29's second half.
	CHECK(h.port.write_calls().empty());
	CHECK(h.measuring.calls.empty());
}

TEST_CASE("apply logs the plan before the first write and the result after the last")
{
	AlignmentHarness h;
	h.port.filters["cam-b"] = disabled(60);
	h.read({100, 40});
	size_t lines_at_first_write = 0;
	h.port.on_write = [&] {
		if (h.port.writes == 1)
			lines_at_first_write = h.lines.size();
		h.now_ns += 2 * AlignmentHarness::ns_per_ms;
	};

	CHECK(h.apply().kind == OutcomeKind::Applied);

	// ALIGN-70, ALIGN-71.
	const std::vector<std::string> info = h.logged(LogLevel::Info);
	REQUIRE(info.size() == 2);
	CHECK(info[0] == "apply.plan camera=\"Cam A\" delay_ms=none->0 camera=\"Cam B\" delay_ms=60(off)->60 "
			 "mic=\"Shure MV7\" sync_offset_ms=0->100");
	CHECK(info[1] == "apply.result ok written=2 skipped=1 duration_ms=4");
	CHECK(lines_at_first_write == 1);
	CHECK(h.lines.back().second == info[1]);
}

TEST_CASE("apply counts the duration from the answer and not the request")
{
	AlignmentHarness h;
	h.port.live = true;
	h.read({100, 40});
	h.while_asking = [&] { h.now_ns += 3000 * AlignmentHarness::ns_per_ms; };
	h.port.on_write = [&] { h.now_ns += 2 * AlignmentHarness::ns_per_ms; };

	CHECK(h.apply().kind == OutcomeKind::Applied);
	CHECK(h.logged(LogLevel::Info, "apply.result ok written=2 skipped=1 duration_ms=4"));
}

TEST_CASE("apply logs each refusal at warning and each event at info")
{
	AlignmentHarness h;
	h.read({100, 40});
	LogLevel level = LogLevel::Warning;
	std::string line;
	SUBCASE("waiting")
	{
		h.request.session.cameras[1].state = CameraState::Settling;
		line = "apply.refused reason=waiting camera=\"Cam B\"";
	}
	SUBCASE("source gone")
	{
		h.port.forget("mic");
		line = "apply.refused reason=source_gone source=\"mic\"";
	}
	SUBCASE("no delay filter")
	{
		h.port.filter_available = false;
		line = "apply.refused reason=no_delay_filter";
	}
	SUBCASE("over the limit")
	{
		h.read({0, -20001});
		line = "apply.refused reason=over_limit source=\"Cam B\" value_ms=20001";
	}
	SUBCASE("changed")
	{
		h.port.live = true;
		h.while_asking = [&] { h.port.filters["cam-a"] = enabled(5); };
		line = "apply.refused reason=changed";
	}
	SUBCASE("already aligned")
	{
		h.read({0, 0});
		level = LogLevel::Info;
		line = "apply.already_aligned";
	}
	bool yes = true;
	SUBCASE("cancelled")
	{
		h.port.live = true;
		yes = false;
		level = LogLevel::Info;
		line = "apply.cancelled";
	}

	h.apply(yes);
	REQUIRE(h.lines.size() == 1);
	CHECK(h.lines[0].first == level);
	CHECK(h.lines[0].second == line);
}
