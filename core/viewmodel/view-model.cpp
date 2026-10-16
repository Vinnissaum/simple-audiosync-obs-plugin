// SPDX-License-Identifier: GPL-2.0-or-later
#include "viewmodel/view-model.hpp"

#include <algorithm>
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
		    bool /*details*/, const Translate &translate)
{
	DockView view;
	for (const SourceInfo &camera : camera_list(sources, choice.mic)) {
		CameraRowView row;
		row.id = camera.id;
		row.name = camera.name;
		row.ticked = contains(choice.cameras, camera.id);
		// A camera that is not ticked, or that the session has dropped, shows nothing (READ-44).
		if (row.ticked) {
			if (const CameraRow *measured = find_row(snapshot.cameras, camera.id))
				row.status = status_text(*measured, translate);
		}
		view.cameras.push_back(std::move(row));
	}
	return view;
}

} // namespace camera_sync
