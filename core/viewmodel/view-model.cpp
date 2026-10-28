// SPDX-License-Identifier: GPL-2.0-or-later
#include "viewmodel/view-model.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "alignment/calculator.hpp"
#include "viewmodel/strings.hpp"

namespace camera_sync {

namespace {

bool contains(const std::vector<SourceId> &ids, const SourceId &id)
{
	return std::find(ids.begin(), ids.end(), id) != ids.end();
}

// What a ticked camera's row says. Settling shows the live value and a Stable camera its reading (READ-37 to READ-41).
std::string status_text(const CameraRow &row, const Translate &translate)
{
	switch (row.state) {
	case CameraState::Looking:
		return translate(strings::row_looking);
	case CameraState::Settling:
		return strings::fill(translate(strings::row_settling), {{"value", format_signed_ms(row.live_ms)}});
	case CameraState::Stable:
		break;
	}
	if (row.reading_ms > 0)
		return strings::fill(translate(strings::row_video_late), {{"value", format_signed_ms(row.reading_ms)}});
	if (row.reading_ms < 0)
		return strings::fill(translate(strings::row_video_early),
				     {{"value", format_signed_ms(row.reading_ms)}});
	return translate(strings::row_in_sync);
}

const CameraRow *find_row(const std::vector<CameraRow> &rows, const SourceId &id)
{
	for (const CameraRow &row : rows) {
		if (row.id == id)
			return &row;
	}
	return nullptr;
}

// The name of the source with this id, or none when the catalog does not list it.
std::string name_of(const std::vector<SourceInfo> &sources, const std::optional<SourceId> &id)
{
	if (!id)
		return {};
	for (const SourceInfo &source : sources) {
		if (source.id == *id)
			return source.name;
	}
	return {};
}

// "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms" and its Undo twin.
std::string values_text(const char *key, const std::vector<NamedValue> &values, const Translate &translate)
{
	std::string list;
	for (const NamedValue &value : values) {
		if (!list.empty())
			list += ", ";
		list += strings::fill(translate(strings::message_value),
				      {{"name", value.name}, {"value", format_value_ms(value.ms)}});
	}
	return strings::fill(translate(key), {{"values", list}});
}

// What the last Apply or Undo says (ALIGN-50 to ALIGN-56).
std::string result_text(const Outcome &outcome, const Translate &translate)
{
	switch (outcome.kind) {
	case OutcomeKind::AlreadyAligned:
		return translate(strings::message_already_aligned);
	case OutcomeKind::OverLimit:
		return strings::fill(translate(strings::message_over_limit), {{"camera", outcome.source}});
	case OutcomeKind::ApplyFailed:
		return translate(strings::message_apply_failed);
	case OutcomeKind::ApplyRestoreFailed:
		return translate(strings::message_apply_restore_failed);
	case OutcomeKind::Applied:
		return values_text(strings::message_applied, outcome.values, translate);
	case OutcomeKind::UndoFailed:
		return translate(strings::message_undo_failed);
	case OutcomeKind::UndoRestoreFailed:
		return translate(strings::message_undo_restore_failed);
	case OutcomeKind::Undone:
		return values_text(strings::message_undone, outcome.values, translate);
	case OutcomeKind::Waiting:
	case OutcomeKind::Cancelled:
		break;
	}
	return {};
}

// What the message line ranks below the session's own messages.
struct AlignmentLine {
	bool aligned = false; // every ticked camera carries the check mark
	std::string waiting;  // the first ticked camera in checklist order the session reads as not Stable
	std::string result;   // the last Apply or Undo's message
};

// The message line, highest first: the pick prompt, a mic that could not be listened to, a mic that hears no pattern
// sound (READ-36, READ-42, READ-43), then "Aligned", the last result and "Waiting for {camera}" (ALIGN-60).
std::string message_text(const SessionSnapshot &snapshot, const std::vector<SourceInfo> &sources, const Choice &choice,
			 bool nothing_chosen, const AlignmentLine &alignment, const Translate &translate)
{
	if (nothing_chosen)
		return translate(strings::message_pick);
	const std::string mic = name_of(sources, snapshot.mic ? snapshot.mic : choice.mic);
	if (snapshot.mic_tap_failed)
		return strings::fill(translate(strings::message_mic_tap), {{"mic", mic}});
	if (snapshot.no_pattern_sound)
		return strings::fill(translate(strings::message_no_sound), {{"mic", mic}});
	if (alignment.aligned)
		return translate(strings::message_aligned);
	if (!alignment.result.empty())
		return alignment.result;
	if (!alignment.waiting.empty())
		return strings::fill(translate(strings::message_waiting), {{"camera", alignment.waiting}});
	return {};
}

} // namespace

std::string format_signed_ms(int64_t ms)
{
	if (ms == 0)
		return "0";
	// The magnitude is taken unsigned so the most negative value needs no special case.
	const uint64_t magnitude = ms > 0 ? static_cast<uint64_t>(ms) : 0 - static_cast<uint64_t>(ms);
	return (ms > 0 ? "+" : "\xE2\x88\x92") + std::to_string(magnitude);
}

std::string format_value_ms(int64_t ms)
{
	return ms < 0 ? format_signed_ms(ms) : std::to_string(ms);
}

DockView build_view(const SessionSnapshot &snapshot, const std::vector<SourceInfo> &sources, const Choice &choice,
		    bool details, const Translate &translate, const AlignmentStatus &alignment)
{
	DockView view;
	AlignmentLine line;
	line.aligned = alignment.aligned_after_apply;
	for (const SourceInfo &camera : camera_list(sources, choice.mic)) {
		CameraRowView row;
		row.id = camera.id;
		row.name = camera.name;
		row.ticked = contains(choice.cameras, camera.id);
		// A camera that is not ticked, or that the session has dropped, shows nothing (READ-44).
		if (row.ticked) {
			const CameraRow *measured = find_row(snapshot.cameras, camera.id);
			// After an Apply, a ticked camera stable within one frame is marked; "Aligned" waits for every
			// one (ALIGN-58, ALIGN-59).
			const bool stable = measured && measured->state == CameraState::Stable;
			const bool marked = alignment.aligned_after_apply && stable &&
					    within_one_frame(measured->reading_ms, snapshot.frame_interval_ms);
			line.aligned = line.aligned && marked;
			if (measured) {
				row.status = status_text(*measured, translate);
				if (marked)
					row.status = strings::fill(translate(strings::row_aligned),
								   {{"status", row.status}});
				if (details)
					row.details = strings::fill(translate(strings::row_details),
								    {{"n", std::to_string(measured->received)},
								     {"m", std::to_string(measured->missed)}});
				// The session reads it but not yet as Stable: Apply waits for it (ALIGN-48). A camera
				// the session has dropped is named nowhere, as READ-44 has it.
				if (measured->state != CameraState::Stable && line.waiting.empty())
					line.waiting = camera.name;
			}
		}
		view.cameras.push_back(std::move(row));
	}

	for (const SourceInfo &mic : mic_list(sources, choice.cameras))
		view.mics.push_back(MicView{mic.id, mic.name, choice.mic && *choice.mic == mic.id});

	const bool nothing_chosen = choice.cameras.empty() || !choice.mic;
	const bool measuring = snapshot.state == SessionState::Measuring;
	if (alignment.last)
		line.result = result_text(*alignment.last, translate);
	view.message = message_text(snapshot, sources, choice, nothing_chosen, line, translate);
	// Apply is enabled by the rule that lets it go ahead: every ticked camera Stable (ALIGN-47).
	const Precheck check = precheck(readings_of(choice.cameras, snapshot.cameras), snapshot.frame_interval_ms);
	view.apply_enabled = !nothing_chosen && check.kind != PrecheckKind::Waiting;
	view.undo_enabled = alignment.undo_available; // ALIGN-49
	// While measuring the button stops the session, which the user can always do (READ-45, READ-47).
	view.start_enabled = measuring || !nothing_chosen;
	view.show_details = details;
	view.start_label = translate(measuring ? strings::button_stop : strings::button_start);
	view.reset_enabled = snapshot.state != SessionState::Idle; // READ-46
	return view;
}

std::string confirmation_question(const Counts &counts, const Translate &translate)
{
	const std::string n = std::to_string(counts.cameras);
	if (counts.cameras == 0)
		return counts.mic ? translate(strings::question_mic) : std::string();
	if (counts.cameras == 1)
		return translate(counts.mic ? strings::question_camera_and_mic : strings::question_camera);
	return strings::fill(translate(counts.mic ? strings::question_cameras_and_mic : strings::question_cameras),
			     {{"n", n}});
}

std::string confirmation_detail(const Counts &counts, const Translate &translate)
{
	return counts.cameras > 0 ? translate(strings::question_pause) : std::string();
}

} // namespace camera_sync
