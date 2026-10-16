// SPDX-License-Identifier: GPL-2.0-or-later
#include "viewmodel/view-model.hpp"

#include <algorithm>
#include <optional>
#include <utility>

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

// The message line: the pick prompt first, then a mic that could not be listened to, then a mic that hears no pattern
// sound (READ-36, READ-42, READ-43).
std::string message_text(const SessionSnapshot &snapshot, const std::vector<SourceInfo> &sources, const Choice &choice,
			 bool nothing_chosen, const Translate &translate)
{
	if (nothing_chosen)
		return translate(strings::message_pick);
	const std::string mic = name_of(sources, snapshot.mic ? snapshot.mic : choice.mic);
	if (snapshot.mic_tap_failed)
		return strings::fill(translate(strings::message_mic_tap), {{"mic", mic}});
	if (snapshot.no_pattern_sound)
		return strings::fill(translate(strings::message_no_sound), {{"mic", mic}});
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

DockView build_view(const SessionSnapshot &snapshot, const std::vector<SourceInfo> &sources, const Choice &choice,
		    bool details, const Translate &translate)
{
	DockView view;
	for (const SourceInfo &camera : camera_list(sources, choice.mic)) {
		CameraRowView row;
		row.id = camera.id;
		row.name = camera.name;
		row.ticked = contains(choice.cameras, camera.id);
		// A camera that is not ticked, or that the session has dropped, shows nothing (READ-44).
		if (row.ticked) {
			if (const CameraRow *measured = find_row(snapshot.cameras, camera.id)) {
				row.status = status_text(*measured, translate);
				if (details)
					row.details = strings::fill(translate(strings::row_details),
								    {{"n", std::to_string(measured->received)},
								     {"m", std::to_string(measured->missed)}});
			}
		}
		view.cameras.push_back(std::move(row));
	}

	for (const SourceInfo &mic : mic_list(sources, choice.cameras))
		view.mics.push_back(MicView{mic.id, mic.name, choice.mic && *choice.mic == mic.id});

	const bool nothing_chosen = choice.cameras.empty() || !choice.mic;
	const bool measuring = snapshot.state == SessionState::Measuring;
	view.message = message_text(snapshot, sources, choice, nothing_chosen, translate);
	// While measuring the button stops the session, which the user can always do (READ-45, READ-47).
	view.start_enabled = measuring || !nothing_chosen;
	view.show_details = details;
	view.start_label = translate(measuring ? strings::button_stop : strings::button_start);
	view.reset_enabled = snapshot.state != SessionState::Idle; // READ-46
	return view;
}

} // namespace camera_sync
