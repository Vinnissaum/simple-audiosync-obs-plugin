// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

#include "detector/qr-payload.hpp"
#include "detector/video-detector.hpp"
#include "fixtures.hpp"

using camera_sync::LogLevel;
using camera_sync::LumaFormat;
using camera_sync::QrPayload;
using camera_sync::VideoDetector;
using camera_sync::VideoFrame;

// qr-frame.pgm is one frame of Sync Dock's pattern q=4,f=884,c=2 at 60 fps, 320x180, from the marker cycle with index
// 37. Its QR code carries q=66,i=37,f=884,c=2,t=0,I=256 (q is the 4 frames of the QR code in ms, rounded down).
TEST_CASE("video detector decodes the payload of one frame of Sync Dock's pattern")
{
	const Pgm frame = read_pgm("qr-frame.pgm");
	REQUIRE(frame.width == 320);
	REQUIRE(frame.height == 180);

	VideoDetector detector(nullptr, nullptr);
	REQUIRE(detector.start(frame.width, frame.height, LumaFormat::Luma8));
	detector.process(VideoFrame{frame.luma.data(), frame.width, 0});

	const QrPayload &payload = detector.qr_data();
	CHECK(payload.valid);
	CHECK(payload.f == 884);
	CHECK(payload.c == 2);
	CHECK(payload.q_ms == 66);
	CHECK(payload.index == 37);
	CHECK(payload.index_max == 256);
	CHECK(payload.type_flags == 0);
}

TEST_CASE("video detector reports no payload for a frame without a QR code")
{
	const std::vector<uint8_t> grey(320 * 180, 127);

	VideoDetector detector(nullptr, nullptr);
	REQUIRE(detector.start(320, 180, LumaFormat::Luma8));
	detector.process(VideoFrame{grey.data(), 320, 0});

	CHECK_FALSE(detector.qr_data().valid);
}

TEST_CASE("qr payload decodes the keys of Sync Dock's pattern")
{
	std::string text = "q=66,i=37,f=884,c=2,t=0,I=256";
	QrPayload payload;

	CHECK(payload.decode(text.data(), nullptr));
	CHECK(payload.valid);
	CHECK(payload.q_ms == 66);
	CHECK(payload.index == 37);
	CHECK(payload.f == 884);
	CHECK(payload.c == 2);
	CHECK(payload.type_flags == 0);
	CHECK(payload.index_max == 256);
}

TEST_CASE("qr payload rejects a value out of range and logs which one")
{
	// f runs from 10 to 32000, c from 1 to f, q from 1 to 1000 ms and i from 0 to 255.
	struct Case {
		const char *payload;
		const char *logged;
	};
	const Case cases[] = {
		{"q=66,i=37,f=9,c=2", "f: out of range: 9"},      {"q=66,i=37,f=32001,c=2", "f: out of range: 32001"},
		{"q=66,i=37,f=884,c=0", "c: out of range: 0"},    {"q=66,i=37,f=884,c=885", "c: out of range: 885"},
		{"q=0,i=37,f=884,c=2", "q: out of range: 0"},     {"q=1001,i=37,f=884,c=2", "q: out of range: 1001"},
		{"q=66,i=256,f=884,c=2", "i: out of range: 256"},
	};

	for (const Case &entry : cases) {
		CAPTURE(entry.payload);
		std::string text = entry.payload;
		std::string logged;
		LogLevel level = LogLevel::Info;
		QrPayload payload;

		CHECK_FALSE(payload.decode(text.data(), [&](LogLevel l, const std::string &message) {
			level = l;
			logged = message;
		}));
		CHECK_FALSE(payload.valid);
		CHECK(logged == entry.logged);
		CHECK(level == LogLevel::Warning);
	}
}
