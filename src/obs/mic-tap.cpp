// SPDX-License-Identifier: GPL-2.0-or-later
#include "mic-tap.hpp"

#include <obs.h>

namespace camera_sync {

std::unique_ptr<MicTap> MicTap::create(const SourceId &mic, AudioSink &sink)
{
	obs_source_t *source = obs_get_source_by_uuid(mic.c_str());
	if (!source)
		return nullptr;
	obs_weak_source_t *weak = obs_source_get_weak_source(source);
	if (!weak) {
		obs_source_release(source);
		return nullptr;
	}

	std::unique_ptr<MicTap> tap(new MicTap(weak, sink));
	obs_source_add_audio_capture_callback(source, on_audio, tap.get());
	obs_source_release(source);
	return tap;
}

MicTap::MicTap(obs_weak_source_t *mic, AudioSink &sink) : mic_(mic), sink_(sink) {}

MicTap::~MicTap()
{
	// libobs runs the callbacks under the lock the removal takes, so a block in flight finishes first. A source that
	// is already gone took its callbacks with it.
	obs_source_t *source = obs_weak_source_get_source(mic_);
	if (source) {
		obs_source_remove_audio_capture_callback(source, on_audio, this);
		obs_source_release(source);
	}
	obs_weak_source_release(mic_);
}

void MicTap::on_audio(void *param, obs_source_t *source, const struct audio_data *data, bool muted)
{
	MicTap &tap = *static_cast<MicTap *>(param);
	if (!data || !data->data[0] || data->frames == 0 || obs_source_removed(source))
		return;

	// The callback's time is the device's, before the Sync Offset the mix applies, so the offset is added here. It is
	// read for every block, so a change reaches the next one. A time before zero is held at zero.
	const int64_t stamped = static_cast<int64_t>(data->timestamp) + obs_source_get_sync_offset(source);

	AudioBlock block;
	block.frames = data->frames;
	block.timestamp = stamped > 0 ? static_cast<uint64_t>(stamped) : 0;
	if (muted) {
		// A muted block still goes through as silence of the same length and time, so the detector reads the stream
		// without a gap.
		tap.silence_.assign(data->frames, 0.0f);
		block.data[0] = tap.silence_.data();
		block.data[1] = data->data[1] ? tap.silence_.data() : nullptr;
	} else {
		// OBS hands sources' audio over as planar 32-bit float.
		block.data[0] = reinterpret_cast<const float *>(data->data[0]);
		block.data[1] = reinterpret_cast<const float *>(data->data[1]);
	}
	tap.sink_.on_audio(block);
}

} // namespace camera_sync
