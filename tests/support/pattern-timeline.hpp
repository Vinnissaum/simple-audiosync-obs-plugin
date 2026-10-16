// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "media.hpp"
#include "reading/camera-pipeline.hpp"
#include "support/fixtures.hpp"

// video-cycles/ holds marker cycles 0 to 3 of Sync Dock's pattern q=4,f=884,c=2 at 60 fps, 320x180, as generate.py
// writes them: each distinct frame once, and sequence.txt naming the 48 frames in order. A cycle is 3q = 12 frames: q
// QR frames, then q frames of each sync image. The generator starts each cycle's sound at frame 2q of the cycle, where
// the second sync image replaces the first, so that frame's time is the cycle's generated marker time.
constexpr uint32_t pattern_fps = 60;
constexpr uint32_t pattern_cycles = 4;
constexpr uint32_t pattern_cycle_frames = 12;
constexpr uint32_t pattern_marker_frame = 8;

// The time frame k of the sequence is shown, with frame 0 at start_ns.
inline uint64_t pattern_frame_ns(uint64_t start_ns, uint64_t k)
{
	return start_ns + camera_sync::mul_div64(k, 1000000000ULL, pattern_fps);
}

struct TimedFrame {
	size_t frame;       // into PatternTimeline::frames
	uint64_t timestamp; // ns
};

struct PatternTimeline {
	std::vector<Pgm> frames;          // the distinct frames
	std::vector<TimedFrame> sequence; // every frame in order, with the time it is shown
};

// The frame sequence at 60 fps, frame 0 shown at start_ns.
inline PatternTimeline read_pattern_timeline(uint64_t start_ns)
{
	const std::vector<uint8_t> bytes = read_binary("video-cycles/sequence.txt");
	std::istringstream in(std::string(bytes.begin(), bytes.end()));

	PatternTimeline timeline;
	std::map<std::string, size_t> read;
	for (std::string name; in >> name;) {
		auto found = read.find(name);
		if (found == read.end()) {
			found = read.emplace(name, timeline.frames.size()).first;
			timeline.frames.push_back(read_pgm("video-cycles/" + name));
		}
		const uint64_t k = timeline.sequence.size();
		timeline.sequence.push_back({found->second, pattern_frame_ns(start_ns, k)});
	}
	return timeline;
}

// Shows the sequence to a camera's pipeline, frame by frame, as a camera's tap does.
inline void play(const PatternTimeline &timeline, camera_sync::FrameSink &sink)
{
	for (const TimedFrame &timed : timeline.sequence) {
		const Pgm &frame = timeline.frames[timed.frame];
		sink.on_frame(camera_sync::VideoFrame{frame.luma.data(), frame.width, timed.timestamp}, frame.width,
			      frame.height, camera_sync::LumaFormat::Luma8);
	}
}
