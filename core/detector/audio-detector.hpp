/*
OBS Audio Video Sync Dock
Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
// Derived from Audio Video Sync Dock, src/sync-test-output.cpp at e4ad2941ed5f2982e9f17b76ebaf22096319f3cb
// Changed for Camera Sync: the audio half of the detector. libobs's audio struct, mul_div and blog are core types and an injected Logger,
// the output wrapper and its signals are a marker callback, and the timestamps are the blocks' own. The pairing with
// video markers is not here. The detection is unchanged.
#pragma once

#include <cstddef>
#include <complex>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>

#include "detector/peak-finder.hpp"
#include "log.hpp"
#include "media.hpp"

namespace camera_sync {

struct AudioBuffer {
	std::deque<std::pair<int32_t, int32_t>> buffer;

	void push_back(int16_t xr, int16_t xi, size_t length)
	{
		int32_t vr = xr, vi = xi;
		if (buffer.size()) {
			vr += buffer.back().first;
			vi += buffer.back().second;
		}
		buffer.push_back(std::make_pair(vr, vi));

		if (buffer.size() <= length)
			return;

		buffer.pop_front();
	};

	std::pair<int32_t, int32_t> sum(size_t n_from_last)
	{
		if (buffer.size() <= 0)
			return std::make_pair(0, 0);
		if (n_from_last >= buffer.size())
			return buffer[0];
		return buffer[buffer.size() - n_from_last - 1];
	}
};

// One sound pulse of the pattern: the time it starts and the index it carries.
struct AudioMarker {
	uint64_t timestamp;
	int index;
	float score;
};

using AudioMarkerFn = std::function<void(const AudioMarker &)>;

// Finds the pattern's sound pulses in the mic's samples. It needs the pattern's carrier frequency f, its cycles per
// symbol c and the QR frames' duration q_ms, which a camera reads from the pattern's QR codes.
class AudioDetector {
public:
	AudioDetector(Logger log, AudioMarkerFn on_marker);
	AudioDetector(const AudioDetector &) = delete;
	AudioDetector &operator=(const AudioDetector &) = delete;

	// Sets the sample rate of the blocks that follow.
	void start(uint32_t sample_rate);

	// Sets the pattern's parameters. It may be called from another thread than process.
	void set_pattern(uint32_t f, uint32_t c, uint32_t q_ms);

	// Reads one block of samples. Blocks arrive in time order. Nothing is read until the pattern is set.
	void process(const AudioBlock &block);

private:
	void decode_data(std::complex<float> phase, uint64_t ts);
	void test_preamble(uint64_t ts);

	Logger log_;
	AudioMarkerFn on_marker_;

	uint32_t audio_sample_rate_ = 0;

	AudioBuffer audio_buffer_;
	PeakFinder audio_marker_finder_;

	std::mutex mutex_;

	/* Audio pattern information from video to audio */
	uint32_t f_ = 0;
	uint32_t c_ = 0;
	uint32_t q_ms_ = 0;

	uint32_t f_last_ = 0;
	uint32_t c_last_ = 0;
};

} // namespace camera_sync
