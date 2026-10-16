// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <optional>

#include "selection/selection-policy.hpp"

struct obs_data;

namespace camera_sync {

// Keeps the selection in the scene collection's module data, under the plugin's key:
// {"version": 1, "cameras": [{"uuid": <UUID>}, ...], "mic": <UUID>}, with "mic" left out when no mic is chosen.
// OBS 30's obs_data holds arrays of objects only and writes no null, hence the objects and the missing key. The store
// copies fields and decides nothing: initial_choice judges the version and the UUIDs.
//
// It registers OBS's save callback for its lifetime, so destroy it before the frontend goes, at the exit event at the
// latest. OBS calls it on the UI thread.
class SelectionStore {
public:
	// What to save, asked each time OBS saves the collection. On a collection switch OBS saves the old collection
	// after the switch has begun, so it has to answer with the old collection's selection until the new one loads.
	using Current = std::function<SavedSelection()>;

	explicit SelectionStore(Current current);
	~SelectionStore();
	SelectionStore(const SelectionStore &) = delete;
	SelectionStore &operator=(const SelectionStore &) = delete;

	// What the collection OBS loaded last had saved, or none when it saved nothing readable. OBS loads a collection
	// once its sources exist and before it reports the collection loaded.
	const std::optional<SavedSelection> &saved() const { return saved_; }

	// Drops what the last load found, when a collection switch begins. OBS creates or duplicates a collection without
	// loading one, and such a collection saved nothing.
	void forget() { saved_.reset(); }

private:
	static void on_save(struct obs_data *save_data, bool saving, void *param);
	void write(struct obs_data *save_data) const;
	void read(struct obs_data *save_data);

	Current current_;
	std::optional<SavedSelection> saved_;
};

} // namespace camera_sync
