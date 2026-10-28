// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "alignment/alignment.hpp"
#include "support/fake-port.hpp"

// Records what Apply and Undo ask of the session.
struct FakeMeasuring : camera_sync::MeasuringControl {
	std::vector<std::string> calls; // "clear" and "resume <num>/<den>", in order

	void clear_readings() override { calls.push_back("clear"); }
	void resume(camera_sync::FrameRate rate) override
	{
		calls.push_back("resume " + std::to_string(rate.fps_num) + "/" + std::to_string(rate.fps_den));
	}
};

// Two cameras, Cam A and Cam B, with no plugin filter, and the mic Shure MV7 at 0 ns, measured at F = 33.333 ms. The
// request asks for both cameras in that order.
struct AlignmentHarness {
	static constexpr int64_t ns_per_ms = 1000000;

	FakePort port;
	FakeMeasuring measuring;
	uint64_t now_ns = 0;
	std::vector<std::pair<camera_sync::LogLevel, std::string>> lines;
	camera_sync::Alignment alignment;
	camera_sync::AlignmentRequest request;
	std::vector<camera_sync::Counts> asked;   // every question's counts, in order
	std::function<void()> while_asking;       // runs inside the question, before the answer

	AlignmentHarness()
		: alignment(
			  port, measuring, [this] { return now_ns; },
			  [this](camera_sync::LogLevel level, const std::string &line) { lines.emplace_back(level, line); })
	{
		port.camera("cam-a");
		port.camera("cam-b");
		port.mic("mic", 0);
		request.cameras = {{"cam-a", "Cam A"}, {"cam-b", "Cam B"}};
		request.mic = camera_sync::NamedSource{"mic", "Shure MV7"};
		request.session.state = camera_sync::SessionState::Measuring;
		request.session.frame_interval_ms = 33.333;
		request.rate = {30, 1};
	}

	// Every requested camera Stable with these readings, in the request's order.
	void read(const std::vector<int64_t> &readings_ms)
	{
		request.session.cameras.clear();
		for (size_t i = 0; i < readings_ms.size(); i++) {
			camera_sync::CameraRow row;
			row.id = request.cameras[i].id;
			row.state = camera_sync::CameraState::Stable;
			row.reading_ms = readings_ms[i];
			request.session.cameras.push_back(row);
		}
	}

	camera_sync::Confirm answer(bool yes)
	{
		return [this, yes](const camera_sync::Counts &counts) {
			asked.push_back(counts);
			port.calls.push_back("question");
			if (while_asking)
				while_asking();
			return yes;
		};
	}

	camera_sync::Outcome apply(bool yes = true) { return alignment.apply(request, answer(yes)); }
	camera_sync::Outcome undo(bool yes = true) { return alignment.undo(answer(yes)); }

	// The lines logged at this level, in order.
	std::vector<std::string> logged(camera_sync::LogLevel level) const
	{
		std::vector<std::string> found;
		for (const auto &line : lines)
			if (line.first == level)
				found.push_back(line.second);
		return found;
	}

	bool logged(camera_sync::LogLevel level, const std::string &text) const
	{
		for (const auto &line : lines)
			if (line.first == level && line.second == text)
				return true;
		return false;
	}

	// The port's calls up to the first one that changes a value.
	std::vector<std::string> calls_before_first_write() const
	{
		std::vector<std::string> found;
		for (const std::string &call : port.calls) {
			if (call.rfind("write_", 0) == 0 || call.rfind("remove_", 0) == 0)
				break;
			found.push_back(call);
		}
		return found;
	}
};
