// SPDX-License-Identifier: GPL-2.0-or-later
#include "reading/session.hpp"

#include <algorithm>
#include <utility>

#include "log-lines.hpp"

namespace camera_sync {

namespace {

// How long the mic may stay silent while cameras see the pattern, and how long the cameras may stop seeing it,
// before the dock is told.
constexpr uint64_t no_sound_ns = 5000000000;

// How long a ticked camera's tap may deliver no frame while measuring before the camera is taken as deactivated.
constexpr uint64_t no_frames_ns = 2000000000;

} // namespace

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
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	if (cameras == cameras_ && mic == mic_)
		return;
	mic_tap_failed_ = false;

	// An unticked camera loses its tap and its reading; the others keep theirs (READ-33).
	for (const SourceId &camera : cameras_)
		if (std::find(cameras.begin(), cameras.end(), camera) == cameras.end())
			drop_camera(camera, released);
	// A newly ticked camera is measured at once while measuring, and only joins the selection otherwise (READ-32).
	for (const SourceId &camera : cameras) {
		if (std::find(cameras_.begin(), cameras_.end(), camera) != cameras_.end())
			continue;
		estimator_.add_camera(camera);
		if (state_ == SessionState::Measuring)
			tap_camera(camera);
	}
	cameras_ = std::move(cameras);

	if (mic == mic_)
		return;
	// A reading is relative to one mic, so neither the readings nor the pattern survive a change of mic (READ-34).
	mic_ = std::move(mic);
	estimator_.clear();
	pattern_.reset();
	restart_no_sound_wait();
	if (state_ != SessionState::Measuring)
		return;
	released.mics.push_back(std::move(mic_tap_));
	// With no mic to listen to, measuring ends as it does when the mic is removed.
	if (!mic_ || !tap_mic()) {
		release_taps(released);
		state_ = SessionState::Idle;
	}
}

StartResult Session::start(FrameRate rate)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ == SessionState::Measuring)
		return StartResult::Started;
	if (cameras_.empty() || !mic_)
		return StartResult::NothingChosen;

	// The mic first: nothing is measured without it, and no camera's tap has been made yet to undo.
	if (!tap_mic())
		return StartResult::MicTapFailed;
	mic_tap_failed_ = false;

	for (const SourceId &camera : cameras_)
		tap_camera(camera);

	estimator_.set_frame_interval_ms(frame_interval_ms(rate.fps_num, rate.fps_den));
	estimator_.reset_counters();
	restart_no_sound_wait();
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
	restart_no_sound_wait();
	state_ = SessionState::Stopped;
}

void Session::reset()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ == SessionState::Idle)
		return;
	estimator_.clear();
	pattern_.reset();
	restart_no_sound_wait();
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

void Session::on_mic_removed()
{
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	release_taps(released);
	estimator_.clear();
	pattern_.reset();
	mic_.reset();
	mic_tap_failed_ = false;
	state_ = SessionState::Idle;
}

void Session::on_camera_removed(const SourceId &camera)
{
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	untick(camera, released);
}

void Session::on_camera_renamed(const SourceId &camera)
{
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	untick(camera, released);
}

void Session::on_camera_filters_changed(const SourceId &camera)
{
	std::lock_guard<std::mutex> lock(mutex_);
	estimator_.clear_camera(camera);
}

void Session::on_mic_sync_offset_changed()
{
	// Every audio marker went with the readings: each was stamped with the old offset. The pattern stays.
	std::lock_guard<std::mutex> lock(mutex_);
	estimator_.clear();
}

void Session::tick()
{
	Released released; // destroyed after the lock below is released
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ != SessionState::Measuring)
		return;

	// A tap's thread may stamp a frame after the clock is read here, so the comparison adds rather than subtracts.
	const uint64_t now = clock_();
	std::vector<SourceId> silent;
	for (const auto &entry : camera_taps_)
		if (entry.second.pipeline->last_frame_ns() + no_frames_ns <= now)
			silent.push_back(entry.first);
	for (const SourceId &camera : silent)
		untick(camera, released);
}

