// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "reading/estimator.hpp"

using camera_sync::CameraRow;
using camera_sync::CameraState;
using camera_sync::PatternParams;
using camera_sync::ReadingEstimator;
using camera_sync::SampleResult;
using camera_sync::StableChange;

namespace {

// Sync Dock's pattern at q=4 frames and 60 fps: a QR code lasts 66 ms, a marker cycle 3 x 66 ms and a wrap of 256
// indexes 50.688 s, so half a wrap is 25.344 s.
const PatternParams pattern{884, 2, 66, 256};
constexpr uint64_t cycle_ns = 198000000;
constexpr uint64_t half_wrap_ns = 25344000000;

constexpr uint64_t ms(uint64_t value)
{
	return value * 1000000;
}

bool has_row(const ReadingEstimator &estimator, const std::string &camera)
{
	for (const CameraRow &row : estimator.snapshot().cameras)
		if (row.id == camera)
			return true;
	return false;
}

CameraRow row(const ReadingEstimator &estimator, const std::string &camera)
{
	for (const CameraRow &row : estimator.snapshot().cameras)
		if (row.id == camera)
			return row;
	FAIL("no row for camera ", camera);
	return {};
}

// One sample on a camera: an audio marker at audio_ns and the camera's video marker offset_ms after it.
void pair(ReadingEstimator &estimator, const std::string &camera, uint32_t index, uint64_t audio_ns, int64_t offset_ms)
{
	estimator.on_audio_marker(index, audio_ns);
	estimator.on_video_marker(camera, index, audio_ns + offset_ms * 1000000);
}

// F at 30 fps: 33.333 ms.
const double f_30fps = 1000.0 / 30;

int64_t to_ns(double offset_ms)
{
	return std::llround(offset_ms * 1e6);
}

// One sample of offset_ms, which may have a fraction, on a camera, its audio marker first. Returns what the video
// marker did.
SampleResult feed(ReadingEstimator &estimator, const char *camera, uint32_t index, uint64_t audio_ns, double offset_ms)
{
	estimator.on_audio_marker(index, audio_ns);
	return estimator.on_video_marker(camera, index, audio_ns + to_ns(offset_ms));
}

// Samples of these offsets on a camera, one marker cycle apart, from this index and audio time on.
void feed_all(ReadingEstimator &estimator, const char *camera, uint32_t index, uint64_t first_ns,
	      std::initializer_list<double> offsets)
{
	for (double offset_ms : offsets) {
		feed(estimator, camera, index, first_ns, offset_ms);
		index++;
		first_ns += cycle_ns;
	}
}

} // namespace

TEST_CASE("estimator forms a sample of the video time minus the audio time")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	SUBCASE("video late")
	{
		estimator.on_audio_marker(7, ms(1000));
		estimator.on_video_marker("cam", 7, ms(1142));
		CHECK(row(estimator, "cam").samples == 1);
		CHECK(row(estimator, "cam").live_ms == 142);
	}
	SUBCASE("video early")
	{
		estimator.on_audio_marker(7, ms(1142));
		estimator.on_video_marker("cam", 7, ms(1000));
		CHECK(row(estimator, "cam").samples == 1);
		CHECK(row(estimator, "cam").live_ms == -142);
	}
}

TEST_CASE("estimator forms no sample from markers of different indexes")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	// An audio marker waiting for a video marker, then a video marker waiting for an audio marker.
	estimator.on_audio_marker(7, ms(1000));
	estimator.on_video_marker("cam", 8, ms(1142));
	estimator.on_audio_marker(9, ms(1200));

	CHECK(row(estimator, "cam").samples == 0);
	CHECK(row(estimator, "cam").state == CameraState::Looking);
}

TEST_CASE("estimator pairs only markers less than half a wrap apart")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("half");
	estimator.add_camera("under");
	const uint64_t audio_ns = ms(30000);

	SUBCASE("audio first")
	{
		estimator.on_audio_marker(7, audio_ns);
		estimator.on_video_marker("half", 7, audio_ns + half_wrap_ns);
		estimator.on_video_marker("under", 7, audio_ns + half_wrap_ns - 1);
		CHECK(row(estimator, "half").samples == 0);
		CHECK(row(estimator, "under").samples == 1);
		CHECK(row(estimator, "under").live_ms == 25344);
	}
	SUBCASE("video first")
	{
		estimator.on_video_marker("half", 7, audio_ns);
		estimator.on_video_marker("under", 7, audio_ns + 1);
		estimator.on_audio_marker(7, audio_ns + half_wrap_ns);
		CHECK(row(estimator, "half").samples == 0);
		CHECK(row(estimator, "under").samples == 1);
		CHECK(row(estimator, "under").live_ms == -25344);
	}
}

