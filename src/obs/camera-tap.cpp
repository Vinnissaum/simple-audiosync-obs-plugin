// SPDX-License-Identifier: GPL-2.0-or-later
#include "camera-tap.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include <obs.h>

namespace camera_sync {

namespace {

constexpr const char *output_id = "camera_sync_camera_tap";

// The largest frame a tap hands on, the TDD's starting point for Tap B and a spike parameter. The camera is scaled down
// to fit it with its aspect kept, and never scaled up.
constexpr uint32_t max_width = 640;
constexpr uint32_t max_height = 360;

// The tap whose output is being made: obs_output_create calls output_create on the same thread before it returns.
thread_local CameraTap *creating = nullptr;

// A frame side NV12 can hold: even, and at least 2.
uint32_t even_size(double size)
{
	const uint32_t whole = static_cast<uint32_t>(size);
	return std::max<uint32_t>(2, whole - whole % 2);
}

} // namespace

void CameraTap::register_output()
{
	obs_output_info info = {};
	info.id = output_id;
	info.flags = OBS_OUTPUT_VIDEO;
	info.get_name = output_name;
	info.create = output_create;
	info.destroy = output_destroy;
	info.start = output_start;
	info.stop = output_stop;
	info.raw_video = output_raw_video;
	obs_register_output(&info);
}

std::unique_ptr<CameraTap> CameraTap::create(const SourceId &camera, FrameSink &sink)
{
	obs_video_info canvas;
	if (!obs_get_video_info(&canvas) || canvas.fps_num == 0)
		return nullptr;

	obs_source_t *source = obs_get_source_by_uuid(camera.c_str());
	if (!source)
		return nullptr;
	obs_weak_source_t *weak = obs_source_get_weak_source(source);
	if (!weak) {
		obs_source_release(source);
		return nullptr;
	}
	std::unique_ptr<CameraTap> tap(new CameraTap(weak, sink));

	// A view draws its channel at the source's own size, not scaled to the canvas, so the mix's base is the camera's
	// size after its filters. A camera that has not delivered a frame yet has no size, and the canvas's stands in.
	uint32_t base_width = obs_source_get_width(source);
	uint32_t base_height = obs_source_get_height(source);
	if (base_width == 0 || base_height == 0) {
		base_width = canvas.base_width;
		base_height = canvas.base_height;
	}
	const double scale = std::min(
		{1.0, static_cast<double>(max_width) / base_width, static_cast<double>(max_height) / base_height});
	tap->width_ = even_size(base_width * scale);
	tap->height_ = even_size(base_height * scale);
	tap->frame_ns_ = 1e9 * canvas.fps_den / canvas.fps_num;

	// The mix takes the canvas's settings and frame rate, its own sizes, and NV12, whose first plane is the luma.
	obs_video_info mix = canvas;
	mix.base_width = base_width;
	mix.base_height = base_height;
	mix.output_width = tap->width_;
	mix.output_height = tap->height_;
	mix.output_format = VIDEO_FORMAT_NV12;
	mix.gpu_conversion = true;

	// The view shows the camera without activating it, and holds its own reference to it.
	tap->view_ = obs_view_create();
	if (tap->view_)
		obs_view_set_source(tap->view_, 0, source);
	obs_source_release(source);
	if (!tap->view_)
		return nullptr;
	video_t *video = obs_view_add2(tap->view_, &mix);
	if (!video)
		return nullptr;

	creating = tap.get();
	tap->output_ = obs_output_create(output_id, "camera-sync camera tap", nullptr, nullptr);
	creating = nullptr;
	if (!tap->output_)
		return nullptr;

	video_scale_info conversion = {};
	conversion.format = VIDEO_FORMAT_NV12;
	conversion.width = tap->width_;
	conversion.height = tap->height_;
	conversion.range = mix.range;
	conversion.colorspace = mix.colorspace;
	obs_output_set_media(tap->output_, video, obs_get_audio());
	obs_output_set_video_conversion(tap->output_, &conversion);
	if (!obs_output_start(tap->output_))
		return nullptr;
	return tap;
}

CameraTap::CameraTap(obs_weak_source_t *camera, FrameSink &sink) : camera_(camera), sink_(sink) {}

CameraTap::~CameraTap()
{
	if (output_) {
		// libobs ends the capture on a thread of its own. Once the output is inactive, that thread's disconnect has
		// returned, and it waited for a frame in flight, so no frame reaches this tap afterwards.
		obs_output_stop(output_);
		while (obs_output_active(output_))
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		obs_output_release(output_);
	}
	if (view_) {
		// OBS frees the mix on its next frame once the view is removed; destroying the view releases the camera.
		obs_view_remove(view_);
		obs_view_destroy(view_);
	}
	obs_weak_source_release(camera_);
}

const char *CameraTap::output_name(void *)
{
	return "Camera Sync camera tap";
}

void *CameraTap::output_create(obs_data_t *, obs_output_t *)
{
	return creating;
}

void CameraTap::output_destroy(void *)
{
	// The tap owns itself; the output only points at it.
}

bool CameraTap::output_start(void *data)
{
	const CameraTap &tap = *static_cast<const CameraTap *>(data);
	return obs_output_can_begin_data_capture(tap.output_, 0) && obs_output_begin_data_capture(tap.output_, 0);
}

void CameraTap::output_stop(void *data, uint64_t)
{
	obs_output_end_data_capture(static_cast<CameraTap *>(data)->output_);
}

void CameraTap::output_raw_video(void *data, struct video_data *frame)
{
	if (frame)
		static_cast<CameraTap *>(data)->on_frame(*frame);
}

void CameraTap::on_frame(const struct video_data &frame)
{
	// The mix's frames come one canvas interval apart; a longer gap holds the frames this tap missed.
	if (last_timestamp_ != 0 && frame.timestamp > last_timestamp_) {
		const double gap = static_cast<double>(frame.timestamp - last_timestamp_);
		if (gap > 1.5 * frame_ns_)
			skipped_ += static_cast<uint64_t>(std::llround(gap / frame_ns_)) - 1;
	}
	last_timestamp_ = frame.timestamp;

	// The mix renders the camera whether or not the camera has a picture, so a frame goes on only while it has one:
	// a removed camera, or one whose device stopped and cleared its frame, has no size. Without this the session
	// could never see a camera deliver nothing for 2 s (READ-57).
	obs_source_t *camera = obs_weak_source_get_source(camera_);
	const bool picture = camera && !obs_source_removed(camera) && obs_source_get_width(camera) != 0 &&
			     obs_source_get_height(camera) != 0;
	obs_source_release(camera);
	if (!picture || !frame.data[0])
		return;

	sink_.on_frame(VideoFrame{frame.data[0], frame.linesize[0], frame.timestamp}, width_, height_,
		       LumaFormat::Luma8);
}

} // namespace camera_sync
