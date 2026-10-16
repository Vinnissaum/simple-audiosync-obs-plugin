// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "detector/audio-detector.hpp"
#include "detector/video-detector.hpp"
#include "reading/camera-pipeline.hpp"
#include "reading/estimator.hpp"
#include "reading/mic-pipeline.hpp"
#include "reading/pattern.hpp"
#include "support/fixtures.hpp"
#include "support/pattern-timeline.hpp"

using camera_sync::AudioBlock;
using camera_sync::AudioMarker;
using camera_sync::CameraPipeline;
using camera_sync::CameraRow;
using camera_sync::CameraState;
using camera_sync::MicPipeline;
using camera_sync::PatternParams;
using camera_sync::ReadingEstimator;
using camera_sync::SourceId;
using camera_sync::VideoFrame;
using camera_sync::VideoMarker;
using camera_sync::VideoMarkerFn;

namespace {

constexpr uint32_t sample_rate = 48000;
constexpr uint32_t block_frames = 480; // 10 ms
constexpr uint64_t one_ms = 1000000;

// The session's part, done by hand: the first video marker's pattern goes to the estimator and to the mic, and every
// marker goes to the estimator.
struct Reading {
	ReadingEstimator estimator;
	MicPipeline mic;
	bool pattern_set = false;

	Reading()
		: mic(nullptr, sample_rate, [this](const AudioMarker &marker) {
			  estimator.on_audio_marker(uint32_t(marker.index), marker.timestamp);
		  })
	{
		estimator.set_frame_interval_ms(camera_sync::frame_interval_ms(60, 1));
	}

	VideoMarkerFn camera(const SourceId &id)
	{
		estimator.add_camera(id);
		return [this, id](const VideoMarker &marker) {
			if (!pattern_set) {
				pattern_set = true;
				estimator.set_pattern(PatternParams::from(marker.qr_data));
				mic.set_pattern(marker.qr_data.f, marker.qr_data.c, marker.qr_data.q_ms);
			}
			estimator.on_video_marker(id, marker.qr_data.index, marker.timestamp);
		};
	}

	// The mic hears the pattern's sound from time 0.
	void hear_sound()
	{
		const std::vector<float> clip = read_audio_clip();
		for (size_t first = 0; first < clip.size(); first += block_frames) {
			const uint32_t frames =
				static_cast<uint32_t>(std::min<size_t>(block_frames, clip.size() - first));
			mic.on_audio(AudioBlock{{clip.data() + first, nullptr},
						frames,
						camera_sync::mul_div64(first, 1000000000ULL, sample_rate)});
		}
	}

	CameraRow row(const SourceId &id) const
	{
		for (const CameraRow &row : estimator.snapshot().cameras)
			if (row.id == id)
				return row;
		FAIL("no row for camera ", id);
		return {};
	}
};

uint64_t no_clock()
{
	return 0;
}

// A camera whose video trails the sound by offset_ms sees the four cycles with frame 0 shown offset_ms after the
// sound's time 0.
void see_pattern(CameraPipeline &camera, uint64_t offset_ms)
{
	play(read_pattern_timeline(offset_ms * one_ms), camera);
}

// A camera pointed away from the phone delivers frames at the same times, with no pattern in them.
void see_no_pattern(CameraPipeline &camera)
{
	const std::vector<uint8_t> grey(320 * 180, 127);
	for (const TimedFrame &timed : read_pattern_timeline(0).sequence)
		camera.on_frame(VideoFrame{grey.data(), 320, timed.timestamp}, 320, 180,
				camera_sync::LumaFormat::Luma8);
}

} // namespace

TEST_CASE("estimator reads two cameras' offsets from the pattern fixtures through their pipelines")
{
	Reading reading;
	CameraPipeline a(nullptr, no_clock, reading.camera("cam-a"));
	CameraPipeline b(nullptr, no_clock, reading.camera("cam-b"));

	see_pattern(a, 100);
	see_pattern(b, 40);
	reading.hear_sound();

	// READ-22: +100 ms and +40 ms, each within 1 ms. Each window holds its own camera's four samples only.
	const CameraRow row_a = reading.row("cam-a");
	CHECK(row_a.state == CameraState::Settling);
	CHECK(row_a.samples == 4);
	CHECK(row_a.samples_formed == 4);
	CHECK(std::abs(row_a.live_ms - 100) <= 1);

	const CameraRow row_b = reading.row("cam-b");
	CHECK(row_b.state == CameraState::Settling);
	CHECK(row_b.samples == 4);
	CHECK(row_b.samples_formed == 4);
	CHECK(std::abs(row_b.live_ms - 40) <= 1);
}

TEST_CASE("estimator leaves a camera that never sees the pattern Looking while two others read theirs")
{
	Reading reading;
	CameraPipeline a(nullptr, no_clock, reading.camera("cam-a"));
	CameraPipeline b(nullptr, no_clock, reading.camera("cam-b"));
	CameraPipeline c(nullptr, no_clock, reading.camera("cam-c"));

	see_pattern(a, 100);
	see_pattern(b, 40);
	see_no_pattern(c);
	reading.hear_sound();

	const CameraRow row_c = reading.row("cam-c");
	CHECK(row_c.state == CameraState::Looking);
	CHECK(row_c.samples == 0);
	CHECK(row_c.samples_formed == 0);

	const CameraRow row_a = reading.row("cam-a");
	CHECK(row_a.state == CameraState::Settling);
	CHECK(row_a.samples == 4);
	CHECK(std::abs(row_a.live_ms - 100) <= 1);

	const CameraRow row_b = reading.row("cam-b");
	CHECK(row_b.state == CameraState::Settling);
	CHECK(row_b.samples == 4);
	CHECK(std::abs(row_b.live_ms - 40) <= 1);
}
