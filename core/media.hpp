// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

namespace camera_sync {

// How the detector reads one pixel's intensity out of a frame's first plane. The plugin maps libobs's video formats
// onto these.
enum class LumaFormat {
	Luma8,   // 1 byte per pixel, the brightness: I420, NV12, I444, I422, I40A, I42A, YUVA
	Luma10,  // 2 bytes per pixel, a 10-bit little-endian value: I010
	Luma16,  // 2 bytes per pixel, the high byte second: P010, P216, P416
	Green32, // 4 bytes per pixel, the green channel second: RGBA, BGRA, BGRX
};

// One video frame as the detector reads it: the first plane only.
struct VideoFrame {
	const uint8_t *data;
	uint32_t linesize;
	uint64_t timestamp; // ns
};

// One block of audio samples as the detector reads it: planar 32-bit float, the first plane for a mono source and the
// first and second for a stereo one.
struct AudioBlock {
	const float *data[2]; // data[1] is null for mono
	uint32_t frames;
	uint64_t timestamp; // ns, of the first frame
};

// num * mul / div, rounded down, for the sample and frame times the detector computes in ns.
inline uint64_t mul_div64(uint64_t num, uint64_t mul, uint64_t div)
{
	const uint64_t rem = num % div;
	return (num / div) * mul + (rem * mul) / div;
}

} // namespace camera_sync
