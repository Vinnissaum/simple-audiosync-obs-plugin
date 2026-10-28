// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "alignment/calculator.hpp"

using camera_sync::compute_plan;
using camera_sync::FilterState;
using camera_sync::Plan;
using camera_sync::PlanResult;
using camera_sync::precheck;
using camera_sync::PrecheckKind;
using camera_sync::Reading;

namespace {

constexpr int64_t ns_per_ms = 1000000;

Reading stable(int64_t ms)
{
	return Reading{true, ms};
}

Reading not_stable(int64_t ms = 0)
{
	return Reading{false, ms};
}

// No plugin filter on any of n cameras.
std::vector<FilterState> no_filters(size_t n)
{
	return std::vector<FilterState>(n);
}

FilterState enabled(int64_t ms)
{
	return FilterState{true, ms, true};
}

// A plan that the test requires to exist.
Plan plan_of(const std::vector<int64_t> &readings, const std::vector<FilterState> &filters, int64_t offset_ms)
{
	const PlanResult result = compute_plan(readings, filters, offset_ms * ns_per_ms);
	REQUIRE(result.plan);
	CHECK_FALSE(result.over_limit);
	return *result.plan;
}

std::vector<int64_t> new_delays(const Plan &plan)
{
	std::vector<int64_t> delays;
	for (const auto &camera : plan.cameras)
		delays.push_back(camera.new_ms);
	return delays;
}

} // namespace

TEST_CASE("precheck waits for the first camera in order that is not stable")
{
	// ALIGN-01: the index is the first in the given order, which is the checklist's.
	auto result = precheck({stable(10), not_stable(5), stable(100)}, 33.333);
	CHECK(result.kind == PrecheckKind::Waiting);
	CHECK(result.camera == 1);

	result = precheck({not_stable(0), not_stable(5)}, 33.333);
	CHECK(result.kind == PrecheckKind::Waiting);
	CHECK(result.camera == 0);

	// Waiting is decided before any reading is looked at: a Settling camera within one frame is not aligned.
	result = precheck({stable(2), not_stable(3)}, 33.333);
	CHECK(result.kind == PrecheckKind::Waiting);
	CHECK(result.camera == 1);
}

TEST_CASE("precheck finds every reading within one frame already aligned")
{
	// ALIGN-02: -F <= r <= F on every camera, on both sides of each bound.
	CHECK(precheck({stable(20), stable(-30)}, 33.333).kind == PrecheckKind::AlreadyAligned);
	CHECK(precheck({stable(33), stable(-33)}, 33.333).kind == PrecheckKind::AlreadyAligned);
	CHECK(precheck({stable(34), stable(0)}, 33.333).kind == PrecheckKind::Ready);
	CHECK(precheck({stable(0), stable(-34)}, 33.333).kind == PrecheckKind::Ready);
	CHECK(precheck({stable(33)}, 33.0).kind == PrecheckKind::AlreadyAligned);
	CHECK(precheck({stable(-33)}, 33.0).kind == PrecheckKind::AlreadyAligned);
	CHECK(precheck({stable(34)}, 33.0).kind == PrecheckKind::Ready);
	CHECK(precheck({stable(-34)}, 33.0).kind == PrecheckKind::Ready);
}

TEST_CASE("precheck finds the eighth test vector already aligned")
{
	// The TDD's last vector: r = (+20, -30) at F = 33 ms, nothing written.
	CHECK(precheck({stable(20), stable(-30)}, 33.0).kind == PrecheckKind::AlreadyAligned);
}

TEST_CASE("plan takes the effective delay of an absent, enabled or disabled filter")
{
	// ALIGN-03. A disabled 60 ms filter on the first camera counts as 0, so the plan is the first vector's and the
	// first camera, 0 before and after, is unchanged.
	Plan plan = plan_of({100, 40}, {FilterState{true, 60, false}, FilterState{}}, 0);
	CHECK(plan.delta_ms == 100);
	CHECK(new_delays(plan) == std::vector<int64_t>{0, 60});
	CHECK(plan.cameras[0].old_ms == 0);
	CHECK_FALSE(plan.cameras[0].changed);

	// The same filter enabled delays the first camera by 60 already.
	plan = plan_of({100, 40}, {enabled(60), FilterState{}}, 0);
	CHECK(plan.delta_ms == 40);
	CHECK(plan.cameras[0].old_ms == 60);
	CHECK(new_delays(plan) == std::vector<int64_t>{0, 0});
	CHECK(plan.new_offset_ms == 40);
}

