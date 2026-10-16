// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

#include "detector/qr-payload.hpp"

namespace camera_sync {

// What tells one pattern from another: the audio carrier f in Hz and its cycles c per pulse, the QR frames' duration q
// in ms, and the number of marker indexes before they wrap.
struct PatternParams {
	uint32_t f = 0;
	uint32_t c = 0;
	uint32_t q_ms = 0;
	uint32_t index_max = 0;

	// The parameters of the pattern a QR payload belongs to.
	static PatternParams from(const QrPayload &payload);

	bool operator==(const PatternParams &other) const;

	// Half of one wrap. A marker cycle lasts 3 x q_ms, so a wrap lasts index_max x 3 x q_ms. A video marker and an
	// audio marker with the same index pair only when they are less than this apart.
	uint64_t half_wrap_ns() const;
};

// The nearest whole number, halves away from zero: 141.5 gives 142 and -141.5 gives -142.
int64_t round_half_away(double ms);

// The canvas frame interval in ms, 1000 x fps_den / fps_num. It is the tolerance F a stable window must fit in.
double frame_interval_ms(uint32_t fps_num, uint32_t fps_den);

} // namespace camera_sync
