// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

#include "detector/qr-payload.hpp"
#include "detector/video-detector.hpp"
#include "log.hpp"
#include "media.hpp"

namespace camera_sync {

// A monotonic clock in ns. The core never reads the system's clock: the plugin injects one and the tests move a fake.
using Clock = std::function<uint64_t()>;

// Where a camera's tap delivers its frames: the first plane, the frame's size and how its pixels are laid out.
class FrameSink {
public:
	virtual ~FrameSink() = default;
	virtual void on_frame(const VideoFrame &frame, uint32_t width, uint32_t height, LumaFormat format) = 0;
};

// One camera's detection: a video detector of its own, fed by the camera's tap on the tap's thread. It sizes the
// detector on the first frame and again whenever the frame's size or layout changes, and remembers when the camera
// last delivered a frame.
class CameraPipeline : public FrameSink {
public:
	CameraPipeline(Logger log, Clock clock, VideoMarkerFn on_marker);

	// Reads one frame. A frame with no data, no width or no height is dropped with a warning.
	void on_frame(const VideoFrame &frame, uint32_t width, uint32_t height, LumaFormat format) override;

	// The clock reading when the last frame arrived, or when the pipeline was built if none has. Any thread reads it.
	uint64_t last_frame_ns() const { return last_frame_ns_.load(); }

	// The last QR code the camera's detector decoded. Read it on the thread that delivers the frames.
	const QrPayload &qr_data() const { return detector_.qr_data(); }

private:
	Logger log_;
	Clock clock_;
	VideoDetector detector_;
	bool started_ = false;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	LumaFormat format_ = LumaFormat::Luma8;
	std::atomic<uint64_t> last_frame_ns_;
};

} // namespace camera_sync
