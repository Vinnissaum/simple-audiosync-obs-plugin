// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs-tap-factory.hpp"

#include <obs.h>

#include "camera-tap.hpp"
#include "mic-tap.hpp"

namespace camera_sync {

std::unique_ptr<Tap> ObsTapFactory::create_camera_tap(const SourceId &camera, FrameSink &sink)
{
	return CameraTap::create(camera, sink);
}

std::unique_ptr<Tap> ObsTapFactory::create_mic_tap(const SourceId &mic, AudioSink &sink)
{
	return MicTap::create(mic, sink);
}

uint32_t ObsTapFactory::mic_sample_rate()
{
	// The mic tap hands on the source's samples after OBS has resampled them to its own output rate.
	return audio_output_get_sample_rate(obs_get_audio());
}

} // namespace camera_sync
