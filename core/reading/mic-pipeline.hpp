// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

#include "detector/audio-detector.hpp"
#include "log.hpp"
#include "media.hpp"

namespace camera_sync {

// Where the mic's tap delivers its samples.
class AudioSink {
public:
	virtual ~AudioSink() = default;
	virtual void on_audio(const AudioBlock &block) = 0;
};

// The mic's detection: one audio detector, started at the mic's sample rate and fed by the mic's tap on the mic's
// audio thread. It hears no marker until the session hands it the pattern a camera read.
class MicPipeline : public AudioSink {
public:
	MicPipeline(Logger log, uint32_t sample_rate, AudioMarkerFn on_marker);

	// Reads one block of samples, mono or stereo. Blocks arrive in time order.
	void on_audio(const AudioBlock &block) override;

	// The pattern's audio carrier f, its cycles c per symbol and the QR frames' duration q_ms. Any thread may call it
	// while blocks are read.
	void set_pattern(uint32_t f, uint32_t c, uint32_t q_ms);

private:
	AudioDetector detector_;
};

} // namespace camera_sync
