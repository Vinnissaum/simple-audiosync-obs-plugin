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
#include "detector/audio-detector.hpp"

#include <cmath>
#include <cstdio>
#include <utility>

namespace camera_sync {

static constexpr uint32_t N_AUDIO_SYMBOLS = 16;
static constexpr uint32_t N_SYMBOL_BUFFER = 20;

static constexpr double PI = 3.14159265358979323846;

static std::pair<int32_t, int32_t> operator-(std::pair<int32_t, int32_t> a, std::pair<int32_t, int32_t> b)
{
	return std::make_pair(a.first - b.first, a.second - b.second);
}

static std::complex<float> int16_to_complex(std::pair<int32_t, int32_t> x)
{
	return std::complex<float>((float)x.first / 32768.0f, (float)x.second / 32768.0f);
}

static uint32_t crc4_check(uint32_t data, uint32_t size)
{
	uint32_t p = 0x13 << (size - 5);
	while (size > 4) {
		if (data & (1 << (size - 1)))
			data ^= p;
		size--;
		p >>= 1;
	}
	return data;
}

AudioDetector::AudioDetector(Logger log, AudioMarkerFn on_marker)
	: log_(std::move(log)),
	  on_marker_(std::move(on_marker))
{
}

void AudioDetector::start(uint32_t sample_rate)
{
	audio_sample_rate_ = sample_rate;
}

void AudioDetector::set_pattern(uint32_t f, uint32_t c, uint32_t q_ms)
{
	std::unique_lock<std::mutex> lock(mutex_);
	f_ = f;
	c_ = c;
	q_ms_ = q_ms;
}

void AudioDetector::decode_data(std::complex<float> phase, uint64_t ts)
{
	uint32_t symbol_num = audio_sample_rate_ * c_last_;
	uint32_t symbol_den = f_last_;

	uint16_t index = 0;
	for (int i = 0; i < 12; i += 2) {
		auto s0 = audio_buffer_.sum(symbol_num * i / 2 / symbol_den);
		auto s1 = audio_buffer_.sum(symbol_num * (i / 2 + 1) / symbol_den);
		auto x = int16_to_complex(s0 - s1);
		auto real = (x / phase).real();
		auto imag = (x / phase).imag();
		if (real > 0.0f)
			index = (uint16_t)(index | (1 << i));
		if (imag > 0.0f)
			index = (uint16_t)(index | (2 << i));
	}

	auto crc4 = crc4_check(0xF0000 | index, 20);

	if (crc4 != 0) {
		char message[96];
		snprintf(message, sizeof(message),
			 "st_raw_audio_decode_data: CRC mismatch: received data=0x%03X crc=0x%X", (unsigned)index,
			 (unsigned)crc4);
		write_log(log_, LogLevel::Debug, message);
		return;
	}

	if (!on_marker_)
		return;

	AudioMarker data;
	data.timestamp = ts;
	data.index = index >> 4;
	data.score = 0.0f;
	on_marker_(data);
}

void AudioDetector::test_preamble(uint64_t ts)
{
	uint32_t f = f_last_;
	uint32_t c1 = c_last_ / 2;
	uint64_t symbol_ns = mul_div64(c1, 1000000000ULL, f);
	size_t buffer_length = (size_t)(audio_sample_rate_ * c1 * N_SYMBOL_BUFFER / f);

	/* Test the preamble pattern 0xF0  */
	auto s0 = audio_buffer_.sum(0);
	auto s4 = audio_buffer_.sum(buffer_length * 4 / N_SYMBOL_BUFFER);
	auto s8 = audio_buffer_.sum(buffer_length * 8 / N_SYMBOL_BUFFER);
	auto s12 = audio_buffer_.sum(buffer_length * 12 / N_SYMBOL_BUFFER);

	float det8_0 = std::abs(int16_to_complex(s4 - s0) - int16_to_complex(s8 - s4));
	float det12_8 = det8_0 * 0.5f - std::abs(int16_to_complex(s12 - s8));
	float det = det8_0 + det12_8;

	if (audio_marker_finder_.append(det, ts, symbol_ns * 12)) {
		auto s12 = audio_buffer_.sum(buffer_length * 12 / N_SYMBOL_BUFFER);
		auto s16 = audio_buffer_.sum(buffer_length * 16 / N_SYMBOL_BUFFER);
		auto s20 = audio_buffer_.sum(buffer_length * 20 / N_SYMBOL_BUFFER);

		auto x = int16_to_complex(s16 - s20) - int16_to_complex(s12 - s16);
		x *= std::complex(1.0f, -1.0f);

		ts = audio_marker_finder_.last_ts - symbol_ns * N_AUDIO_SYMBOLS / 2;

		decode_data(x / std::abs(x), ts);
	}
}

void AudioDetector::process(const AudioBlock &block)
{
	std::unique_lock<std::mutex> lock(mutex_);
	uint32_t f = f_;
	uint32_t c = c_;
	uint32_t q_ms = q_ms_;
	lock.unlock();

	if (f <= 0 || c <= 0)
		return;

	if (f != f_last_ || c != c_last_) {
		f_last_ = f;
		c_last_ = c;
		audio_buffer_.buffer.clear();
	}

	if (q_ms > 0)
		audio_marker_finder_.dumping_range = q_ms * 1000000 * 6 * 2;

	float phase = (float)(block.timestamp % 1000000000) * (float)(1e-9 * 2 * PI * f);
	float phase_step = (float)(2 * PI * f) / (float)audio_sample_rate_;
	size_t buffer_length = (size_t)(audio_sample_rate_ * c * N_SYMBOL_BUFFER / f);

	for (uint32_t i = 0; i < block.frames; i++) {
		float osc0 = sinf(phase + phase_step * (float)i);
		float osc1 = cosf(phase + phase_step * (float)i);
		uint64_t ts = block.timestamp + mul_div64(i, 1000000000ULL, audio_sample_rate_);

		float v0 = block.data[0][i];
		float v1 = block.data[1] ? block.data[1][i] : 0.0f;
		int16_t vr = (int16_t)((v0 * osc0 - v1 * osc1) * 16383.0f);
		int16_t vi = (int16_t)((v0 * osc1 + v1 * osc0) * 16383.0f);
		audio_buffer_.push_back(vr, vi, buffer_length);

		if (audio_buffer_.buffer.size() < buffer_length)
			continue;

		test_preamble(ts);
	}
}

} // namespace camera_sync
