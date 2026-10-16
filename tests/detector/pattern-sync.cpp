// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "detector/audio-detector.hpp"
#include "detector/video-detector.hpp"
#include "reading/camera-pipeline.hpp"
#include "reading/mic-pipeline.hpp"
#include "support/fixtures.hpp"
#include "support/pattern-timeline.hpp"

using camera_sync::AudioBlock;
using camera_sync::AudioMarker;
using camera_sync::CameraPipeline;
using camera_sync::MicPipeline;
using camera_sync::VideoMarker;

namespace {

constexpr uint32_t sample_rate = 48000;
constexpr uint32_t block_frames = 480; // 10 ms

// The mic hears the audio fixture from time 0, in 10 ms blocks.
void hear_clip(MicPipeline &mic)
{
	const std::vector<float> clip = read_audio_clip();
	for (size_t first = 0; first < clip.size(); first += block_frames) {
		const uint32_t frames = static_cast<uint32_t>(std::min<size_t>(block_frames, clip.size() - first));
		mic.on_audio(AudioBlock{{clip.data() + first, nullptr},
					frames,
					camera_sync::mul_div64(first, 1000000000ULL, sample_rate)});
	}
}

} // namespace

// The video cycles and the sound come from the same generator run, both from time 0, so each marker cycle's flash and
// its sound are generated at the same instant: the start of frame 2q of the cycle.
TEST_CASE("pattern video and audio fixtures give each index's markers within 1 ms of each other")
{
	std::vector<VideoMarker> video;
	CameraPipeline camera(
		nullptr, [] { return uint64_t(0); }, [&](const VideoMarker &marker) { video.push_back(marker); });
	play(read_pattern_timeline(0), camera);
	REQUIRE(video.size() == pattern_cycles);

	// The mic hears the pattern the camera read, as the session hands it over.
	std::vector<AudioMarker> audio;
	MicPipeline mic(nullptr, sample_rate, [&](const AudioMarker &marker) { audio.push_back(marker); });
	mic.set_pattern(video[0].qr_data.f, video[0].qr_data.c, video[0].qr_data.q_ms);
	hear_clip(mic);

	for (uint32_t index = 0; index < pattern_cycles; index++) {
		CAPTURE(index);
		const auto v = std::find_if(video.begin(), video.end(),
					    [&](const VideoMarker &marker) { return marker.qr_data.index == index; });
		const auto a = std::find_if(audio.begin(), audio.end(),
					    [&](const AudioMarker &marker) { return marker.index == int(index); });
		REQUIRE(v != video.end());
		REQUIRE(a != audio.end());
		const int64_t bias_ns = int64_t(v->timestamp) - int64_t(a->timestamp);
		CAPTURE(bias_ns);
		CHECK(std::abs(bias_ns) <= 1000000);
	}
}

TEST_CASE("pattern audio fixture decodes every marker with its generated index within 1 ms of its generated time")
{
	const std::vector<int> generated = read_audio_indices();
	REQUIRE(generated.size() == 10);

	// The pattern the clip was generated with, q=4,f=884,c=2 at 60 fps, as its QR codes carry it.
	std::vector<AudioMarker> audio;
	MicPipeline mic(nullptr, sample_rate, [&](const AudioMarker &marker) { audio.push_back(marker); });
	mic.set_pattern(884, 2, 66);
	hear_clip(mic);

	// READ-21: every audio marker of the clip, in order. The sound of cycle k carries generated[k] and is generated
	// from the start of frame 2q of the cycle.
	REQUIRE(audio.size() == generated.size());
	for (size_t k = 0; k < audio.size(); k++) {
		CAPTURE(k);
		CHECK(audio[k].index == generated[k]);
		const uint64_t generated_ns = pattern_frame_ns(0, k * pattern_cycle_frames + pattern_marker_frame);
		const int64_t error_ns = int64_t(audio[k].timestamp) - int64_t(generated_ns);
		CAPTURE(error_ns);
		CHECK(std::abs(error_ns) <= 1000000);
	}
}