TEST_CASE("plan rounds the sync offset half away from zero before taking the delta")
{
	// ALIGN-04: with one camera at -500 the offset's bound -s sets the delta, so the rounded s shows in it.
	const std::vector<FilterState> none = no_filters(1);

	PlanResult result = compute_plan({-500}, none, -200400000);
	REQUIRE(result.plan);
	CHECK(result.plan->old_offset_ms == -200);
	CHECK(result.plan->delta_ms == 200);
	CHECK(result.plan->new_offset_ms == 0);

	result = compute_plan({-500}, none, -200500000);
	REQUIRE(result.plan);
	CHECK(result.plan->old_offset_ms == -201);
	CHECK(result.plan->delta_ms == 201);
	CHECK(result.plan->new_offset_ms == 0);

	result = compute_plan({-500}, none, -200600000);
	REQUIRE(result.plan);
	CHECK(result.plan->old_offset_ms == -201);
	CHECK(result.plan->delta_ms == 201);

	// A positive half rounds up.
	result = compute_plan({-500}, none, 100500000);
	REQUIRE(result.plan);
	CHECK(result.plan->old_offset_ms == 101);
}

TEST_CASE("plan gives a single camera the whole delta")
{
	// ALIGN-05: d' = d + delta - r and s' = s + delta.
	const Plan plan = plan_of({75}, no_filters(1), 0);
	CHECK(plan.delta_ms == 75);
	CHECK(new_delays(plan) == std::vector<int64_t>{0});
	CHECK(plan.new_offset_ms == 75);
}

TEST_CASE("plan never gives a negative delay or sync offset over a grid of inputs")
{
	// ALIGN-06, and the TDD's guarantee that every reading ends at zero: r + (d' - d) - (s' - s) = 0.
	int points = 0;
	for (int64_t r1 = -1000; r1 <= 1000; r1 += 37) {
		for (int64_t r2 = -1000; r2 <= 1000; r2 += 37) {
			for (int64_t d : {0, 200}) {
				for (int64_t s : {-950, 0, 300}) {
					const std::vector<int64_t> readings{r1, r2};
					const std::vector<FilterState> filters{enabled(d), FilterState{}};
					const PlanResult result = compute_plan(readings, filters, s * ns_per_ms);
					REQUIRE(result.plan);
					const Plan &plan = *result.plan;
					const int64_t old_delays[] = {d, 0};
					for (size_t i = 0; i < 2; i++) {
						REQUIRE(plan.cameras[i].new_ms >= 0);
						const int64_t added = plan.cameras[i].new_ms - old_delays[i];
						REQUIRE(readings[i] + added - (plan.new_offset_ms - s) == 0);
					}
					REQUIRE(plan.new_offset_ms >= 0);
					points++;
				}
			}
		}
	}
	CHECK(points == 55 * 55 * 2 * 3);
}

TEST_CASE("plan refuses the first source over 20000 ms and accepts exactly 20000")
{
	// ALIGN-07. A camera's new delay is delta - r, so a camera 20000 ms ahead of the slowest gets 20000.
	PlanResult result = compute_plan({0, -20000}, no_filters(2), 0);
	REQUIRE(result.plan);
	CHECK(new_delays(*result.plan) == std::vector<int64_t>{0, 20000});

	result = compute_plan({0, -20001}, no_filters(2), 0);
	CHECK_FALSE(result.plan);
	REQUIRE(result.over_limit);
	CHECK(*result.over_limit == 1);
	CHECK(result.over_limit_ms == 20001);

	// The mic's new offset is s + delta.
	result = compute_plan({20000}, no_filters(1), 0);
	REQUIRE(result.plan);
	CHECK(result.plan->new_offset_ms == 20000);

	result = compute_plan({20001}, no_filters(1), 0);
	CHECK_FALSE(result.plan);
	REQUIRE(result.over_limit);
	CHECK(*result.over_limit == 1); // the camera count names the mic
	CHECK(result.over_limit_ms == 20001);

	// Two cameras over: the first in order is named, though the second is further over.
	result = compute_plan({0, -20005, -20010}, no_filters(3), 0);
	REQUIRE(result.over_limit);
	CHECK(*result.over_limit == 1);
	CHECK(result.over_limit_ms == 20005);

	// A camera over and the mic over: the camera comes first.
	result = compute_plan({25000, -100}, no_filters(2), 0);
	REQUIRE(result.over_limit);
	CHECK(*result.over_limit == 1);
	CHECK(result.over_limit_ms == 25100);
}

