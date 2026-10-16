// SPDX-License-Identifier: GPL-2.0-or-later
#include "selection-store.hpp"

#include <string>
#include <utility>

#include <obs-frontend-api.h>
#include <obs.h>

namespace camera_sync {

namespace {

// The plugin's key in the collection's module data. It is the module's name and must not change, or saved selections
// are lost.
constexpr const char *module_key = "camera-sync";

} // namespace

SelectionStore::SelectionStore(Current current) : current_(std::move(current))
{
	obs_frontend_add_save_callback(on_save, this);
}

SelectionStore::~SelectionStore()
{
	obs_frontend_remove_save_callback(on_save, this);
}

void SelectionStore::on_save(obs_data_t *save_data, bool saving, void *param)
{
	SelectionStore &store = *static_cast<SelectionStore *>(param);
	if (saving)
		store.write(save_data);
	else
		store.read(save_data);
}

void SelectionStore::write(obs_data_t *save_data) const
{
	if (!save_data || !current_)
		return;
	const SavedSelection saved = current_();

	obs_data_t *selection = obs_data_create();
	obs_data_set_int(selection, "version", saved.version);

	obs_data_array_t *cameras = obs_data_array_create();
	for (const SourceId &camera : saved.cameras) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "uuid", camera.c_str());
		obs_data_array_push_back(cameras, item);
		obs_data_release(item);
	}
	obs_data_set_array(selection, "cameras", cameras);
	obs_data_array_release(cameras);

	if (saved.mic)
		obs_data_set_string(selection, "mic", saved.mic->c_str());

	obs_data_set_obj(save_data, module_key, selection);
	obs_data_release(selection);
}

void SelectionStore::read(obs_data_t *save_data)
{
	// OBS passes no module data for a collection that has none, and a key that is not an object is malformed: either
	// way the collection saved no selection.
	saved_.reset();
	obs_data_t *selection = save_data ? obs_data_get_obj(save_data, module_key) : nullptr;
	if (!selection)
		return;

	// The version is passed through for initial_choice to judge. A missing one reads as 0, and one that does not fit
	// an int as 0 too, rather than wrapping onto a known version.
	SavedSelection saved;
	const long long version = obs_data_get_int(selection, "version");
	saved.version = version == static_cast<int>(version) ? static_cast<int>(version) : 0;

	obs_data_array_t *cameras = obs_data_get_array(selection, "cameras");
	const size_t count = obs_data_array_count(cameras);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(cameras, i);
		const char *uuid = obs_data_get_string(item, "uuid");
		if (uuid && *uuid)
			saved.cameras.emplace_back(uuid);
		obs_data_release(item);
	}
	obs_data_array_release(cameras);

	const char *mic = obs_data_get_string(selection, "mic");
	if (mic && *mic)
		saved.mic = mic;

	obs_data_release(selection);
	saved_ = std::move(saved);
}

} // namespace camera_sync
