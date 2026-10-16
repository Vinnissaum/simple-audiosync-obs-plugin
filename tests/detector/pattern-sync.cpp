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
	const std::vector<float> clip = read_audio_clip();
	for (size_t first = 0; first < clip.size(); first += block_frames) {
		const uint32_t frames = static_cast<uint32_t>(std::min<size_t>(block_frames, clip.size() - first));
		mic.on_audio(AudioBlock{{clip.data() + first, nullptr},
					frames,
					camera_sync::mul_div64(first, 1000000000ULL, sample_rate)});
	}

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
