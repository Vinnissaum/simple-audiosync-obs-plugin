// SPDX-License-Identifier: GPL-2.0-or-later
#include "selection/selection-policy.hpp"

#include <algorithm>

namespace camera_sync {

namespace {

// ASCII letters compare without case; every other byte compares as it is.
unsigned char fold(char c)
{
	const unsigned char byte = static_cast<unsigned char>(c);
	return byte >= 'A' && byte <= 'Z' ? static_cast<unsigned char>(byte - 'A' + 'a') : byte;
}

bool name_before(const SourceInfo &a, const SourceInfo &b)
{
	const auto folded_less = [](char x, char y) {
		return fold(x) < fold(y);
	};
	if (std::lexicographical_compare(a.name.begin(), a.name.end(), b.name.begin(), b.name.end(), folded_less))
		return true;
	if (std::lexicographical_compare(b.name.begin(), b.name.end(), a.name.begin(), a.name.end(), folded_less))
		return false;
	// Names equal but for case keep one order whatever order OBS listed them in.
	return a.name != b.name ? a.name < b.name : a.id < b.id;
}

bool contains(const std::vector<SourceId> &ids, const SourceId &id)
{
	return std::find(ids.begin(), ids.end(), id) != ids.end();
}

} // namespace

Choice initial_choice(const std::vector<SourceInfo> &sources, const std::optional<SavedSelection> &saved)
{
	Choice choice;

	if (saved && saved->version == saved_selection_version) {
		for (const SourceInfo &source : sources) {
			if (contains(saved->cameras, source.id))
				choice.cameras.push_back(source.id);
			if (saved->mic && source.id == *saved->mic)
				choice.mic = source.id;
		}
		return choice;
	}

	for (const SourceInfo &source : sources)
		if (source.async_video && source.in_program)
			choice.cameras.push_back(source.id);

	// A camera with its own audio counts as a source with audio. When it is the only one and it was just ticked, the
	// ticked camera wins: a source is never both a camera and the mic.
	const SourceInfo *only_audio = nullptr;
	size_t with_audio = 0;
	for (const SourceInfo &source : sources) {
		if (source.audio) {
			with_audio++;
			only_audio = &source;
		}
	}
	if (with_audio == 1 && !contains(choice.cameras, only_audio->id))
		choice.mic = only_audio->id;

	return choice;
}

std::vector<SourceInfo> camera_list(const std::vector<SourceInfo> &sources, const std::optional<SourceId> &mic)
{
	std::vector<SourceInfo> list;
	for (const SourceInfo &source : sources)
		if (source.async_video && (!mic || source.id != *mic))
			list.push_back(source);
	std::sort(list.begin(), list.end(), name_before);
	return list;
}

std::vector<SourceInfo> mic_list(const std::vector<SourceInfo> &sources, const std::vector<SourceId> &ticked)
{
	std::vector<SourceInfo> list;
	for (const SourceInfo &source : sources)
		if (source.audio && !contains(ticked, source.id))
			list.push_back(source);
	std::sort(list.begin(), list.end(), name_before);
	return list;
}

SavedSelection to_saved(const Choice &choice)
{
	return SavedSelection{saved_selection_version, choice.cameras, choice.mic};
}

} // namespace camera_sync
