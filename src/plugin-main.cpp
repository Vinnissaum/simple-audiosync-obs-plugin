// SPDX-License-Identifier: GPL-2.0-or-later
/*
Camera Sync
Copyright (C) 2026 Vinicius Braz

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>

#include "camera-sync-dock.hpp"
#include "load-line.hpp"
#include "obs/camera-tap.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	const std::string line = camera_sync::load_line(PLUGIN_VERSION, LIBOBS_API_VER, obs_get_version_string(),
							CAMERA_SYNC_QUIRC_COMMIT);
	obs_log(LOG_INFO, "%s", line.c_str());

	camera_sync::CameraTap::register_output();
	camera_sync_register_dock();
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
