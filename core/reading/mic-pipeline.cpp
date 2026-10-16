// SPDX-License-Identifier: GPL-2.0-or-later
#include "reading/mic-pipeline.hpp"

#include <utility>

namespace camera_sync {

MicPipeline::MicPipeline(Logger log, uint32_t sample_rate, AudioMarkerFn on_marker)
	: detector_(std::move(log), std::move(on_marker))
{
	detector_.start(sample_rate);
}

void MicPipeline::on_audio(const AudioBlock &block)
{
	detector_.process(block);
}

void MicPipeline::set_pattern(uint32_t f, uint32_t c, uint32_t q_ms)
{
	detector_.set_pattern(f, c, q_ms);
}

} // namespace camera_sync