TEST_CASE("estimator pairs a video marker with the nearest audio marker of its index")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	SUBCASE("the later of two, listed second")
	{
		estimator.on_audio_marker(5, ms(1000));
		estimator.on_audio_marker(5, ms(1100));
		estimator.on_video_marker("cam", 5, ms(1142));
		CHECK(row(estimator, "cam").live_ms == 42);
	}
	SUBCASE("the later of two, listed first")
	{
		estimator.on_audio_marker(5, ms(1100));
		estimator.on_audio_marker(5, ms(1000));
		estimator.on_video_marker("cam", 5, ms(1142));
		CHECK(row(estimator, "cam").live_ms == 42);
	}
	SUBCASE("the earlier of two")
	{
		estimator.on_audio_marker(5, ms(1000));
		estimator.on_audio_marker(5, ms(1300));
		estimator.on_video_marker("cam", 5, ms(1142));
		CHECK(row(estimator, "cam").live_ms == 142);
	}
	CHECK(row(estimator, "cam").samples == 1);
}

TEST_CASE("estimator forms the same sample whichever marker arrives first")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	SUBCASE("audio first")
	{
		estimator.on_audio_marker(3, ms(2000));
		estimator.on_video_marker("cam", 3, ms(2142));
	}
	SUBCASE("video first")
	{
		estimator.on_video_marker("cam", 3, ms(2142));
		estimator.on_audio_marker(3, ms(2000));
	}
	CHECK(row(estimator, "cam").samples == 1);
	CHECK(row(estimator, "cam").live_ms == 142);
}

TEST_CASE("estimator keeps one window per camera against the same audio markers")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("a");
	estimator.add_camera("b");
	estimator.add_camera("c");

	// Camera a's video trails the mic by 100 ms and camera b's by 40 ms, and both reach the estimator after the
	// audio marker. Camera c's video leads by 20 ms and reaches it before.
	for (uint32_t k = 0; k < 3; k++) {
		const uint64_t audio_ns = ms(10000) + k * cycle_ns;
		estimator.on_video_marker("c", k, audio_ns - ms(20));
		estimator.on_audio_marker(k, audio_ns);
		estimator.on_video_marker("a", k, audio_ns + ms(100));
		estimator.on_video_marker("b", k, audio_ns + ms(40));
	}

	CHECK(row(estimator, "a").samples == 3);
	CHECK(row(estimator, "a").live_ms == 100);
	CHECK(row(estimator, "b").samples == 3);
	CHECK(row(estimator, "b").live_ms == 40);
	CHECK(row(estimator, "c").samples == 3);
	CHECK(row(estimator, "c").live_ms == -20);

	estimator.remove_camera("b");
	CHECK_FALSE(has_row(estimator, "b"));
	CHECK(row(estimator, "a").samples == 3);
	CHECK(row(estimator, "a").live_ms == 100);
	CHECK(row(estimator, "c").samples == 3);
	CHECK(row(estimator, "c").live_ms == -20);
}

// The spec's duplicate handling: a repeat cannot add a second sample for the same index.
TEST_CASE("estimator forms one sample from a repeated video marker")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	SUBCASE("both after the audio marker")
	{
		estimator.on_audio_marker(4, ms(1000));
		estimator.on_video_marker("cam", 4, ms(1142));
		estimator.on_video_marker("cam", 4, ms(1150));
		CHECK(row(estimator, "cam").live_ms == 142);
	}
	SUBCASE("both before the audio marker: the nearer pairs")
	{
		estimator.on_video_marker("cam", 4, ms(1150));
		estimator.on_video_marker("cam", 4, ms(1142));
		estimator.on_audio_marker(4, ms(1000));
		CHECK(row(estimator, "cam").live_ms == 142);
	}
	CHECK(row(estimator, "cam").samples == 1);
}

