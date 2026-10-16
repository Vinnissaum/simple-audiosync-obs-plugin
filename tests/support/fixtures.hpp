// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

// The fixtures in tests/fixtures, written by generate.sh with Sync Dock's generator.
inline std::string fixture_path(const std::string &name)
{
	return std::string(CAMERA_SYNC_FIXTURE_DIR) + "/" + name;
}

inline std::vector<uint8_t> read_binary(const std::string &name)
{
	std::ifstream in(fixture_path(name), std::ios::binary);
	REQUIRE_MESSAGE(in.good(), "cannot open fixture ", name);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// A binary PGM ("P5\n<width> <height>\n255\n" and one byte per pixel), which is how the luma plane is stored.
struct Pgm {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> luma;
};

inline Pgm read_pgm(const std::string &name)
{
	const std::vector<uint8_t> bytes = read_binary(name);
	const std::string text(bytes.begin(), bytes.end());
	std::istringstream header(text);

	std::string magic;
	unsigned width = 0, height = 0, maxval = 0;
	header >> magic >> width >> height >> maxval;
	REQUIRE(magic == "P5");
	REQUIRE(maxval == 255);

	Pgm pgm;
	pgm.width = width;
	pgm.height = height;
	const size_t pixels = static_cast<size_t>(width) * height;
	// One whitespace byte follows maxval, then the pixels.
	pgm.luma.assign(bytes.end() - static_cast<std::ptrdiff_t>(pixels), bytes.end());
	REQUIRE(pgm.luma.size() == pixels);
	return pgm;
}

// audio-48k-mono-s16le.pcm is the sound of Sync Dock's pattern q=4,f=884,c=2 at 60 fps: ten marker cycles of 0.2 s,
// 2 seconds of mono 16-bit samples at 48 kHz, carrying the indices in audio-indices.txt.
inline std::vector<float> read_audio_clip()
{
	const std::vector<uint8_t> bytes = read_binary("audio-48k-mono-s16le.pcm");
	std::vector<float> samples;
	for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
		const int16_t value = static_cast<int16_t>(bytes[i] | (bytes[i + 1] << 8));
		samples.push_back(static_cast<float>(value) / 32768.0f);
	}
	return samples;
}

inline std::vector<int> read_audio_indices()
{
	const std::vector<uint8_t> bytes = read_binary("audio-indices.txt");
	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	std::vector<int> indices;
	for (int index; in >> index;)
		indices.push_back(index);
	return indices;
}
