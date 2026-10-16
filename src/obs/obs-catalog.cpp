// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs-catalog.hpp"

#include <set>
#include <string>
#include <utility>

#include <obs-frontend-api.h>
#include <obs.h>

namespace camera_sync {

namespace {

// The UUIDs of the sources in the Program scene, and of the scenes and groups already walked. A scene is walked once,
// so a scene that holds itself, directly or through another, ends the walk instead of looping.
struct ProgramScan {
	std::set<std::string> sources;
	std::set<std::string> scenes;
};

void scan_scene(obs_source_t *scene_source, ProgramScan &scan);

bool scan_item(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	ProgramScan &scan = *static_cast<ProgramScan *>(param);
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *uuid = source ? obs_source_get_uuid(source) : nullptr;
	if (!uuid)
		return true;
	scan.sources.insert(uuid);
	// A nested scene or a group: the sources inside it are in Program too.
	scan_scene(source, scan);
	return true;
}

void scan_scene(obs_source_t *scene_source, ProgramScan &scan)
{
	obs_scene_t *scene = obs_group_or_scene_from_source(scene_source);
	const char *uuid = obs_source_get_uuid(scene_source);
	if (!scene || !uuid || !scan.scenes.insert(uuid).second)
		return;
	obs_scene_enum_items(scene, scan_item, &scan);
}

struct Listing {
	const ProgramScan *program;
	std::vector<SourceInfo> sources;
};

bool list_source(void *param, obs_source_t *source)
{
	Listing &listing = *static_cast<Listing *>(param);
	const char *uuid = obs_source_get_uuid(source);
	if (!uuid)
		return true;
	const char *name = obs_source_get_name(source);
	const uint32_t flags = obs_source_get_output_flags(source);

	SourceInfo info;
	info.id = uuid;
	info.name = name ? name : "";
	info.async_video = (flags & OBS_SOURCE_ASYNC_VIDEO) == OBS_SOURCE_ASYNC_VIDEO;
	info.audio = (flags & OBS_SOURCE_AUDIO) != 0;
	info.in_program = listing.program->sources.count(info.id) != 0;
	listing.sources.push_back(std::move(info));
	return true;
}

} // namespace

std::vector<SourceInfo> ObsCatalog::list() const
{
	// The frontend's current scene is Program, in studio mode as well. It comes with a reference to release.
	ProgramScan program;
	obs_source_t *current = obs_frontend_get_current_scene();
	if (current) {
		scan_scene(current, program);
		obs_source_release(current);
	}

	// obs_enum_sources lends each source for the length of the call, so nothing is held after it.
	Listing listing{&program, {}};
	obs_enum_sources(list_source, &listing);
	return std::move(listing.sources);
}

} // namespace camera_sync