TEST_CASE("estimator drops the oldest of more than 256 unpaired video markers")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	// 257 markers, one a cycle: indexes 0 to 255, then 0 again a wrap after the first.
	const uint64_t first_ns = ms(100000);
	for (uint32_t k = 0; k <= 256; k++)
		estimator.on_video_marker("cam", k % 256, first_ns + k * cycle_ns);

	// The first marker was dropped, and the index 0 left is a wrap away from this audio marker.
	estimator.on_audio_marker(0, first_ns - ms(100));
	CHECK(row(estimator, "cam").samples == 0);

	// The second marker is still there.
	estimator.on_audio_marker(1, first_ns + cycle_ns - ms(100));
	CHECK(row(estimator, "cam").samples == 1);
	CHECK(row(estimator, "cam").live_ms == 100);
}

TEST_CASE("estimator drops the oldest of more than 256 audio markers")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	const uint64_t first_ns = ms(100000);
	for (uint32_t k = 0; k <= 256; k++)
		estimator.on_audio_marker(k % 256, first_ns + k * cycle_ns);

	// The first audio marker was dropped, so the video marker of its flash forms no sample.
	estimator.on_video_marker("cam", 0, first_ns + ms(100));
	CHECK(row(estimator, "cam").samples == 0);

	// The second one is still there.
	estimator.on_video_marker("cam", 1, first_ns + cycle_ns + ms(100));
	CHECK(row(estimator, "cam").samples == 1);
	CHECK(row(estimator, "cam").live_ms == 100);
}

TEST_CASE("estimator reports Looking until the first sample, then Settling with it rounded")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	CHECK(row(estimator, "cam").state == CameraState::Looking);
	CHECK(row(estimator, "cam").samples == 0);

	// A video marker alone is no sample.
	estimator.on_video_marker("cam", 1, ms(5000));
	CHECK(row(estimator, "cam").state == CameraState::Looking);

	SUBCASE("141.5 ms")
	{
		estimator.on_audio_marker(1, ms(5000) - 141500000);
		CHECK(row(estimator, "cam").live_ms == 142);
	}
	SUBCASE("-141.5 ms")
	{
		estimator.on_audio_marker(1, ms(5000) + 141500000);
		CHECK(row(estimator, "cam").live_ms == -142);
	}
	CHECK(row(estimator, "cam").state == CameraState::Settling);
	CHECK(row(estimator, "cam").samples == 1);
}

TEST_CASE("estimator keeps a camera Settling with its latest sample while its window fills")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	// Samples 50 ms apart: far more than any frame, so they never agree.
	const int64_t offsets[] = {100, 150, 200, 250, 300};
	for (uint32_t k = 0; k < 5; k++) {
		pair(estimator, "cam", k, ms(10000) + k * cycle_ns, offsets[k]);
		CAPTURE(k);
		CHECK(row(estimator, "cam").state == CameraState::Settling);
		CHECK(row(estimator, "cam").samples == k + 1);
		CHECK(row(estimator, "cam").live_ms == offsets[k]);
	}
}

TEST_CASE("estimator keeps the five most recent samples by video time")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("cam");

	const uint64_t first_ns = ms(10000);
	const int64_t offsets[] = {100, 150, 200, 250, 300};
	for (uint32_t k = 0; k < 5; k++)
		pair(estimator, "cam", 10 + k, first_ns + k * cycle_ns, offsets[k]);
	REQUIRE(row(estimator, "cam").samples == 5);
	REQUIRE(row(estimator, "cam").live_ms == 300);

	// A sixth sample from a flash before all five is not among the five most recent.
	pair(estimator, "cam", 9, first_ns - cycle_ns, 50);
	CHECK(row(estimator, "cam").samples == 5);
	CHECK(row(estimator, "cam").live_ms == 300);

	// A sample from a later flash pushes out the oldest and is the latest.
	pair(estimator, "cam", 15, first_ns + 5 * cycle_ns, 350);
	CHECK(row(estimator, "cam").samples == 5);
	CHECK(row(estimator, "cam").live_ms == 350);
}

