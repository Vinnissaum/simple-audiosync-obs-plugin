// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "reading/camera-pipeline.hpp"
#include "reading/session.hpp"

struct obs_data;
struct obs_output;
struct obs_view;
struct obs_weak_source;
struct video_data;

namespace camera_sync {

// One camera's tap (the TDD's Tap B): a private view whose only channel is the camera, rendered by OBS's render loop
// into a video mix of its own at the canvas frame rate, small and in NV12. Each frame reaches a FrameSink as its luma
// plane, stamped with the render loop's time, which Program's frames of the same tick carry too. The camera is shown
// but not activated, and nothing is added to it, to a scene or to Program.
//
// SPEC_DEVIATION: the design connects video_output_connect to the mix's video. The frames reach it through a private
// raw output instead.
// Reason: in OBS 30.0.2 a mix renders and hands out raw frames only while its raw_active count is up, and only
// start_raw_video raises it. That function is internal; outputs and encoders reach it, a bare video_output_connect
// does not, so it would never get a frame. A raw output bound to the mix's video with obs_output_set_media is the
// public way to start it.
class CameraTap : public Tap {
public:
	// Registers the private output type the taps use. Call it once, from obs_module_load.
	static void register_output();

	// The tap on the source with this UUID, or null when no source has it, or the view, the mix or the output cannot
	// be made or started. Call it on the UI thread.
	static std::unique_ptr<CameraTap> create(const SourceId &camera, FrameSink &sink);

	// Stops the output, waiting for a frame in flight, then removes and destroys the view.
	~CameraTap() override;
	CameraTap(const CameraTap &) = delete;
	CameraTap &operator=(const CameraTap &) = delete;

	// The mix's frames this tap never received: each gap of more than 1.5 frame intervals between two frames counts
	// the frames missing from it.
	uint64_t skipped_frames() const override { return skipped_.load(); }

private:
	CameraTap(struct obs_weak_source *camera, FrameSink &sink);

	static const char *output_name(void *type_data);
	static void *output_create(struct obs_data *settings, struct obs_output *output);
	static void output_destroy(void *data);
	static bool output_start(void *data);
	static void output_stop(void *data, uint64_t ts);
	static void output_raw_video(void *data, struct video_data *frame);

	void on_frame(const struct video_data &frame);

	struct obs_weak_source *camera_;
	FrameSink &sink_;
	struct obs_view *view_ = nullptr;
	struct obs_output *output_ = nullptr;
	uint32_t width_ = 0; // the mix's output size
	uint32_t height_ = 0;
	double frame_ns_ = 0; // the canvas frame interval

	// Used on the mix's video thread only.
	uint64_t last_timestamp_ = 0;
	std::atomic<uint64_t> skipped_{0};
};

} // namespace camera_sync
