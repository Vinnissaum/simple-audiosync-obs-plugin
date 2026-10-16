// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace camera_sync {
class Controller;
}

// Registers the Camera Sync dock with OBS's main window and hands it to the controller, which repaints it. Call it once,
// from obs_module_load.
void camera_sync_register_dock(camera_sync::Controller &controller);