TEST_CASE("estimator clear returns every camera to Looking and keeps them added")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("a");
	estimator.add_camera("b");
	pair(estimator, "a", 1, ms(10000), 100);
	pair(estimator, "b", 1, ms(10000), 40);
	// Markers still waiting: an audio marker and one of camera a's video markers.
	estimator.on_audio_marker(2, ms(10000) + cycle_ns);
	estimator.on_video_marker("a", 3, ms(10000) + 2 * cycle_ns + ms(100));

	estimator.clear();

	REQUIRE(estimator.snapshot().cameras.size() == 2);
	CHECK(row(estimator, "a").state == CameraState::Looking);
	CHECK(row(estimator, "a").samples == 0);
	CHECK(row(estimator, "b").state == CameraState::Looking);
	CHECK(row(estimator, "b").samples == 0);

	// The waiting markers are gone: their partners form no sample.
	estimator.on_video_marker("b", 2, ms(10000) + cycle_ns + ms(40));
	estimator.on_audio_marker(3, ms(10000) + 2 * cycle_ns);
	CHECK(row(estimator, "a").samples == 0);
	CHECK(row(estimator, "b").samples == 0);

	// Both cameras are still added.
	pair(estimator, "a", 4, ms(10000) + 3 * cycle_ns, 100);
	pair(estimator, "b", 5, ms(10000) + 4 * cycle_ns, 40);
	CHECK(row(estimator, "a").samples == 1);
	CHECK(row(estimator, "b").samples == 1);
}

TEST_CASE("estimator clear_camera clears one camera and keeps it added")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("a");
	estimator.add_camera("b");
	for (uint32_t k = 0; k < 2; k++) {
		const uint64_t audio_ns = ms(10000) + k * cycle_ns;
		estimator.on_audio_marker(k, audio_ns);
		estimator.on_video_marker("a", k, audio_ns + ms(100));
		estimator.on_video_marker("b", k, audio_ns + ms(40));
	}
	// Each camera has a video marker waiting for its audio marker.
	estimator.on_video_marker("a", 2, ms(10000) + 2 * cycle_ns + ms(100));
	estimator.on_video_marker("b", 2, ms(10000) + 2 * cycle_ns + ms(40));

	estimator.clear_camera("a");

	CHECK(row(estimator, "a").state == CameraState::Looking);
	CHECK(row(estimator, "a").samples == 0);
	CHECK(row(estimator, "b").state == CameraState::Settling);
	CHECK(row(estimator, "b").samples == 2);
	CHECK(row(estimator, "b").live_ms == 40);

	// Camera a's waiting marker is gone and camera b's is not.
	estimator.on_audio_marker(2, ms(10000) + 2 * cycle_ns);
	CHECK(row(estimator, "a").samples == 0);
	CHECK(row(estimator, "b").samples == 3);

	// Camera a is still added.
	pair(estimator, "a", 3, ms(10000) + 3 * cycle_ns, 100);
	CHECK(row(estimator, "a").samples == 1);
	CHECK(row(estimator, "a").live_ms == 100);
}

TEST_CASE("estimator ignores a video marker of a camera that is not added")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.add_camera("a");

	pair(estimator, "ghost", 1, ms(10000), 100);
	CHECK_FALSE(has_row(estimator, "ghost"));
	CHECK(estimator.snapshot().cameras.size() == 1);

	estimator.remove_camera("a");
	pair(estimator, "a", 2, ms(10000) + cycle_ns, 100);
	CHECK(estimator.snapshot().cameras.empty());
}

TEST_CASE("estimator counts a window spanning exactly F as within one frame")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(33.0);
	estimator.add_camera("exact");
	estimator.add_camera("over");

	const double exact[] = {100, 110, 120, 130, 133};
	const double over[] = {100, 110, 120, 130, 133.01};
	for (uint32_t k = 0; k < 5; k++) {
		const uint64_t audio_ns = ms(10000) + k * cycle_ns;
		estimator.on_audio_marker(k, audio_ns);
		estimator.on_video_marker("exact", k, audio_ns + to_ns(exact[k]));
		estimator.on_video_marker("over", k, audio_ns + to_ns(over[k]));
	}

	CHECK(row(estimator, "exact").state == CameraState::Stable);
	CHECK(row(estimator, "exact").reading_ms == 120);
	CHECK(row(estimator, "over").state == CameraState::Settling);
}

TEST_CASE("estimator makes five samples within F Stable at their median, halves away from zero")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("cam");

	// The median differs from the mean (136.2) and from the latest sample (125).
	double sign = 1;
	int64_t expected = 0;
	SUBCASE("median 141.5")
	{
		sign = 1;
		expected = 142;
	}
	SUBCASE("median -141.5")
	{
		sign = -1;
		expected = -142;
	}

	const double offsets[] = {120, 141.5, 149.5, 145, 125};
	for (uint32_t k = 0; k < 4; k++)
		feed(estimator, "cam", k, ms(10000) + k * cycle_ns, sign * offsets[k]);
	CHECK(row(estimator, "cam").state == CameraState::Settling);

	feed(estimator, "cam", 4, ms(10000) + 4 * cycle_ns, sign * offsets[4]);
	CHECK(row(estimator, "cam").state == CameraState::Stable);
	CHECK(row(estimator, "cam").reading_ms == expected);
	CHECK(row(estimator, "cam").samples == 5);
}

