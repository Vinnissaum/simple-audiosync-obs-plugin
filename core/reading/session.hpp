// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "detector/audio-detector.hpp"
#include "detector/video-detector.hpp"
#include "log.hpp"
#include "reading/camera-pipeline.hpp"
#include "reading/estimator.hpp"
#include "reading/mic-pipeline.hpp"
#include "reading/pattern.hpp"
#include "reading/types.hpp"

namespace camera_sync {

// What delivers one camera's frames or the mic's samples while it lives. Destroying a tap waits for a delivery in
// flight, so none runs after the destructor returns.
class Tap {
public:
	virtual ~Tap() = default;
	// The frames the tap skipped since it was made.
	virtual uint64_t skipped_frames() const = 0;
};

// Makes the taps the session measures through. A null tap means it could not be made.
class TapFactory {
public:
	virtual ~TapFactory() = default;
	virtual std::unique_ptr<Tap> create_camera_tap(const SourceId &camera, FrameSink &sink) = 0;
	virtual std::unique_ptr<Tap> create_mic_tap(const SourceId &mic, AudioSink &sink) = 0;
	virtual uint32_t mic_sample_rate() = 0;
};

// A source's name as OBS shows it, for the log.
using NameOf = std::function<std::string(const SourceId &)>;

// OBS's canvas frame rate: fps_num / fps_den frames a second.
struct FrameRate {
	uint32_t fps_num = 0;
	uint32_t fps_den = 1;
};

enum class SessionState { Idle, Measuring, Stopped };

enum class StartResult { Started, NothingChosen, MicTapFailed };

struct SessionSnapshot {
	SessionState state = SessionState::Idle;
	std::vector<CameraRow> cameras; // the ticked cameras, sorted by id
	std::optional<SourceId> mic;
	bool mic_tap_failed = false; // the last Start could not listen to the mic
};

// The measuring session: the selection, the pattern, the taps and the readings. Its control methods run on the UI
// thread and are the only ones that change what is measured. The marker entry points run on the taps' threads. One
// lock guards it all, and a tap is destroyed only once the lock is released: a delivery in flight may be waiting for
// the lock, and the tap's destructor waits for that delivery.
class Session {
public:
	// SPEC_DEVIATION: the design's constructor takes a SessionConfig with the frame rate; this one takes name_of.
	// Reason: the frame rate is read at each Start, so start(FrameRate) takes it, and the log lines need the
	// sources' names.
	Session(TapFactory &factory, Clock clock, Logger log, NameOf name_of);
	~Session();

	// The ticked cameras and the chosen mic.
	void set_selection(std::vector<SourceId> cameras, std::optional<SourceId> mic);

	// Measures every ticked camera against the mic at the canvas frame rate, from Idle or Stopped. With no camera or no
	// mic it changes nothing. A camera whose tap cannot be made is skipped with a warning; a mic whose tap cannot be
	// made measures nothing.
	StartResult start(FrameRate rate);
	void stop();
	void reset();
	// The scene collection is about to change: every tap, reading and the selection go, and the session is Idle.
	void collection_changing();
	// OBS is exiting: every tap goes before libobs does.
	void shutdown();

	// A camera's pipeline saw a flash of the pattern, or the mic's heard one. They are read only while measuring.
	void on_video_marker(const SourceId &camera, const VideoMarker &marker);
	void on_audio_marker(const AudioMarker &marker);

	SessionSnapshot snapshot() const;

private:
	struct TappedCamera {
		std::unique_ptr<CameraPipeline> pipeline;
		std::unique_ptr<Tap> tap; // declared after the pipeline it delivers to, so it is destroyed first
	};

	// Taps taken out under the lock, destroyed once it is released: each tap before the pipeline it delivers to.
	struct Released {
		std::vector<TappedCamera> cameras;
		std::unique_ptr<MicPipeline> mic_pipeline;
		std::unique_ptr<Tap> mic_tap;
	};

	void tap_camera(const SourceId &camera);
	void release_taps(Released &released);

	TapFactory &factory_;
	Clock clock_;
	Logger log_;
	NameOf name_of_;

	mutable std::mutex mutex_;
	SessionState state_ = SessionState::Idle;
	std::vector<SourceId> cameras_;
	std::optional<SourceId> mic_;
	bool mic_tap_failed_ = false;
	std::optional<PatternParams> pattern_;
	ReadingEstimator estimator_;
	std::unique_ptr<MicPipeline> mic_pipeline_;
	std::unique_ptr<Tap> mic_tap_;
	std::map<SourceId, TappedCamera> tapped_;
};

} // namespace camera_sync
