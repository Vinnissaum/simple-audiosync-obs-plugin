// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "detector/audio-detector.hpp"
#include "support/fixtures.hpp"

using camera_sync::AudioBlock;
using camera_sync::AudioDetector;
using camera_sync::AudioMarker;

static constexpr uint32_t SAMPLE_RATE = 48000;
static constexpr uint32_t BLOCK_FRAMES = 480; // 10 ms, a block size OBS's audio thread produces

// Feeds mono samples to the detector in blocks, stamping each block with the time of its first sample.
static std::vector<AudioMarker> decode(const std::vector<float> &samples, uint32_t f, uint32_t c, uint32_t q_ms)
{
	std::vector<AudioMarker> found;
	AudioDetector detector(nullptr, [&](const AudioMarker &marker) { found.push_back(marker); });
	detector.start(SAMPLE_RATE);
	detector.set_pattern(f, c, q_ms);

	for (size_t first = 0; first < samples.size(); first += BLOCK_FRAMES) {
		const uint32_t frames = static_cast<uint32_t>(std::min<size_t>(BLOCK_FRAMES, samples.size() - first));
		AudioBlock block{{samples.data() + first, nullptr},
				 frames,
				 camera_sync::mul_div64(first, 1000000000ULL, SAMPLE_RATE)};
		detector.process(block);
	}
	return found;
}

TEST_CASE("audio detector decodes the indices of Sync Dock's sound in order")
{
	const std::vector<float> clip = read_audio_clip();
	REQUIRE(clip.size() == 2 * SAMPLE_RATE);

	const std::vector<int> expected = read_audio_indices();
	REQUIRE(expected.size() == 10);

	const std::vector<AudioMarker> found = decode(clip, 884, 2, 66);

	std::vector<int> indices;
	for (const AudioMarker &marker : found)
		indices.push_back(marker.index);
	CHECK(indices == expected);
}

TEST_CASE("audio detector finds no marker in 2 seconds of silence")
{
	const std::vector<float> silence(2 * SAMPLE_RATE, 0.0f);

	CHECK(decode(silence, 884, 2, 66).empty());
}
