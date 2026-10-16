// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>

#include "reading/session.hpp"

namespace camera_sync {

// The taps the session measures through, made from OBS: a CameraTap per camera and a MicTap for the mic. A tap that
// cannot be made comes back null, and the session handles it. Call it on the UI thread.
class ObsTapFactory : public TapFactory {
public:
	std::unique_ptr<Tap> create_camera_tap(const SourceId &camera, FrameSink &sink) override;
	std::unique_ptr<Tap> create_mic_tap(const SourceId &mic, AudioSink &sink) override;
	// OBS's audio output rate, which the mic's samples arrive at.
	uint32_t mic_sample_rate() override;
};

} // namespace camera_sync
