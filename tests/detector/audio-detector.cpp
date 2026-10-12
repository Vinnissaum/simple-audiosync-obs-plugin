// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "detector/audio-detector.hpp"
#include "fixtures.hpp"

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

// audio-48k-mono-s16le.pcm is the sound of Sync Dock's pattern q=4,f=884,c=2 at 60 fps: ten marker cycles of 0.2 s,
// 2 seconds of mono 16-bit samples at 48 kHz, carrying the indices in audio-indices.txt.
static std::vector<float> read_clip()
{
	const std::vector<uint8_t> bytes = read_binary("audio-48k-mono-s16le.pcm");
	std::vector<float> samples;
	for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
		const int16_t value = static_cast<int16_t>(bytes[i] | (bytes[i + 1] << 8));
		samples.push_back(static_cast<float>(value) / 32768.0f);
	}
	return samples;
}

static std::vector<int> read_indices()
{
	const std::vector<uint8_t> bytes = read_binary("audio-indices.txt");
	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	std::vector<int> indices;
	for (int index; in >> index;)
		indices.push_back(index);
	return indices;
}

TEST_CASE("audio detector decodes the indices of Sync Dock's sound in order")
{
	const std::vector<float> clip = read_clip();
	REQUIRE(clip.size() == 2 * SAMPLE_RATE);

	const std::vector<int> expected = read_indices();
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