void Session::on_video_marker(const SourceId &camera, const VideoMarker &marker)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (state_ != SessionState::Measuring)
		return;

	// The first marker's pattern is the one the session follows until Reset: the mic listens for its sound and the
	// estimator pairs by its wrap (READ-17). Another pattern's markers are ignored (READ-18).
	const PatternParams params = PatternParams::from(marker.qr_data);
	if (!pattern_) {
		pattern_ = params;
		estimator_.set_pattern(params);
		if (mic_tap_.pipeline)
			mic_tap_.pipeline->set_pattern(params.f, params.c, params.q_ms);
	} else if (!(params == *pattern_)) {
		return;
	}

	// The wait for the mic starts at the first video marker no audio marker answered, and starts over once the video
	// markers have stopped for 5 s (READ-42, READ-43).
	const uint64_t now = clock_();
	if (!unanswered_since_ || now - last_video_ns_ >= no_sound_ns)
		unanswered_since_ = now;
	last_video_ns_ = now;

	estimator_.on_video_marker(camera, marker.qr_data.index, marker.timestamp);
}

void Session::on_audio_marker(const AudioMarker &marker)
{
	std::lock_guard<std::mutex> lock(mutex_);
	// With no pattern there is nothing to listen for: after Reset the mic may still decode the pattern it cleared
	// until a camera reads one again (READ-19).
	if (state_ != SessionState::Measuring || !pattern_)
		return;
	unanswered_since_.reset();
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
	snapshot.no_pattern_sound = no_pattern_sound(clock_());
	return snapshot;
}

// Makes the mic's pipeline and tap, where the session has none. A tap that cannot be made is logged and remembered.
bool Session::tap_mic()
{
	auto pipeline = std::make_unique<MicPipeline>(log_, factory_.mic_sample_rate(),
						      [this](const AudioMarker &marker) { on_audio_marker(marker); });
	// A pattern kept through Stop is listened for at once (READ-26).
	if (pattern_)
		pipeline->set_pattern(pattern_->f, pattern_->c, pattern_->q_ms);
	std::unique_ptr<Tap> tap = factory_.create_mic_tap(*mic_, *pipeline);
	if (!tap) {
		mic_tap_failed_ = true;
		write_log(log_, LogLevel::Warning, "tap.failed mic=" + quote(name_of_(*mic_)));
		return false;
	}
	mic_tap_ = TappedMic{std::move(pipeline), std::move(tap)};
	return true;
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
	camera_taps_.emplace(camera, TappedCamera{std::move(pipeline), std::move(tap)});
}

// Drops one camera's reading, and hands its tap, if it has one, to released.
void Session::drop_camera(const SourceId &camera, Released &released)
{
	estimator_.remove_camera(camera);
	auto found = camera_taps_.find(camera);
	if (found == camera_taps_.end())
		return;
	released.cameras.push_back(std::move(found->second));
	camera_taps_.erase(found);
}

void Session::restart_no_sound_wait()
{
	unanswered_since_.reset();
	last_video_ns_ = 0;
}

bool Session::no_pattern_sound(uint64_t now) const
{
	return state_ == SessionState::Measuring && unanswered_since_ && now - *unanswered_since_ >= no_sound_ns &&
	       now - last_video_ns_ < no_sound_ns;
}

// A camera leaves the selection, with its tap and reading.
void Session::untick(const SourceId &camera, Released &released)
{
	auto found = std::find(cameras_.begin(), cameras_.end(), camera);
	if (found == cameras_.end())
		return;
	cameras_.erase(found);
	drop_camera(camera, released);
}

void Session::release_taps(Released &released)
{
	for (auto &entry : camera_taps_)
		released.cameras.push_back(std::move(entry.second));
	camera_taps_.clear();
	released.mics.push_back(std::move(mic_tap_));
}

} // namespace camera_sync