TEST_CASE("estimator decides stability on the five most recent samples by video time")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("cam");

	// The oldest of the five is 100 ms away from the others.
	const uint64_t first_ns = ms(10000);
	feed_all(estimator, "cam", 1, first_ns + cycle_ns, {200, 100, 100, 100, 100});
	REQUIRE(row(estimator, "cam").state == CameraState::Settling);

	// A sample from an earlier flash is not among the five most recent, so the 200 stays.
	feed(estimator, "cam", 0, first_ns, 100);
	CHECK(row(estimator, "cam").state == CameraState::Settling);

	// A sample from a later flash pushes the 200 out.
	feed(estimator, "cam", 6, first_ns + 6 * cycle_ns, 100);
	CHECK(row(estimator, "cam").state == CameraState::Stable);
	CHECK(row(estimator, "cam").reading_ms == 100);
}

TEST_CASE("estimator keeps a Stable camera's reading when its next five samples spread wider than F")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("cam");
	feed_all(estimator, "cam", 0, ms(10000), {142, 142, 142, 142, 142});
	REQUIRE(row(estimator, "cam").state == CameraState::Stable);

	const double offsets[] = {100, 200, 150, 300, 250};
	for (uint32_t k = 0; k < 5; k++) {
		CAPTURE(k);
		feed(estimator, "cam", 5 + k, ms(10000) + (5 + k) * cycle_ns, offsets[k]);
		CHECK(row(estimator, "cam").state == CameraState::Stable);
		CHECK(row(estimator, "cam").reading_ms == 142);
	}
}

TEST_CASE("estimator keeps a Stable camera's reading when its samples stop")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("cam");
	feed_all(estimator, "cam", 0, ms(10000), {142, 142, 142, 142, 142});
	REQUIRE(row(estimator, "cam").state == CameraState::Stable);

	// The phone faces away: the mic keeps hearing the pattern and the camera sees nothing.
	for (uint32_t k = 5; k < 20; k++)
		estimator.on_audio_marker(k, ms(10000) + k * cycle_ns);

	CHECK(row(estimator, "cam").state == CameraState::Stable);
	CHECK(row(estimator, "cam").reading_ms == 142);
}

TEST_CASE("estimator replaces a Stable camera's reading with the median of a later window within F")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("cam");
	feed_all(estimator, "cam", 0, ms(10000), {142, 142, 142, 142, 142});
	REQUIRE(row(estimator, "cam").reading_ms == 142);

	// Until the window holds only the new samples it spans more than F.
	const double offsets[] = {100, 101, 99, 100, 102};
	for (uint32_t k = 0; k < 4; k++) {
		CAPTURE(k);
		feed(estimator, "cam", 5 + k, ms(10000) + (5 + k) * cycle_ns, offsets[k]);
		CHECK(row(estimator, "cam").reading_ms == 142);
	}
	feed(estimator, "cam", 9, ms(10000) + 9 * cycle_ns, offsets[4]);
	CHECK(row(estimator, "cam").state == CameraState::Stable);
	CHECK(row(estimator, "cam").reading_ms == 100);
}

TEST_CASE("estimator shows the latest sample of a camera that was never stable as its live value")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("cam");

	// Seven samples that never agree: the window turns over and the camera stays Settling.
	const double offsets[] = {100, 150, 200, 250, 300, 141.5, 90.4};
	const int64_t live[] = {100, 150, 200, 250, 300, 142, 90};
	for (uint32_t k = 0; k < 7; k++) {
		CAPTURE(k);
		feed(estimator, "cam", k, ms(10000) + k * cycle_ns, offsets[k]);
		CHECK(row(estimator, "cam").state == CameraState::Settling);
		CHECK(row(estimator, "cam").live_ms == live[k]);
	}
}

