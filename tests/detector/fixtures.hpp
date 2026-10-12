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
