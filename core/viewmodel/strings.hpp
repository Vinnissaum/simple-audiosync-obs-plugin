// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace camera_sync::strings {

// The locale keys the dock uses, one constant for each. data/locale/en-US.ini holds their en-US text, and a test keeps
// the two in step (READ-48). The dock's title is the plugin's alone and is not listed here.
inline constexpr const char *row_looking = "CameraSync.Row.Looking";
inline constexpr const char *row_settling = "CameraSync.Row.Settling";
inline constexpr const char *row_video_late = "CameraSync.Row.VideoLate";
inline constexpr const char *row_video_early = "CameraSync.Row.VideoEarly";
inline constexpr const char *row_in_sync = "CameraSync.Row.InSync";
inline constexpr const char *row_details = "CameraSync.Row.Details";
inline constexpr const char *message_pick = "CameraSync.Message.Pick";
inline constexpr const char *message_no_sound = "CameraSync.Message.NoSound";
inline constexpr const char *message_mic_tap = "CameraSync.Message.MicTap";
inline constexpr const char *button_start = "CameraSync.Button.Start";
inline constexpr const char *button_stop = "CameraSync.Button.Stop";
inline constexpr const char *button_reset = "CameraSync.Button.Reset";
inline constexpr const char *label_mic = "CameraSync.Label.Mic";
inline constexpr const char *label_pattern = "CameraSync.Label.Pattern";
inline constexpr const char *label_details = "CameraSync.Label.Details";

// Alignment: Apply, Undo and the on-air question (ALIGN-63).
inline constexpr const char *button_apply = "CameraSync.Button.Apply";
inline constexpr const char *button_undo = "CameraSync.Button.Undo";
inline constexpr const char *button_cancel = "CameraSync.Button.Cancel";
inline constexpr const char *row_aligned = "CameraSync.Row.Aligned";
inline constexpr const char *message_waiting = "CameraSync.Message.Waiting";
inline constexpr const char *message_already_aligned = "CameraSync.Message.AlreadyAligned";
inline constexpr const char *message_over_limit = "CameraSync.Message.OverLimit";
inline constexpr const char *message_apply_failed = "CameraSync.Message.ApplyFailed";
inline constexpr const char *message_apply_restore_failed = "CameraSync.Message.ApplyRestoreFailed";
inline constexpr const char *message_undo_failed = "CameraSync.Message.UndoFailed";
inline constexpr const char *message_undo_restore_failed = "CameraSync.Message.UndoRestoreFailed";
inline constexpr const char *message_applied = "CameraSync.Message.Applied";
inline constexpr const char *message_undone = "CameraSync.Message.Undone";
inline constexpr const char *message_value = "CameraSync.Message.Value";
inline constexpr const char *message_aligned = "CameraSync.Message.Aligned";
inline constexpr const char *question_cameras_and_mic = "CameraSync.Question.CamerasAndMic";
inline constexpr const char *question_camera_and_mic = "CameraSync.Question.CameraAndMic";
inline constexpr const char *question_cameras = "CameraSync.Question.Cameras";
inline constexpr const char *question_camera = "CameraSync.Question.Camera";
inline constexpr const char *question_mic = "CameraSync.Question.Mic";
inline constexpr const char *question_pause = "CameraSync.Question.Pause";

// Every key above.
inline constexpr std::array<const char *, 36> all_keys = {
	row_looking,
	row_settling,
	row_video_late,
	row_video_early,
	row_in_sync,
	row_details,
	message_pick,
	message_no_sound,
	message_mic_tap,
	button_start,
	button_stop,
	button_reset,
	label_mic,
	label_pattern,
	label_details,
	button_apply,
	button_undo,
	button_cancel,
	row_aligned,
	message_waiting,
	message_already_aligned,
	message_over_limit,
	message_apply_failed,
	message_apply_restore_failed,
	message_undo_failed,
	message_undo_restore_failed,
	message_applied,
	message_undone,
	message_value,
	message_aligned,
	question_cameras_and_mic,
	question_camera_and_mic,
	question_cameras,
	question_camera,
	question_mic,
	question_pause,
};

// A placeholder's name and the text that replaces it.
using Placeholder = std::pair<std::string, std::string>;

// The text with each {name} that has a replacement swapped for it. A placeholder with no replacement stays as
// written, and a replacement is inserted as it is, so a "{" in it is never expanded again. Locale files keep their
// word order because the code names the placeholders and never their position.
inline std::string fill(const std::string &text, const std::vector<Placeholder> &replacements)
{
	std::string result;
	size_t at = 0;
	while (at < text.size()) {
		const size_t open = text.find('{', at);
		if (open == std::string::npos)
			break;
		const size_t close = text.find('}', open);
		if (close == std::string::npos)
			break;
		const std::string name = text.substr(open + 1, close - open - 1);
		const Placeholder *found = nullptr;
		for (const Placeholder &replacement : replacements) {
			if (replacement.first == name) {
				found = &replacement;
				break;
			}
		}
		if (!found) {
			// Not a placeholder this call fills: keep the brace and look again just after it.
			result.append(text, at, open + 1 - at);
			at = open + 1;
			continue;
		}
		result.append(text, at, open - at);
		result += found->second;
		at = close + 1;
	}
	result.append(text, at, std::string::npos);
	return result;
}

} // namespace camera_sync::strings