TEST_CASE("estimator returns the reading and spread when a video marker makes a camera Stable or changes its reading")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("cam");

	// A video marker with no audio marker forms nothing.
	SampleResult result = estimator.on_video_marker("cam", 100, ms(9000));
	CHECK_FALSE(result.formed_sample);
	CHECK_FALSE(result.stable_changed);

	const double offsets[] = {120, 141.5, 149.5, 145, 125};
	for (uint32_t k = 0; k < 4; k++) {
		CAPTURE(k);
		result = feed(estimator, "cam", k, ms(10000) + k * cycle_ns, offsets[k]);
		CHECK(result.formed_sample);
		CHECK_FALSE(result.stable_changed);
	}

	// The fifth makes it Stable at 142; the window spans 149.5 - 120 = 29.5, rounded to 30.
	result = feed(estimator, "cam", 4, ms(10000) + 4 * cycle_ns, offsets[4]);
	CHECK(result.formed_sample);
	CHECK(result.stable_changed);
	CHECK(result.reading_ms == 142);
	CHECK(result.spread_ms == 30);

	// A window with the same median changes nothing.
	result = feed(estimator, "cam", 5, ms(10000) + 5 * cycle_ns, 141.5);
	CHECK(result.formed_sample);
	CHECK_FALSE(result.stable_changed);

	// The window 149.5, 145, 125, 141.5, 150 has its median at 145 and spans 25.
	result = feed(estimator, "cam", 6, ms(10000) + 6 * cycle_ns, 150);
	CHECK(result.stable_changed);
	CHECK(result.reading_ms == 145);
	CHECK(result.spread_ms == 25);
}

TEST_CASE("estimator returns each camera an audio marker makes Stable")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(f_30fps);
	estimator.add_camera("a");
	estimator.add_camera("b");
	estimator.add_camera("c");

	// Four samples each: a at 100, b at 40 to 50, c far apart.
	feed_all(estimator, "a", 0, ms(10000), {100, 100, 100, 100});
	feed_all(estimator, "b", 10, ms(20000), {40, 50, 45, 42});
	feed_all(estimator, "c", 20, ms(30000), {0, 100, 200, 300});

	// The fifth video markers wait for one audio marker.
	const uint64_t audio_ns = ms(40000);
	estimator.on_video_marker("a", 30, audio_ns + ms(100));
	estimator.on_video_marker("b", 30, audio_ns + ms(44));
	estimator.on_video_marker("c", 30, audio_ns + ms(400));
	const std::vector<StableChange> changes = estimator.on_audio_marker(30, audio_ns);

	REQUIRE(changes.size() == 2);
	CHECK(changes[0].camera == "a");
	CHECK(changes[0].reading_ms == 100);
	CHECK(changes[0].spread_ms == 0);
	CHECK(changes[1].camera == "b");
	CHECK(changes[1].reading_ms == 44);
	CHECK(changes[1].spread_ms == 10);

	// An audio marker that changes no reading returns nothing.
	estimator.on_video_marker("a", 31, audio_ns + cycle_ns + ms(100));
	CHECK(estimator.on_audio_marker(31, audio_ns + cycle_ns).empty());
}

namespace {

// Where the lock-step batches meet. C++17 has no std::barrier.
class Barrier {
public:
	explicit Barrier(int count) : count_(count) {}

	void arrive_and_wait()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		const int generation = generation_;
		if (++waiting_ == count_) {
			waiting_ = 0;
			generation_++;
			woken_.notify_all();
			return;
		}
		woken_.wait(lock, [&] { return generation_ != generation; });
	}

private:
	std::mutex mutex_;
	std::condition_variable woken_;
	const int count_;
	int waiting_ = 0;
	int generation_ = 0;
};

} // namespace

