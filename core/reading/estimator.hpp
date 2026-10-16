// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>

#include "reading/pattern.hpp"
#include "reading/types.hpp"

namespace camera_sync {

// One camera's row of a snapshot.
struct CameraRow {
	SourceId id;
	CameraState state = CameraState::Looking;
	int64_t live_ms = 0;    // the latest sample by video time, rounded; valid when state is not Looking
	int64_t reading_ms = 0; // valid when state is Stable
	uint32_t samples = 0;   // samples in the window, at most five
};

// Every added camera's row as it was at one instant, sorted by id.
struct EstimatorSnapshot {
	std::vector<CameraRow> cameras;
};

// A camera that became Stable, or whose stable reading changed, with the spread of the window that decided it: its
// largest sample minus its smallest, rounded.
struct StableChange {
	SourceId camera;
	int64_t reading_ms;
	int64_t spread_ms;
};

// What one video marker did: whether it formed a sample, and whether that made its camera Stable or changed its
// reading.
struct SampleResult {
	bool formed_sample = false;
	bool stable_changed = false;
	int64_t reading_ms = 0; // valid when stable_changed
	int64_t spread_ms = 0;  // valid when stable_changed
};

// Pairs each camera's video markers with the mic's audio markers into samples, and keeps each camera's five most recent
// samples by video time. A sample is the video marker's time minus the audio marker's, in ms: positive when the
// camera's video is late. A camera whose five samples lie within one frame of each other is Stable, and its reading is
// their median. It stays Stable, with its reading, until it is cleared: only a later window within one frame changes
// the reading.
class ReadingEstimator {
public:
	// The pattern whose markers are paired. A pair's markers carry the same index and lie less than half a wrap
	// apart.
	void set_pattern(const PatternParams &pattern);

	// The frame interval F in ms. Five samples whose largest minus smallest is at most F make a camera Stable.
	void set_frame_interval_ms(double ms);

	// A camera's markers are paired once it is added. Adding a camera twice changes nothing.
	void add_camera(const SourceId &camera);
	void remove_camera(const SourceId &camera);
	// Drops one camera's samples, reading and unpaired video markers. The camera stays added.
	void clear_camera(const SourceId &camera);
	// Drops every camera's samples, reading and unpaired video markers, and every audio marker. The cameras stay
	// added.
	void clear();

	// A flash of the pattern, seen by a camera or heard by the mic, with its marker index and time in ns. A video
	// marker of a camera that is not added is ignored. An audio marker can complete a pair for several cameras, so
	// it returns each camera it made Stable or whose reading it changed.
	SampleResult on_video_marker(const SourceId &camera, uint32_t index, uint64_t time_ns);
	std::vector<StableChange> on_audio_marker(uint32_t index, uint64_t time_ns);

	EstimatorSnapshot snapshot() const;

private:
	struct Marker {
		uint32_t index;
		uint64_t time_ns;
	};

	// SPEC_DEVIATION: the design says a paired marker leaves its list. A video marker does; an audio marker stays
	// in its list after it pairs, and pairs once with each camera.
	// Reason: every camera pairs with the same audio markers (READ-05), so one camera's pair cannot take the marker
	// from the others. Pairing once per camera keeps a repeated video marker from forming a second sample.
	struct AudioEntry {
		Marker marker;
		std::vector<SourceId> paired;
	};

	struct Sample {
		uint64_t video_ns;
		double ms;
	};

	struct Camera {
		std::deque<Marker> waiting; // video markers no audio marker has paired with yet
		std::vector<Sample> window; // the five most recent samples, oldest video time first
		bool stable = false;
		int64_t reading_ms = 0;
	};

	bool pairs(const Marker &video, const Marker &audio) const;
	// Adds a sample to the camera's window and decides its state. Returns the window's spread when the camera
	// became Stable or its reading changed.
	std::optional<int64_t> add_sample(Camera &camera, uint64_t video_ns, uint64_t audio_ns);

	PatternParams pattern_;
	double frame_interval_ms_ = 0;
	std::map<SourceId, Camera> cameras_;
	std::deque<AudioEntry> audio_;
};

} // namespace camera_sync
