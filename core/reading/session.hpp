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
	// While measuring: 5 s have passed since the first video marker the mic has not answered, and video markers still
	// arrive.
	bool no_pattern_sound = false;
	// The frame interval F read at the last Start, in ms; 0 before any Start.
	double frame_interval_ms = 0;
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

	// The ticked cameras and the chosen mic. A newly ticked camera is measured at once while measuring; an unticked one
	// loses its tap and its reading. Another mic clears every reading and the pattern, and while measuring its tap
	// replaces the old mic's.
	void set_selection(std::vector<SourceId> cameras, std::optional<SourceId> mic);

	// Measures every ticked camera against the mic at the canvas frame rate, from Idle or Stopped. With no camera or no
	// mic it changes nothing. A camera whose tap cannot be made is skipped with a warning; a mic whose tap cannot be
	// made measures nothing.
	StartResult start(FrameRate rate);
	// Releases every tap and keeps every reading and the pattern, for a later Start.
	void stop();
	// Clears every reading, sample, marker and the pattern, while measuring or stopped.
	void reset();
	// The scene collection is about to change: every tap, reading and the selection go, and the session is Idle.
	void collection_changing();
	// OBS is exiting: every tap goes before libobs does.
	void shutdown();

	// The chosen mic left OBS: every tap, reading and the pattern go, and the session is Idle.
	void on_mic_removed();
	// A ticked camera left OBS or was renamed: its tap and reading go, and it is no longer ticked.
	void on_camera_removed(const SourceId &camera);
	void on_camera_renamed(const SourceId &camera);
	// A filter was added to or removed from a ticked camera: its reading no longer holds.
	void on_camera_filters_changed(const SourceId &camera);
	// The mic's Sync Offset changed: no reading holds.
	void on_mic_sync_offset_changed();
	// Every camera goes back to Looking. The pattern, the taps and the session's state stay, as after an Apply or an
	// Undo, whose writes leave no reading standing.
	void clear_readings();

	// Called on the UI thread about ten times a second. While measuring, a ticked camera whose tap delivered no frame
	// for 2 s is dropped as on_camera_removed drops it.
	void tick();

	// A camera's pipeline saw a flash of the pattern, or the mic's heard one. They are read only while measuring, a
	// camera's only while it has a tap.
	void on_video_marker(const SourceId &camera, const VideoMarker &marker);
	void on_audio_marker(const AudioMarker &marker);

	SessionSnapshot snapshot() const;

private:
	// A source's name is read when its tap is made, on the UI thread, for the lines its markers log on the tap's.
	struct TappedCamera {
		std::string name;
		std::unique_ptr<CameraPipeline> pipeline;
		std::unique_ptr<Tap> tap; // declared after the pipeline it delivers to, so it is destroyed first
		uint64_t skipped_logged = 0;
		std::optional<uint64_t> skipped_logged_ns;
	};

	struct TappedMic {
		std::string name;
		std::unique_ptr<MicPipeline> pipeline;
		std::unique_ptr<Tap> tap; // declared after the pipeline it delivers to, so it is destroyed first
	};

	// Taps taken out under the lock, destroyed once it is released.
	struct Released {
		std::vector<TappedCamera> cameras;
		std::vector<TappedMic> mics;
	};

	// Each of these runs under the lock.
	bool tap_mic();
	void tap_camera(const SourceId &camera);
	void drop_camera(const SourceId &camera, Released &released);
	void untick(const SourceId &camera, Released &released);
	void release_taps(Released &released);
	void restart_no_sound_wait();
	bool no_pattern_sound(uint64_t now) const;

	TapFactory &factory_;
	Clock clock_;
	Logger log_;
	NameOf name_of_;

	mutable std::mutex mutex_;
	SessionState state_ = SessionState::Idle;
	std::vector<SourceId> cameras_;
	std::optional<SourceId> mic_;
	bool mic_tap_failed_ = false;
	double frame_interval_ms_ = 0;
	std::optional<PatternParams> pattern_;
	std::optional<uint64_t> unanswered_since_; // the first video marker since the last audio marker, or Start
	uint64_t last_video_ns_ = 0;
	ReadingEstimator estimator_;
	TappedMic mic_tap_;
	std::map<SourceId, TappedCamera> camera_taps_;
};

} // namespace camera_sync
