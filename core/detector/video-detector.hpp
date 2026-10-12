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
#pragma once

#include <cstdint>
#include <functional>

#include "detector/qr-payload.hpp"
#include "log.hpp"
#include "media.hpp"

struct quirc;

namespace camera_sync {

constexpr uint32_t QR_CORNERS = 4;

struct QrCorner {
	uint32_t x = 0, y = 0;
	uint32_t r = 0;
};

// One flash of the pattern: the time the flash crosses zero, the strength of the crossing, and the QR code it follows.
struct VideoMarker {
	uint64_t timestamp;
	float score;
	QrPayload qr_data;
};

using VideoMarkerFn = std::function<void(const VideoMarker &)>;

// Finds the pattern's QR codes in one camera's frames, and the flash after each one.
class VideoDetector {
public:
	VideoDetector(Logger log, VideoMarkerFn on_marker);
	~VideoDetector();
	VideoDetector(const VideoDetector &) = delete;
	VideoDetector &operator=(const VideoDetector &) = delete;

	// Sizes the detector for frames of this width, height and layout. False, logged, when they are not supported.
	bool start(uint32_t width, uint32_t height, LumaFormat format);

	// Reads one frame. Frames arrive in time order.
	void process(const VideoFrame &frame);

	// The last QR code that decoded and passed its range checks.
	const QrPayload &qr_data() const { return qr_data_; }

private:
	void decode_qrcode(const VideoFrame &frame);
	void find_marker(const VideoFrame &frame);
	void marker_found(uint64_t timestamp, float score);

	Logger log_;
	VideoMarkerFn on_marker_;

	uint32_t video_width_ = 0, video_height_ = 0;
	uint32_t video_pixelsize_ = 0;
	uint32_t video_pixeloffset_ = 0;
	uint8_t (*video_get_intensity_)(const uint8_t *data) = nullptr;

	struct quirc *qr_ = nullptr;
	uint32_t qr_step_ = 1;
	QrCorner qr_corners_[QR_CORNERS];
	QrPayload qr_data_;

	int64_t video_level_prev_ = 0;
	uint64_t video_level_prev_ts_ = 0;
	uint64_t video_marker_max_ts_ = 0;
};

} // namespace camera_sync
