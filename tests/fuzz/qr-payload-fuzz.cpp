// SPDX-License-Identifier: GPL-2.0-or-later
// The fuzz target for the QR payload parser. A camera can see any QR code, so decode takes whatever text it is given
// and must never accept a value outside the ranges the pattern defines. The same function runs under libFuzzer
// (qr-payload-fuzz) and over the seed corpus in CTest (qr-payload-replay).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "detector/qr-payload.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// decode tokenizes in place and needs a NUL-terminated string, as the video detector hands it one.
	std::vector<char> text(data, data + size);
	text.push_back('\0');

	camera_sync::QrPayload payload;
	if (!payload.decode(text.data(), nullptr))
		return 0;

	if (!payload.valid || payload.f < 10 || 32000 < payload.f || payload.c < 1 || payload.f < payload.c ||
	    payload.q_ms < 1 || 1000 < payload.q_ms || 255 < payload.index) {
		std::fprintf(stderr, "accepted out of range: f=%u c=%u q=%u i=%u valid=%d\n", (unsigned)payload.f,
			     (unsigned)payload.c, (unsigned)payload.q_ms, (unsigned)payload.index,
			     payload.valid ? 1 : 0);
		std::abort();
	}
	return 0;
}
