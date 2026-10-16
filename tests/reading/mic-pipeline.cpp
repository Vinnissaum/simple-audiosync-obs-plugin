// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "reading/mic-pipeline.hpp"
#include "support/fixtures.hpp"

using camera_sync::AudioBlock;
using camera_sync::AudioMarker;
using camera_sync::AudioSink;
using camera_sync::MicPipeline;

namespace {

constexpr uint32_t sample_rate = 48000;
constexpr uint32_t block_frames = 480; // 10 ms, a block size OBS's audio thread produces
constexpr uint64_t clip_ns = 2000000000;

// Sends the clip through the pipeline's AudioSink, the only way the mic's tap reaches it, in 10 ms blocks whose first
// sample is at start_ns. With stereo set, the second plane carries the same samples as the first.
void feed(AudioSink &sink, const std::vector<float> &samples, uint64_t start_ns, bool stereo = false)
{
	for (size_t first = 0; first < samples.size(); first += block_frames) {
		const uint32_t frames = static_cast<uint32_t>(std::min<size_t>(block_frames, samples.size() - first));
		const float *plane = samples.data() + first;
		AudioBlock block{{plane, stereo ? plane : nullptr},
				 frames,
				 start_ns + camera_sync::mul_div64(first, 1000000000ULL, sample_rate)};
		sink.on_audio(block);
	}
}

std::vector<int> indices_of(const std::vector<AudioMarker> &markers)
{
	std::vector<int> indices;
	for (const AudioMarker &marker : markers)
		indices.push_back(marker.index);
	return indices;
}

} // namespace

TEST_CASE("mic pipeline emits no marker before the pattern is set")
{
	const std::vector<float> clip = read_audio_clip();
	std::vector<AudioMarker> found;
	MicPipeline pipeline(nullptr, sample_rate, [&](const AudioMarker &marker) { found.push_back(marker); });

	feed(pipeline, clip, 0);

	CHECK(found.empty());
}

TEST_CASE("mic pipeline decodes the pattern's indices in order once the pattern is set")
{
	const std::vector<float> clip = read_audio_clip();
	const std::vector<int> expected = read_audio_indices();
	REQUIRE(expected == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
	std::vector<AudioMarker> found;
	MicPipeline pipeline(nullptr, sample_rate, [&](const AudioMarker &marker) { found.push_back(marker); });

	feed(pipeline, clip, 0);
	REQUIRE(found.empty());

	pipeline.set_pattern(884, 2, 66);
	feed(pipeline, clip, clip_ns);

	CHECK(indices_of(found) == expected);
}

TEST_CASE("mic pipeline takes the pattern from another thread while it reads blocks")
{
	const std::vector<float> clip = read_audio_clip();
	std::vector<AudioMarker> found;
	MicPipeline pipeline(nullptr, sample_rate, [&](const AudioMarker &marker) { found.push_back(marker); });

	// A camera thread sets the pattern before the first block, then keeps setting it while the mic thread reads.
	std::atomic<bool> set{false};
	std::atomic<bool> done{false};
	std::thread camera([&] {
		pipeline.set_pattern(884, 2, 66);
		set = true;
		while (!done) {
			pipeline.set_pattern(884, 2, 66);
			std::this_thread::yield();
		}
	});
	while (!set)
		std::this_thread::yield();
	feed(pipeline, clip, 0);
	done = true;
	camera.join();

	CHECK(indices_of(found) == read_audio_indices());
}

TEST_CASE("mic pipeline decodes a stereo block the same as the mono block of the same samples")
{
	const std::vector<float> clip = read_audio_clip();
	std::vector<AudioMarker> mono_found;
	std::vector<AudioMarker> stereo_found;
	MicPipeline mono(nullptr, sample_rate, [&](const AudioMarker &marker) { mono_found.push_back(marker); });
	MicPipeline stereo(nullptr, sample_rate, [&](const AudioMarker &marker) { stereo_found.push_back(marker); });
	mono.set_pattern(884, 2, 66);
	stereo.set_pattern(884, 2, 66);

	feed(mono, clip, 0);
	feed(stereo, clip, 0, true);

	REQUIRE(indices_of(mono_found) == read_audio_indices());
	REQUIRE(stereo_found.size() == mono_found.size());
	for (size_t i = 0; i < mono_found.size(); i++) {
		CAPTURE(i);
		CHECK(stereo_found[i].index == mono_found[i].index);
		CHECK(stereo_found[i].timestamp == mono_found[i].timestamp);
	}
}
