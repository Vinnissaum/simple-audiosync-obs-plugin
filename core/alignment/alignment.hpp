// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "alignment/settings-port.hpp"
#include "alignment/types.hpp"
#include "alignment/write-steps.hpp"
#include "log.hpp"
#include "reading/session.hpp"

namespace camera_sync {

// What Apply and Undo ask of the measuring session. The controller adapts the Session to it, so the alignment needs
// neither the session's taps nor its threads.
class MeasuringControl {
public:
	virtual ~MeasuringControl() = default;
	// Every camera back to Looking, the pattern and the session's state kept.
	virtual void clear_readings() = 0;
	// Measures again at this frame rate, from Stopped.
	virtual void resume(FrameRate rate) = 0;
};

// The on-air question: true when the user accepts. The dock asks it modally, so OBS keeps running while it is open.
using Confirm = std::function<bool(const Counts &)>;

// What Apply works from: the ticked cameras in checklist order with their names, the mic, the session's readings,
// state and frame interval, and the canvas frame rate to measure at again.
struct AlignmentRequest {
	std::vector<NamedSource> cameras;
	std::optional<NamedSource> mic;
	SessionSnapshot session;
	FrameRate rate;
};

// Runs Apply and Undo, each in one call on the UI thread, and keeps what lives between clicks: the Undo snapshot, the
// "aligned after Apply" flag and the last result. It reaches OBS only through the settings port.
class Alignment {
public:
	Alignment(SettingsPort &port, MeasuringControl &measuring, Clock clock, Logger log);

	// Writes the alignment for the request's readings, all of it or nothing (ALIGN-11 to ALIGN-30).
	Outcome apply(const AlignmentRequest &request, const Confirm &confirm);
	// Puts back the values from before the last Apply, all of them or nothing (ALIGN-31 to ALIGN-40). With no
	// snapshot it touches nothing and returns Waiting.
	Outcome undo(const Confirm &confirm);

	// The scene collection is about to change: the snapshot, the flag and the last result go, and a question still
	// open will be refused when it is answered (ALIGN-32).
	void collection_changing();
	// Reset: the flag and the last result go; the snapshot stays (ALIGN-57, ALIGN-58).
	void reset();

	AlignmentStatus status() const;

private:
	// The values from before the last Apply, in memory only (ALIGN-32), in Apply's camera order.
	struct UndoSnapshot {
		std::vector<std::pair<NamedSource, FilterState>> cameras;
		NamedSource mic;
		int64_t sync_offset_ns = 0;
	};

	// What one read of OBS found: each camera's plugin filter, and the mic's Sync Offset.
	struct Found {
		std::vector<FilterState> filters;
		int64_t sync_offset_ns = 0;

		bool operator==(const Found &other) const
		{
			return filters == other.filters && sync_offset_ns == other.sync_offset_ns;
		}
	};

	// Resolves every source, checks the filter type, and reads the values; none, with the refusal logged, when a
	// source is gone or the filter type is not registered.
	std::optional<Found> read_all(const char *prefix, const std::vector<NamedSource> &cameras,
				      const NamedSource &mic);
	enum class Answer { Go, Cancelled, Refused };

	// Asks the on-air question and, after a yes, reads again. Logs a cancel or a refusal.
	Answer ask(const char *prefix, const std::vector<Step> &steps, const Confirm &confirm,
		   const std::vector<NamedSource> &cameras, const NamedSource &mic, const Found &found);
	// Logs a failed write_steps and drops what a failed restore leaves untrue. Returns whether every value went back.
	bool failed(const char *prefix, const std::vector<Step> &steps, const StepsResult &result);
	Outcome remember(Outcome outcome);
	int64_t elapsed_ms(uint64_t since) const;
	void warn(const std::string &line) const;
	void info(const std::string &line) const;

	SettingsPort &port_;
	MeasuringControl &measuring_;
	Clock clock_;
	Logger log_;

	std::optional<UndoSnapshot> undo_;
	bool aligned_after_apply_ = false;
	std::optional<Outcome> last_;
	uint64_t generation_ = 0; // bumped by a collection switch; a question compares it after the answer
};

} // namespace camera_sync
