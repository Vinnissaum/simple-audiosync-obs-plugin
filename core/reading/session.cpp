// SPDX-License-Identifier: GPL-2.0-or-later
#include "reading/session.hpp"

#include <algorithm>
#include <utility>

#include "log-lines.hpp"

namespace camera_sync {

Session::Session(TapFactory &factory, Clock clock, Logger log, NameOf name_of)
	: factory_(factory),
	  clock_(std::move(clock)),
	  log_(std::move(log)),
	  name_of_(std::move(name_of))
{
}

Session::~Session()
{
	shutdown();
}

void Session::set_selection(std::vector<SourceId> cameras, std::optional<SourceId> mic)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (cameras == cameras_ && mic == mic_)
		return;
	for (const SourceId &camera : cameras_)
		if (std::find(cameras.begin(), cameras.end(), camera) == cameras.end())
			estimator_.remove_camera(camera);
	for (const SourceId &camera : cameras)
		estimator_.add_camera(camera);
	cameras_ = std::move(cameras);
	mic_ = std::move(mic);
	mic_tap_failed_ = false;
}

StartResult Session::start(FrameRate rate)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ == SessionState::Measuring)
		return StartResult::Started;
	if (cameras_.empty() || !mic_)
		return StartResult::NothingChosen;

	// The mic first: nothing is measured without it, and no camera's tap has been made yet to undo.
	auto mic_pipeline = std::make_unique<MicPipeline>(
		log_, factory_.mic_sample_rate(), [this](const AudioMarker &marker) { on_audio_marker(marker); });
	std::unique_ptr<Tap> mic_tap = factory_.create_mic_tap(*mic_, *mic_pipeline);
	if (!mic_tap) {
		mic_tap_failed_ = true;
		write_log(log_, LogLevel::Warning, "tap.failed mic=" + quote(name_of_(*mic_)));
		return StartResult::MicTapFailed;
	}
	mic_tap_failed_ = false;
	mic_pipeline_ = std::move(mic_pipeline);
	mic_tap_ = std::move(mic_tap);

	for (const SourceId &camera : cameras_)
		tap_camera(camera);

	estimator_.set_frame_interval_ms(frame_interval_ms(rate.fps_num, rate.fps_den));
	estimator_.reset_counters();
	state_ = SessionState::Measuring;
	return StartResult::Started;
}

void Session::stop()
{
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ != SessionState::Measuring)
		return;
	release_taps(released);
	state_ = SessionState::Stopped;
}

void Session::reset()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ == SessionState::Idle)
		return;
}

void Session::collection_changing()
{
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	release_taps(released);
	for (const SourceId &camera : cameras_)
		estimator_.remove_camera(camera);
	estimator_.clear();
	cameras_.clear();
	mic_.reset();
	mic_tap_failed_ = false;
	pattern_.reset();
	state_ = SessionState::Idle;
}

void Session::shutdown()
{
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	release_taps(released);
	if (state_ == SessionState::Measuring)
		state_ = SessionState::Stopped;
}

void Session::on_video_marker(const SourceId &camera, const VideoMarker &marker)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ != SessionState::Measuring)
		return;

	// The first marker's pattern is the one the session follows: the mic listens for its sound and the estimator
	// pairs by its wrap.
	if (!pattern_) {
		pattern_ = PatternParams::from(marker.qr_data);
		estimator_.set_pattern(*pattern_);
		if (mic_pipeline_)
			mic_pipeline_->set_pattern(pattern_->f, pattern_->c, pattern_->q_ms);
	}
	estimator_.on_video_marker(camera, marker.qr_data.index, marker.timestamp);
}

void Session::on_audio_marker(const AudioMarker &marker)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ != SessionState::Measuring)
		return;
	estimator_.on_audio_marker(static_cast<uint32_t>(marker.index), marker.timestamp);
}

SessionSnapshot Session::snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	SessionSnapshot snapshot;
	snapshot.state = state_;
	snapshot.cameras = estimator_.snapshot().cameras;
	snapshot.mic = mic_;
	snapshot.mic_tap_failed = mic_tap_failed_;
	return snapshot;
}

void Session::tap_camera(const SourceId &camera)
{
	auto pipeline = std::make_unique<CameraPipeline>(log_, clock_, [this, camera](const VideoMarker &marker) {
		on_video_marker(camera, marker);
	});
	std::unique_ptr<Tap> tap = factory_.create_camera_tap(camera, *pipeline);
	if (!tap) {
		write_log(log_, LogLevel::Warning, "tap.failed camera=" + quote(name_of_(camera)));
		return;
	}
	tapped_.emplace(camera, TappedCamera{std::move(pipeline), std::move(tap)});
}

void Session::release_taps(Released &released)
{
	for (auto &entry : tapped_)
		released.cameras.push_back(std::move(entry.second));
	tapped_.clear();
	released.mic_tap = std::move(mic_tap_);
	released.mic_pipeline = std::move(mic_pipeline_);
}

} // namespace camera_sync
