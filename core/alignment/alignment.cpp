// SPDX-License-Identifier: GPL-2.0-or-later
#include "alignment/alignment.hpp"

#include <algorithm>

#include "alignment/calculator.hpp"
#include "log-lines.hpp"
#include "reading/pattern.hpp"

namespace camera_sync {

namespace {

constexpr const char *apply_prefix = "apply";
constexpr const char *undo_prefix = "undo";
constexpr int64_t ns_per_ms = 1000000;

int64_t to_ms(int64_t ns)
{
	return round_half_away(static_cast<double>(ns) / ns_per_ms);
}

Counts count(const std::vector<Step> &steps)
{
	Counts counts;
	for (const Step &step : steps) {
		if (step.mic)
			counts.mic = true;
		else
			counts.cameras++;
	}
	return counts;
}

Outcome outcome_of(OutcomeKind kind, std::string source = {})
{
	Outcome outcome;
	outcome.kind = kind;
	outcome.source = std::move(source);
	return outcome;
}

} // namespace

AlignmentRequest request_for(const std::vector<SourceInfo> &sources, const Choice &choice,
			     const SessionSnapshot &session, FrameRate rate)
{
	AlignmentRequest request;
	for (const SourceInfo &camera : camera_list(sources, choice.mic)) {
		if (std::find(choice.cameras.begin(), choice.cameras.end(), camera.id) != choice.cameras.end())
			request.cameras.push_back(NamedSource{camera.id, camera.name});
	}
	if (choice.mic) {
		NamedSource mic{*choice.mic, {}};
		for (const SourceInfo &source : sources) {
			if (source.id == *choice.mic)
				mic.name = source.name;
		}
		request.mic = mic;
	}
	request.session = session;
	request.rate = rate;
	return request;
}

Alignment::Alignment(SettingsPort &port, MeasuringControl &measuring, Clock clock, Logger log)
	: port_(port),
	  measuring_(measuring),
	  clock_(std::move(clock)),
	  log_(std::move(log))
{
}

Outcome Alignment::apply(const AlignmentRequest &requested, const Confirm &confirm)
{
	// The question's event loop may change the caller's request; Apply works from the request as it was pressed.
	const AlignmentRequest request = requested;
	uint64_t started = clock_();

	// Nothing is resolved or read before these two refusals (ALIGN-11, ALIGN-12).
	std::vector<SourceId> ids;
	for (const NamedSource &camera : request.cameras)
		ids.push_back(camera.id);
	const std::vector<Reading> readings = readings_of(ids, request.session.cameras);
	const Precheck check = precheck(readings, request.session.frame_interval_ms);
	if (!request.mic || request.cameras.empty() || check.kind == PrecheckKind::Waiting) {
		std::string name;
		if (request.mic && !request.cameras.empty())
			name = request.cameras[check.camera].name;
		warn(alignment_refused_waiting(apply_prefix, name));
		return outcome_of(OutcomeKind::Waiting, name);
	}
	if (check.kind == PrecheckKind::AlreadyAligned) {
		info(alignment_event(apply_prefix, "already_aligned"));
		return remember(outcome_of(OutcomeKind::AlreadyAligned));
	}
	const NamedSource &mic = *request.mic;

	// The values found are both the plan's input and the restore point (ALIGN-15).
	const std::optional<Found> found = read_all(apply_prefix, request.cameras, mic);
	if (!found)
		return remember(outcome_of(OutcomeKind::ApplyFailed));

	std::vector<int64_t> readings_ms;
	for (const Reading &reading : readings)
		readings_ms.push_back(reading.ms);
	const PlanResult result = compute_plan(readings_ms, found->filters, found->sync_offset_ns);
	if (!result.plan) {
		const size_t index = *result.over_limit;
		const std::string &name = index < request.cameras.size() ? request.cameras[index].name : mic.name;
		warn(alignment_refused_over_limit(apply_prefix, name, result.over_limit_ms));
		return remember(outcome_of(OutcomeKind::OverLimit, name));
	}
	const Plan &plan = *result.plan;

	// Only what changes is written, the cameras in order and then the mic (ALIGN-20 to ALIGN-23).
	std::vector<Step> steps;
	for (size_t i = 0; i < request.cameras.size(); i++) {
		if (!plan.cameras[i].changed)
			continue;
		Step step;
		step.source = request.cameras[i].id;
		step.name = request.cameras[i].name;
		step.camera_target = FilterState{true, plan.cameras[i].new_ms, true};
		step.camera_found = found->filters[i];
		steps.push_back(step);
	}
	if (plan.mic_changed) {
		Step step;
		step.mic = true;
		step.source = mic.id;
		step.name = mic.name;
		step.mic_target_ns = plan.new_offset_ms * ns_per_ms;
		step.mic_found_ns = found->sync_offset_ns;
		steps.push_back(step);
	}

	if (!steps.empty() && port_.on_air()) {
		const Answer answer = ask(apply_prefix, steps, confirm, request.cameras, mic, *found);
		// A cancel is no result: the message before it stays (ALIGN-18, ALIGN-57).
		if (answer == Answer::Cancelled)
			return outcome_of(OutcomeKind::Cancelled);
		if (answer == Answer::Refused)
			return remember(outcome_of(OutcomeKind::ApplyFailed));
		started = clock_(); // the duration runs from the answer (ALIGN-71)
	}

	std::vector<PlanEntry> entries;
	for (size_t i = 0; i < request.cameras.size(); i++) {
		const std::string to = std::to_string(plan.cameras[i].new_ms);
		entries.push_back(PlanEntry{request.cameras[i].name, filter_value(found->filters[i]), to});
	}
	const std::string mic_from = std::to_string(plan.old_offset_ms);
	const std::string mic_to = std::to_string(plan.new_offset_ms);
	info(alignment_plan(apply_prefix, entries, PlanEntry{mic.name, mic_from, mic_to}));

	const StepsResult written = write_steps(port_, steps);
	if (!written.ok) {
		const bool restored = failed(apply_prefix, steps, written);
		return remember(outcome_of(restored ? OutcomeKind::ApplyFailed : OutcomeKind::ApplyRestoreFailed));
	}
	info(alignment_ok(apply_prefix, steps.size(), request.cameras.size() + 1 - steps.size(), elapsed_ms(started)));

	UndoSnapshot snapshot;
	for (size_t i = 0; i < request.cameras.size(); i++)
		snapshot.cameras.emplace_back(request.cameras[i], found->filters[i]);
	snapshot.mic = mic;
	snapshot.sync_offset_ns = found->sync_offset_ns;
	undo_ = std::move(snapshot);
	aligned_after_apply_ = true;

	// Clear first: a Start in Stopped keeps every reading (ALIGN-28).
	measuring_.clear_readings();
	if (request.session.state == SessionState::Stopped)
		measuring_.resume(request.rate);

	Outcome outcome = outcome_of(OutcomeKind::Applied);
	for (size_t i = 0; i < request.cameras.size(); i++)
		outcome.values.push_back(NamedValue{request.cameras[i].name, plan.cameras[i].new_ms});
	outcome.values.push_back(NamedValue{mic.name, plan.new_offset_ms});
	return remember(outcome);
}

Outcome Alignment::undo(const Confirm &confirm)
{
	if (!undo_)
		return outcome_of(OutcomeKind::Waiting);
	// A collection switch during the question drops undo_; Undo works from the snapshot as it was pressed.
	const UndoSnapshot snapshot = *undo_;
	uint64_t started = clock_();

	std::vector<NamedSource> cameras;
	for (const auto &camera : snapshot.cameras)
		cameras.push_back(camera.first);
	const std::optional<Found> found = read_all(undo_prefix, cameras, snapshot.mic);
	if (!found)
		return remember(outcome_of(OutcomeKind::UndoFailed));

	// Only what differs from the snapshot is written, hand edits included (ALIGN-36, ALIGN-37).
	std::vector<Step> steps;
	for (size_t i = 0; i < cameras.size(); i++) {
		if (found->filters[i] == snapshot.cameras[i].second)
			continue;
		Step step;
		step.source = cameras[i].id;
		step.name = cameras[i].name;
		step.camera_target = snapshot.cameras[i].second;
		step.camera_found = found->filters[i];
		steps.push_back(step);
	}
	if (found->sync_offset_ns != snapshot.sync_offset_ns) {
		Step step;
		step.mic = true;
		step.source = snapshot.mic.id;
		step.name = snapshot.mic.name;
		step.mic_target_ns = snapshot.sync_offset_ns;
		step.mic_found_ns = found->sync_offset_ns;
		steps.push_back(step);
	}

	if (!steps.empty() && port_.on_air()) {
		const Answer answer = ask(undo_prefix, steps, confirm, cameras, snapshot.mic, *found);
		if (answer == Answer::Cancelled)
			return outcome_of(OutcomeKind::Cancelled);
		if (answer == Answer::Refused)
			return remember(outcome_of(OutcomeKind::UndoFailed));
		started = clock_();
	}

	std::vector<PlanEntry> entries;
	for (size_t i = 0; i < cameras.size(); i++) {
		const std::string to = filter_value(snapshot.cameras[i].second);
		entries.push_back(PlanEntry{cameras[i].name, filter_value(found->filters[i]), to});
	}
	const std::string mic_from = std::to_string(to_ms(found->sync_offset_ns));
	const std::string mic_to = std::to_string(to_ms(snapshot.sync_offset_ns));
	info(alignment_plan(undo_prefix, entries, PlanEntry{snapshot.mic.name, mic_from, mic_to}));

	const StepsResult written = write_steps(port_, steps);
	if (!written.ok) {
		const bool restored = failed(undo_prefix, steps, written);
		return remember(outcome_of(restored ? OutcomeKind::UndoFailed : OutcomeKind::UndoRestoreFailed));
	}
	info(alignment_ok(undo_prefix, steps.size(), cameras.size() + 1 - steps.size(), elapsed_ms(started)));

	undo_.reset();
	aligned_after_apply_ = false;
	measuring_.clear_readings(); // the session keeps its state (ALIGN-40)

	Outcome outcome = outcome_of(OutcomeKind::Undone);
	for (const auto &camera : snapshot.cameras) {
		const int64_t delay = camera.second.present ? camera.second.delay_ms : 0;
		outcome.values.push_back(NamedValue{camera.first.name, delay});
	}
	outcome.values.push_back(NamedValue{snapshot.mic.name, to_ms(snapshot.sync_offset_ns)});
	return remember(outcome);
}

void Alignment::collection_changing()
{
	undo_.reset();
	aligned_after_apply_ = false;
	last_.reset();
	generation_++;
}

void Alignment::reset()
{
	aligned_after_apply_ = false;
	last_.reset();
}

AlignmentStatus Alignment::status() const
{
	AlignmentStatus status;
	status.undo_available = undo_.has_value();
	status.aligned_after_apply = aligned_after_apply_;
	status.last = last_;
	return status;
}

std::optional<Alignment::Found> Alignment::read_all(const char *prefix, const std::vector<NamedSource> &cameras,
						     const NamedSource &mic)
{
	for (const NamedSource &camera : cameras) {
		if (!port_.resolves(camera.id)) {
			warn(alignment_refused_source_gone(prefix, camera.id));
			return std::nullopt;
		}
	}
	if (!port_.resolves(mic.id)) {
		warn(alignment_refused_source_gone(prefix, mic.id));
		return std::nullopt;
	}
	// A filter of a type that is not registered delays nothing, so writing one would be a false success (ALIGN-14).
	if (!port_.delay_filter_available()) {
		warn(alignment_refused_no_delay_filter(prefix));
		return std::nullopt;
	}

	Found found;
	for (const NamedSource &camera : cameras) {
		const std::optional<FilterState> filter = port_.read_filter(camera.id);
		if (!filter) {
			warn(alignment_refused_source_gone(prefix, camera.id));
			return std::nullopt;
		}
		found.filters.push_back(*filter);
	}
	const std::optional<int64_t> offset = port_.read_sync_offset_ns(mic.id);
	if (!offset) {
		warn(alignment_refused_source_gone(prefix, mic.id));
		return std::nullopt;
	}
	found.sync_offset_ns = *offset;
	return found;
}

Alignment::Answer Alignment::ask(const char *prefix, const std::vector<Step> &steps, const Confirm &confirm,
				 const std::vector<NamedSource> &cameras, const NamedSource &mic, const Found &found)
{
	const uint64_t generation = generation_;
	if (!confirm || !confirm(count(steps))) {
		info(alignment_event(prefix, "cancelled"));
		return Answer::Cancelled;
	}
	// OBS kept running while the question was open. A plan computed from values that have changed since, or from a
	// collection that is gone, is not written (ALIGN-19).
	if (generation != generation_) {
		warn(alignment_refused_changed(prefix));
		return Answer::Refused;
	}
	const std::optional<Found> again = read_all(prefix, cameras, mic);
	if (!again)
		return Answer::Refused;
	if (!(*again == found)) {
		warn(alignment_refused_changed(prefix));
		return Answer::Refused;
	}
	return Answer::Go;
}

bool Alignment::failed(const char *prefix, const std::vector<Step> &steps, const StepsResult &result)
{
	const bool restored = result.restore_failures.empty();
	const Step &step = steps[*result.failed];
	warn(alignment_failed(prefix, step.mic, step.name, restored));
	for (const RestoreFailure &failure : result.restore_failures) {
		const Step &put_back = steps[failure.step];
		if (put_back.mic)
			warn(restore_failed_mic(put_back.name, put_back.mic_found_ns, failure.mic_actual_ns));
		else
			warn(restore_failed_camera(put_back.name, put_back.camera_found, failure.camera_actual));
	}
	// Something was written, so no reading holds (ALIGN-29).
	measuring_.clear_readings();
	// After a failed restore the setup is no longer the one the snapshot was taken from (ALIGN-26, ALIGN-39).
	if (!restored) {
		undo_.reset();
		aligned_after_apply_ = false;
	}
	return restored;
}

Outcome Alignment::remember(Outcome outcome)
{
	last_ = outcome;
	return outcome;
}

int64_t Alignment::elapsed_ms(uint64_t since) const
{
	return static_cast<int64_t>((clock_() - since) / static_cast<uint64_t>(ns_per_ms));
}

void Alignment::warn(const std::string &line) const
{
	write_log(log_, LogLevel::Warning, line);
}

void Alignment::info(const std::string &line) const
{
	write_log(log_, LogLevel::Info, line);
}

} // namespace camera_sync
