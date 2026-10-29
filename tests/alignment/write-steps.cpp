// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "alignment/write-steps.hpp"
#include "support/fake-port.hpp"

using camera_sync::FilterState;
using camera_sync::Step;
using camera_sync::StepsResult;
using camera_sync::write_steps;

namespace {

const FilterState absent{};
const FilterState enabled_60{true, 60, true};
const FilterState enabled_100{true, 100, true};

// Cam A has no plugin filter and gets one at 60 ms; Cam B's at 100 ms is removed; the mic goes from 0 to 100 ms.
struct ThreeSteps {
	FakePort port;
	std::vector<Step> steps;

	ThreeSteps()
	{
		port.camera("cam-a");
		port.camera("cam-b", enabled_100);
		port.mic("mic", 0);

		Step a;
		a.source = "cam-a";
		a.name = "Cam A";
		a.camera_target = enabled_60;
		a.camera_found = absent;
		Step b;
		b.source = "cam-b";
		b.name = "Cam B";
		b.camera_target = absent;
		b.camera_found = enabled_100;
		Step m;
		m.mic = true;
		m.source = "mic";
		m.name = "Shure MV7";
		m.mic_target_ns = 100000000;
		m.mic_found_ns = 0;
		steps = {a, b, m};
	}

	// Every value as it was before any step.
	void check_as_found() const
	{
		CHECK(port.filter("cam-a") == absent);
		CHECK(port.filter("cam-b") == enabled_100);
		CHECK(port.offsets.at("mic") == 0);
	}
};

// The write that puts each step's found value back.
const std::vector<std::string> restore_write = {"remove_filter cam-a", "write_filter cam-b 100 on",
						"write_offset mic 0"};

} // namespace

TEST_CASE("write steps reads each value back right after writing it")
{
	ThreeSteps t;
	const StepsResult result = write_steps(t.port, t.steps);

	// ALIGN-24: a camera with an absent target loses its filter; the others are written with their values.
	const std::vector<std::string> &calls = t.port.calls;
	REQUIRE(calls.size() == 6);
	CHECK(calls[0] == "write_filter cam-a 60 on");
	CHECK(calls[1] == "read_filter cam-a");
	CHECK(calls[2] == "remove_filter cam-b");
	CHECK(calls[3] == "read_filter cam-b");
	CHECK(calls[4] == "write_offset mic 100000000");
	CHECK(calls[5] == "read_offset mic");
	CHECK(result.ok);
	CHECK(result.written == 3);
	CHECK_FALSE(result.failed);
	CHECK(result.restore_failures.empty());
	CHECK(t.port.filter("cam-a") == enabled_60);
	CHECK(t.port.filter("cam-b") == absent);
	CHECK(t.port.offsets.at("mic") == 100000000);
}

TEST_CASE("write steps fails a step the port refuses or reads back different")
{
	ThreeSteps t;
	SUBCASE("the port refuses the second write")
	{
		t.port.refuse_writes = {1};
	}
	SUBCASE("the second write reads back different")
	{
		t.port.misread_after = {1};
	}
	const StepsResult result = write_steps(t.port, t.steps);
	CHECK_FALSE(result.ok);
	REQUIRE(result.failed);
	CHECK(*result.failed == 1);
	CHECK(result.written == 2);
	// Nothing more from the steps is written: the mic is never touched.
	for (const std::string &call : t.port.write_calls())
		CHECK(call.rfind("write_offset mic 100000000", 0) != 0);

	// A disabled target is held to the state written, though a disabled filter delays nothing at any delay: a
	// read-back at another delay, or enabled, fails it, and the filter it created goes again.
	for (bool delay_misread : {true, false}) {
		CAPTURE(delay_misread);
		FakePort port;
		port.camera("cam-a");
		Step step;
		step.source = "cam-a";
		step.name = "Cam A";
		step.camera_target = FilterState{true, 120, false};
		step.camera_found = absent;
		if (delay_misread)
			port.misread_after = {0};
		else
			port.misread_enabled_after = {0};
		const StepsResult disabled = write_steps(port, {step});
		CHECK_FALSE(disabled.ok);
		CHECK(port.filter("cam-a") == absent);
	}
}

TEST_CASE("write steps puts every value back in reverse order after a failure at each position")
{
	// ALIGN-25: a failure at each of the three writes and each of the three read-backs.
	for (size_t position = 0; position < 3; position++) {
		for (bool refuse : {true, false}) {
			CAPTURE(position);
			CAPTURE(refuse);
			ThreeSteps t;
			if (refuse)
				t.port.refuse_writes = {position};
			else
				t.port.misread_after = {position};

			const StepsResult result = write_steps(t.port, t.steps);
			CHECK_FALSE(result.ok);
			REQUIRE(result.failed);
			CHECK(*result.failed == position);
			CHECK(result.written == position + 1);
			CHECK(result.restore_failures.empty());
			t.check_as_found();

			// The writes after the failed one put back, newest first, every step that changed a value: a refused
			// write changed nothing, a misread one landed and is put back too.
			std::vector<std::string> expected;
			for (size_t step = refuse ? position : position + 1; step-- > 0;)
				expected.push_back(restore_write[step]);
			const std::vector<std::string> writes = t.port.write_calls();
			REQUIRE(writes.size() == position + 1 + expected.size());
			CHECK(std::vector<std::string>(writes.begin() + position + 1, writes.end()) == expected);
		}
	}
}

TEST_CASE("write steps removes a created filter whose delay reads back wrong")
{
	// A write can half-land: the filter is created and its delay reads back wrong. The failed step is put back.
	ThreeSteps t;
	t.steps.resize(1);
	t.port.misread_after = {0};
	const StepsResult result = write_steps(t.port, t.steps);
	CHECK_FALSE(result.ok);
	CHECK(t.port.filter("cam-a") == absent);
	CHECK(t.port.write_calls() == std::vector<std::string>{"write_filter cam-a 60 on", "remove_filter cam-a"});
}

TEST_CASE("write steps carries on putting back past a failed restore write")
{
	// ALIGN-26: the mic's write fails, then putting Cam B's filter back fails; Cam A is still put back.
	ThreeSteps t;
	t.port.refuse_writes = {2, 3};
	const StepsResult result = write_steps(t.port, t.steps);
	CHECK_FALSE(result.ok);
	REQUIRE(result.failed);
	CHECK(*result.failed == 2);
	REQUIRE(result.restore_failures.size() == 1);
	CHECK(result.restore_failures[0].step == 1);
	CHECK(result.restore_failures[0].camera_actual == absent);
	CHECK(t.port.filter("cam-a") == absent);
	CHECK(t.port.filter("cam-b") == absent);
	CHECK(t.port.write_calls().back() == "remove_filter cam-a");

	// A failed restore of the mic reports what it reads.
	ThreeSteps m;
	m.port.misread_after = {2};
	m.port.refuse_writes = {3};
	const StepsResult mic = write_steps(m.port, m.steps);
	REQUIRE(mic.restore_failures.size() == 1);
	CHECK(mic.restore_failures[0].step == 2);
	CHECK(mic.restore_failures[0].mic_actual_ns == 100000000);
	CHECK(m.port.filter("cam-a") == absent);
	CHECK(m.port.filter("cam-b") == enabled_100);
}
