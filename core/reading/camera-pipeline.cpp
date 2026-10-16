// SPDX-License-Identifier: GPL-2.0-or-later
#include "reading/camera-pipeline.hpp"

#include <string>
#include <utility>

namespace camera_sync {

CameraPipeline::CameraPipeline(Logger log, Clock clock, VideoMarkerFn on_marker)
	: log_(std::move(log)),
	  clock_(std::move(clock)),
	  detector_(log_, std::move(on_marker)),
	  last_frame_ns_(clock_())
{
}

void CameraPipeline::on_frame(const VideoFrame &frame, uint32_t width, uint32_t height, LumaFormat format)
{
	if (!frame.data || width == 0 || height == 0) {
		write_log(log_, LogLevel::Warning,
			  "camera frame dropped: width=" + std::to_string(width) + " height=" + std::to_string(height) +
				  (frame.data ? "" : " no data"));
		return;
	}

	last_frame_ns_.store(clock_());

	if (!started_ || width != width_ || height != height_ || format != format_) {
		started_ = true;
		width_ = width;
		height_ = height;
		format_ = format;
		detector_.start(width, height, format);
	}
	detector_.process(frame);
}

} // namespace camera_sync
