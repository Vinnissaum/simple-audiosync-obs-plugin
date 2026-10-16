// SPDX-License-Identifier: GPL-2.0-or-later
#include "reading/pattern.hpp"

#include <cmath>

namespace camera_sync {

PatternParams PatternParams::from(const QrPayload &payload)
{
	return PatternParams{payload.f, payload.c, payload.q_ms, payload.index_max};
}

bool PatternParams::operator==(const PatternParams &other) const
{
	return f == other.f && c == other.c && q_ms == other.q_ms && index_max == other.index_max;
}

uint64_t PatternParams::half_wrap_ns() const
{
	return uint64_t(index_max) * 3 * q_ms * 1000000 / 2;
}

int64_t round_half_away(double ms)
{
	return std::llround(ms);
}

double frame_interval_ms(uint32_t fps_num, uint32_t fps_den)
{
	return 1000.0 * fps_den / fps_num;
}

} // namespace camera_sync
