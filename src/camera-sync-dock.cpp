// SPDX-License-Identifier: GPL-2.0-or-later
#include "camera-sync-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QLabel>
#include <QString>

static const char *DOCK_ID = "camera-sync-dock";

void camera_sync_register_dock()
{
	// OBS puts the widget in a dock and owns it from then on. The dock holds one line of text and no controls.
	auto *label = new QLabel(QString("Camera Sync %1").arg(QString::fromUtf8(PLUGIN_VERSION)));

	if (!obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("CameraSync.Dock.Title"), label)) {
		obs_log(LOG_WARNING, "could not add the dock %s", DOCK_ID);
		delete label;
	}
}
