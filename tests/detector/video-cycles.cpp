// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "detector/video-detector.hpp"
#include "reading/camera-pipeline.hpp"
#include "support/pattern-timeline.hpp"

using camera_sync::CameraPipeline;
using camera_sync::VideoMarker;

namespace {

constexpr uint64_t one_ms = 1000000;

uint64_t distance(uint64_t a, uint64_t b)
{
	return a > b ? a - b : b - a;
}

// The video markers one camera's pipeline finds in the four cycles, frame 0 shown at start_ns.
std::vector<VideoMarker> markers_of(uint64_t start_ns)
{
	const PatternTimeline timeline = read_pattern_timeline(start_ns);
	REQUIRE(timeline.sequence.size() == pattern_cycles * pattern_cycle_frames);

	std::vector<VideoMarker> markers;
	CameraPipeline pipeline(
		nullptr, [] { return uint64_t(0); }, [&](const VideoMarker &marker) { markers.push_back(marker); });
	play(timeline, pipeline);
	return markers;
}

} // namespace

TEST_CASE("pattern video cycles decode to one marker per cycle with the index it was generated with")
{
	const std::vector<VideoMarker> markers = markers_of(0);

	REQUIRE(markers.size() == pattern_cycles);
	for (uint32_t k = 0; k < pattern_cycles; k++) {
		CAPTURE(k);
		CHECK(markers[k].qr_data.index == k);
		// READ-20: within 1 ms of the generated time, the start of frame 2q of cycle k.
		const uint64_t generated_ns = pattern_frame_ns(0, k * pattern_cycle_frames + pattern_marker_frame);
		CHECK(distance(markers[k].timestamp, generated_ns) <= one_ms);
	}
}

TEST_CASE("pattern video cycles' markers are one marker cycle of 3q frames, 200 ms, apart")
{
	// Frame 0 shown 2.5 s in: the first marker moves with it.
	const uint64_t start_ns = 2500 * one_ms;
	const std::vector<VideoMarker> markers = markers_of(start_ns);

	REQUIRE(markers.size() == pattern_cycles);
	CHECK(distance(markers[0].timestamp, pattern_frame_ns(start_ns, pattern_marker_frame)) <= one_ms);
	for (uint32_t k = 1; k < pattern_cycles; k++) {
		CAPTURE(k);
		CHECK(distance(markers[k].timestamp - markers[k - 1].timestamp, 200 * one_ms) <= one_ms);
	}
}