TEST_CASE("plan matches the seven computed test vectors")
{
	// The TDD's vectors at F = 33 ms: readings, plugin filter delays, sync offset; delta, new delays, new offset.
	struct Vector {
		std::vector<int64_t> r;
		std::vector<int64_t> d;
		int64_t s;
		int64_t delta;
		std::vector<int64_t> d_new;
		int64_t s_new;
	};
	const Vector vectors[] = {
		{{100, 40}, {0, 0}, 0, 100, {0, 60}, 100},           // two cameras, no corrections
		{{-50, -120}, {0, 0}, 0, 0, {50, 120}, 0},           // mic slower than every camera
		{{40, 40}, {0, 60}, 100, 40, {0, 60}, 140},          // re-apply adds the remainder
		{{150, 30}, {200, 0}, 100, 30, {80, 0}, 130},        // the plugin's filter over-delays a camera
		{{-50, -30}, {0, 0}, 100, -30, {20, 0}, 70},         // the mic's offset is larger than needed
		{{480, 30}, {0, 0}, 0, 480, {0, 450}, 480},          // a user's own 500 ms filter on camera 1
		{{60, 10}, {0, 0}, -200, 200, {140, 190}, 0},        // hand-set negative sync offset
	};
	for (const Vector &v : vectors) {
		CAPTURE(v.r[0]);
		CAPTURE(v.r[1]);
		CHECK(precheck({stable(v.r[0]), stable(v.r[1])}, 33.0).kind == PrecheckKind::Ready);
		std::vector<FilterState> filters;
		for (int64_t d : v.d)
			filters.push_back(d ? enabled(d) : FilterState{});
		const Plan plan = plan_of(v.r, filters, v.s);
		CHECK(plan.delta_ms == v.delta);
		CHECK(new_delays(plan) == v.d_new);
		CHECK(plan.old_offset_ms == v.s);
		CHECK(plan.new_offset_ms == v.s_new);
	}
}

TEST_CASE("plan marks which cameras and whether the mic change")
{
	// ALIGN-09: the first vector changes the second camera and the mic, not the first.
	Plan plan = plan_of({100, 40}, no_filters(2), 0);
	CHECK_FALSE(plan.cameras[0].changed);
	CHECK(plan.cameras[1].changed);
	CHECK(plan.mic_changed);

	// The second vector changes both cameras and leaves the mic.
	plan = plan_of({-50, -120}, no_filters(2), 0);
	CHECK(plan.cameras[0].changed);
	CHECK(plan.cameras[1].changed);
	CHECK_FALSE(plan.mic_changed);

	// An offset that rounds to the same ms is unchanged though its nanoseconds differ from the new whole ms.
	const PlanResult rounded = compute_plan({-50, -120}, no_filters(2), 400000);
	REQUIRE(rounded.plan);
	CHECK_FALSE(rounded.plan->mic_changed);
}

TEST_CASE("plan leaves a camera without a filter unchanged when its new delay is 0")
{
	// ALIGN-10: the first vector's first camera has no filter and keeps 0, so nothing would create one.
	const Plan plan = plan_of({100, 40}, no_filters(2), 0);
	CHECK(plan.cameras[0].old_ms == 0);
	CHECK(plan.cameras[0].new_ms == 0);
	CHECK_FALSE(plan.cameras[0].changed);
}
