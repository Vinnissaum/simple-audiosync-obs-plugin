// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "reading/mic-pipeline.hpp"
#include "reading/session.hpp"

struct audio_data;
struct obs_source;
struct obs_weak_source;

namespace camera_sync {

// The mic's tap: the mic's own samples from its audio capture callback, before OBS mixes them with any other source.
// Each block is stamped with the mic's device time plus its current Sync Offset, which the callback's time leaves
// out, and delivered to an AudioSink on the mic's audio thread. It adds nothing to the mic and changes none of its
// settings.
class MicTap : public Tap {
public:
	// The tap on the source with this UUID, or null when no source has it. Call it on the UI thread.
	static std::unique_ptr<MicTap> create(const SourceId &mic, AudioSink &sink);

	// Removes the callback, waiting for a block in flight.
	~MicTap() override;
	MicTap(const MicTap &) = delete;
	MicTap &operator=(const MicTap &) = delete;

	// Audio has no frames to skip: the mic's callback hands over every block.
	uint64_t skipped_frames() const override { return 0; }

private:
	MicTap(struct obs_weak_source *mic, AudioSink &sink);

	static void on_audio(void *param, struct obs_source *source, const struct audio_data *data, bool muted);

	struct obs_weak_source *mic_;
	AudioSink &sink_;
	std::vector<float> silence_; // the zeros a muted block is replaced with, used on the audio thread only
};

} // namespace camera_sync
