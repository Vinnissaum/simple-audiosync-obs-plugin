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
// Changed for Camera Sync: the video half of the detector. libobs's frame struct, format helpers, mul_div and blog are
// core types and an injected Logger, the output wrapper and its signals are a marker callback, and the timestamps are
// the frames' own. The detection is unchanged.
#include "detector/video-detector.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "quirc.h"

namespace camera_sync {

/* There are several reason to limit the width and the height.
 * - Since a square of 3/8 QR-code-length is calculated using uint32_t,
 *   the 3/8 of width or height cannot exceed the square root of uint32_t max.
 * - Since a sum of the pixels in a line is accumurated on uint32_t,
 *   the width must be less than 1/255 of uint32_t max.
 *   */
static constexpr uint32_t MAX_WIDTH_HEIGHT = 87378u;

static constexpr double SQRT1_2 = 0.70710678118654752440;

template<typename T> static T sq(T x)
{
	return x * x;
}

static uint8_t get_intensity_10le(const uint8_t *data)
{
	uint16_t v = (uint16_t)((data[0] >> 2) | (data[1] << 6));
	return (uint8_t)std::min<uint16_t>(v, 0xFF);
}

static inline uint32_t diff_u32(uint32_t x, uint32_t y)
{
	if (x < y)
		return y - x;
	else
		return x - y;
}

static inline uint32_t sqrt_u32(uint32_t x)
{
	uint32_t r = 0;
	for (uint32_t b = 1 << 15; b; b >>= 1) {
		if (sq(r | b) <= x)
			r |= b;
	}
	return r;
}

static inline int qrcode_length(const QrCorner *cc)
{
	auto l02 = std::hypot((float)((int)cc[0].x - (int)cc[2].x), (float)((int)cc[0].y - (int)cc[2].y));
	auto l13 = std::hypot((float)((int)cc[1].x - (int)cc[3].x), (float)((int)cc[1].y - (int)cc[3].y));
	return (int)((l02 + l13) * (float)(SQRT1_2 / 2.0f));
}

static inline void adjust_corners(QrCorner *cc)
{
	int cx = 0, cy = 0;
	for (int i = 0; i < 4; i++) {
		cx += (int)cc[i].x;
		cy += (int)cc[i].y;
	}

	cx /= 4;
	cy /= 4;
	int r = qrcode_length(cc) / 4;

	// Move (x, y) to center side so that the circles will cover the pattern.
	for (int i = 0; i < 4; i++) {
		cc[i].x = (uint32_t)((int)cc[i].x * 15 + cx * 9) / 24;
		cc[i].y = (uint32_t)((int)cc[i].y * 15 + cy * 9) / 24;
		cc[i].r = (uint32_t)r;
	}
}

VideoDetector::VideoDetector(Logger log, VideoMarkerFn on_marker)
	: log_(std::move(log)),
	  on_marker_(std::move(on_marker))
{
}

VideoDetector::~VideoDetector()
{
	if (qr_)
		quirc_destroy(qr_);
}

bool VideoDetector::start(uint32_t width, uint32_t height, LumaFormat format)
{
	video_pixelsize_ = 0;

	if (width > MAX_WIDTH_HEIGHT || height > MAX_WIDTH_HEIGHT) {
		write_log(log_, LogLevel::Error,
			  "Requested size " + std::to_string(width) + "x" + std::to_string(height) +
				  " exceeds maximum size " + std::to_string(MAX_WIDTH_HEIGHT) + "x" +
				  std::to_string(MAX_WIDTH_HEIGHT));
		return false;
	}
	video_width_ = width;
	video_height_ = height;

	uint32_t pixelsize, pixeloffset;
	switch (format) {
	case LumaFormat::Luma8:
		pixelsize = 1;
		pixeloffset = 0;
		video_get_intensity_ = nullptr;
		break;
	case LumaFormat::Luma10:
		pixelsize = 2;
		pixeloffset = 0;
		video_get_intensity_ = get_intensity_10le;
		break;
	case LumaFormat::Luma16:
		pixelsize = 2;
		pixeloffset = 1; // little endian
		video_get_intensity_ = nullptr;
		break;
	case LumaFormat::Green32:
		pixelsize = 4;
		pixeloffset = 1; // green channel
		video_get_intensity_ = nullptr;
		break;
	default:
		write_log(log_, LogLevel::Error, "unsupported pixel format " + std::to_string((int)format));
		return false;
	}

	uint32_t qr_width = video_width_;
	uint32_t qr_height = video_height_;
	qr_step_ = 1;
	while (qr_width * qr_height > 640 * 480) {
		qr_width /= 2;
		qr_height /= 2;
		qr_step_ *= 2;
	}
	if (!qr_)
		qr_ = quirc_new();
	if (!qr_) {
		write_log(log_, LogLevel::Error, "failed to create QR code encoding context");
		return false;
	}
	if (quirc_resize(qr_, (int)qr_width, (int)qr_height) < 0) {
		write_log(log_, LogLevel::Error, "failed to set-up QR code encoding context");
		return false;
	}

	video_pixelsize_ = pixelsize;
	video_pixeloffset_ = pixeloffset;
	return true;
}

void VideoDetector::decode_qrcode(const VideoFrame &frame)
{
	int w, h;
	auto qr = qr_;
	uint8_t *qrbuf = quirc_begin(qr, &w, &h);

	const auto qr_step = qr_step_;
	const auto pixelsize = video_pixelsize_ * qr_step;
	const uint8_t *linedata = frame.data + frame.linesize * (qr_step / 2);
	auto *ptr = qrbuf;
	for (int y = 0; y < h; y++) {
		const uint8_t *data = linedata + video_pixeloffset_ + video_pixelsize_ * (qr_step / 2);
		if (!video_get_intensity_) {
			for (int x = 0; x < w; x++) {
				*ptr++ = *data;
				data += pixelsize;
			}
		} else {
			for (int x = 0; x < w; x++) {
				*ptr++ = video_get_intensity_(data);
				data += pixelsize;
			}
		}

		linedata += frame.linesize * qr_step;
	}

	quirc_end(qr);

	int num_codes = quirc_count(qr);

	for (int i = 0; i < num_codes; i++) {
		// (x0, y0): top left
		// (x1, y1): top right
		// (x2, y2): bottom right
		// (x3, y3): bottom left

		struct quirc_code code;
		struct quirc_data data;
		quirc_extract(qr, i, &code);
		auto err = quirc_decode(&code, &data);
		if (err == QUIRC_ERROR_DATA_ECC) {
			quirc_flip(&code);
			err = quirc_decode(&code, &data);
		}

		if (err)
			continue;

		data.payload[QUIRC_MAX_PAYLOAD - 1] = 0;
		if (!qr_data_.decode((char *)data.payload, log_))
			continue;

		for (int j = 0; j < 4; j++) {
			qr_corners_[j].x = (uint32_t)code.corners[j].x * qr_step_;
			qr_corners_[j].y = (uint32_t)code.corners[j].y * qr_step_;
		}

		adjust_corners(qr_corners_);

		video_marker_max_ts_ = frame.timestamp + qr_data_.q_ms * 3 * 1000000;
		video_level_prev_ = 0;
	}
}

void VideoDetector::find_marker(const VideoFrame &frame)
{
	int64_t sum = 0;

	if (frame.timestamp > video_marker_max_ts_) {
		video_level_prev_ = 0;
		return;
	}

	const uint8_t *linedata = frame.data;
	const uint32_t pixelsize = video_pixelsize_;

	for (size_t i = 0; i < QR_CORNERS; i++) {
		const QrCorner c = qr_corners_[i];
		if (c.r == 0)
			return;
		uint32_t y0 = c.y > c.r ? c.y - c.r : 0;
		uint32_t y1 = std::min(c.y + c.r, video_height_);
		uint32_t sq_r = sq(c.r);

		for (uint32_t y = y0; y < y1; y++) {
			uint32_t dx = sqrt_u32(sq_r - sq(diff_u32(y, c.y)));
			uint32_t x0 = c.x > dx ? c.x - dx : 0;
			uint32_t x1 = std::min(c.x + dx, video_width_);

			const uint8_t *data =
				linedata + frame.linesize * y + video_pixeloffset_ + video_pixelsize_ * x0;

			uint32_t line_sum = 0;

			if (!video_get_intensity_) {
				for (uint32_t x = x0; x < x1; x++) {
					line_sum += *data;
					data += pixelsize;
				}
			} else {
				for (uint32_t x = x0; x < x1; x++) {
					line_sum += video_get_intensity_(data);
					data += pixelsize;
				}
			}

			if (i & 1)
				sum += line_sum;
			else
				sum -= line_sum;
		}
	}

	if (qr_data_.valid && video_level_prev_ < 0 && sum >= 0) {
		/* Calculate the time half frame later than the zero-cross of `sum`. */
		uint64_t t = frame.timestamp - video_level_prev_ts_;
		uint64_t add = mul_div64(t, (uint64_t)(sum - video_level_prev_ * 3),
					 (uint64_t)((sum - video_level_prev_) * 2));
		marker_found(video_level_prev_ts_ + add, (float)(sum - video_level_prev_));
	}
	video_level_prev_ = sum;
	video_level_prev_ts_ = frame.timestamp;
}

void VideoDetector::marker_found(uint64_t timestamp, float score)
{
	if (!on_marker_)
		return;

	VideoMarker marker;
	marker.timestamp = timestamp;
	marker.score = score;
	marker.qr_data = qr_data_;
	on_marker_(marker);
}

void VideoDetector::process(const VideoFrame &frame)
{
	if (!video_pixelsize_)
		return;

	decode_qrcode(frame);
	find_marker(frame);
}

} // namespace camera_sync