TEST_CASE("estimator records every marker that four camera threads and the mic thread push")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(1000.0 / 60);
	const std::string cameras[] = {"cam0", "cam1", "cam2", "cam3"};
	const uint64_t offsets_ms[] = {40, 80, 120, 160};
	for (const std::string &camera : cameras)
		estimator.add_camera(camera);

	// 1,000 flashes in batches of 100: every thread pushes a batch, then waits for the others, so a video marker
	// never waits for more than a batch and no audio marker it needs is dropped.
	constexpr uint32_t flashes = 1000;
	constexpr uint32_t batch = 100;
	const uint64_t first_ns = ms(10000);
	Barrier barrier(5);

	std::vector<std::thread> threads;
	for (size_t j = 0; j < 4; j++) {
		threads.emplace_back([&, j] {
			for (uint32_t k = 0; k < flashes; k++) {
				const uint64_t audio_ns = first_ns + k * cycle_ns;
				estimator.on_video_marker(cameras[j], k % 256, audio_ns + ms(offsets_ms[j]));
				if ((k + 1) % batch == 0)
					barrier.arrive_and_wait();
			}
		});
	}
	threads.emplace_back([&] {
		for (uint32_t k = 0; k < flashes; k++) {
			estimator.on_audio_marker(k % 256, first_ns + k * cycle_ns);
			if ((k + 1) % batch == 0)
				barrier.arrive_and_wait();
		}
	});
	for (std::thread &thread : threads)
		thread.join();

	for (size_t j = 0; j < 4; j++) {
		CAPTURE(cameras[j]);
		const CameraRow camera = row(estimator, cameras[j]);
		CHECK(camera.samples_formed == flashes);
		CHECK(camera.state == CameraState::Stable);
		CHECK(camera.reading_ms == int64_t(offsets_ms[j]));
	}
}

TEST_CASE("estimator snapshots show each camera as it was at one instant while four threads push markers")
{
	ReadingEstimator estimator;
	estimator.set_pattern(pattern);
	estimator.set_frame_interval_ms(1000.0 / 60);

	// Cameras 0 and 1 see steady samples and turn Stable. Cameras 2 and 3 alternate 60 ms apart and stay Settling.
	// Every 30 flashes each camera is cleared and then sees five flashes the mic does not hear, so it is Looking
	// for a while. Each camera's times lie 1,000 s from the others', so no camera pairs with another's audio
	// markers.
	const std::string cameras[] = {"cam0", "cam1", "cam2", "cam3"};
	const uint64_t offsets_ms[] = {40, 80, 120, 160};
	const bool steady[] = {true, true, false, false};
	for (const std::string &camera : cameras)
		estimator.add_camera(camera);

	std::atomic<bool> stop{false};
	std::atomic<int> started{0};
	std::vector<std::thread> writers;
	for (size_t j = 0; j < 4; j++) {
		writers.emplace_back([&, j] {
			for (uint32_t k = 0; !stop; k++) {
				const uint64_t audio_ns = ms(10000) + j * ms(1000000) + k * cycle_ns;
				const uint64_t jitter_ms = steady[j] || k % 2 ? 0 : 60;
				const uint64_t video_ns = audio_ns + ms(offsets_ms[j] + jitter_ms);
				if (k % 30 == 0)
					estimator.clear_camera(cameras[j]);
				if (k % 30 >= 5)
					estimator.on_audio_marker(k % 256, audio_ns);
				estimator.on_video_marker(cameras[j], k % 256, video_ns);
				if (k == 30)
					started++;
			}
		});
	}
	while (started < 4)
		std::this_thread::yield();

	// At least 10,000 snapshots, and more until each state has been seen.
	uint32_t taken = 0, looking = 0, settling = 0, stable = 0, mixed = 0;
	CameraRow first_mixed;
	while (taken < 10000 || ((!looking || !settling || !stable) && taken < 1000000)) {
		for (const CameraRow &camera : estimator.snapshot().cameras) {
			const size_t j = size_t(camera.id[3] - '0');
			const bool reads_offset = steady[j] && camera.reading_ms == int64_t(offsets_ms[j]);
			bool one_instant = camera.samples <= camera.samples_formed;
			switch (camera.state) {
			case CameraState::Looking:
				looking++;
				one_instant = one_instant && camera.samples == 0 && camera.samples_formed == 0;
				break;
			case CameraState::Settling:
				settling++;
				one_instant = one_instant && camera.samples >= 1 && camera.samples <= 5;
				break;
			case CameraState::Stable:
				stable++;
				one_instant = one_instant && camera.samples == 5 && reads_offset;
				break;
			}
			if (!one_instant && mixed++ == 0)
				first_mixed = camera;
		}
		taken++;
	}
	stop = true;
	for (std::thread &writer : writers)
		writer.join();

	CAPTURE(first_mixed.id);
	CAPTURE(int(first_mixed.state));
	CAPTURE(first_mixed.samples);
	CAPTURE(first_mixed.reading_ms);
	CHECK(taken >= 10000);
	CHECK(mixed == 0);
	CHECK(looking > 0);
	CHECK(settling > 0);
	CHECK(stable > 0);
}
