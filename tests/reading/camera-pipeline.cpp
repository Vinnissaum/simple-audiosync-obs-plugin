// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "reading/camera-pipeline.hpp"
#include "support/fixtures.hpp"

using camera_sync::CameraPipeline;
using camera_sync::Clock;
using camera_sync::FrameSink;
using camera_sync::Logger;
using camera_sync::LogLevel;
using camera_sync::LumaFormat;
using camera_sync::VideoFrame;

namespace {

// A clock the test moves by hand.
struct FakeClock {
	uint64_t now = 0;
	Clock clock()
	{
		return [this] {
			return now;
		};
	}
};

struct Log {
	std::vector<std::pair<LogLevel, std::string>> lines;
	Logger logger()
	{
		return [this](LogLevel level, const std::string &message) {
			lines.emplace_back(level, message);
		};
	}
};

// Sends one Luma8 frame through the pipeline's FrameSink, the only way a tap reaches it.
void deliver(FrameSink &sink, const std::vector<uint8_t> &luma, uint32_t width, uint32_t height, uint64_t ts = 0)
{
	sink.on_frame(VideoFrame{luma.data(), width, ts}, width, height, LumaFormat::Luma8);
}

// The same picture as 4 bytes per pixel with the brightness in every colour channel, as an RGBA frame carries it.
std::vector<uint8_t> to_rgba(const std::vector<uint8_t> &luma)
{
	std::vector<uint8_t> rgba;
	rgba.reserve(luma.size() * 4);
	for (uint8_t y : luma) {
		rgba.push_back(y);
		rgba.push_back(y);
		rgba.push_back(y);
		rgba.push_back(255);
	}
	return rgba;
}

} // namespace

// qr-frame.pgm is one 320x180 frame of Sync Dock's pattern q=4,f=884,c=2 at 60 fps, from the marker cycle with index 37.
TEST_CASE("camera pipeline starts its detector on the first frame and decodes the pattern's qr code")
{
	const Pgm frame = read_pgm("qr-frame.pgm");
	CameraPipeline pipeline(nullptr, [] { return uint64_t(0); }, nullptr);
	CHECK_FALSE(pipeline.qr_data().valid);

	deliver(pipeline, frame.luma, frame.width, frame.height);

	CHECK(pipeline.qr_data().valid);
	CHECK(pipeline.qr_data().index == 37);
}

TEST_CASE("camera pipeline restarts its detector when the frame size or format changes")
{
	const Pgm frame = read_pgm("qr-frame.pgm");

	SUBCASE("size")
	{
		// A detector still sized for 160x90 would read only the top left of the 320x180 frame and find no QR code.
		const std::vector<uint8_t> small(160 * 90, 127);
		CameraPipeline pipeline(nullptr, [] { return uint64_t(0); }, nullptr);

		deliver(pipeline, small, 160, 90);
		CHECK_FALSE(pipeline.qr_data().valid);
		deliver(pipeline, frame.luma, frame.width, frame.height);
		CHECK(pipeline.qr_data().valid);
		CHECK(pipeline.qr_data().index == 37);

		// Back to 160x90 and to 320x180 again: no crash, and the QR code still decodes.
		deliver(pipeline, small, 160, 90);
		deliver(pipeline, frame.luma, frame.width, frame.height);
		CHECK(pipeline.qr_data().valid);
		CHECK(pipeline.qr_data().index == 37);
	}

	SUBCASE("format")
	{
		// A detector still reading one byte per pixel would see a stretched quarter of the RGBA frame.
		const std::vector<uint8_t> grey(320 * 180, 127);
		const std::vector<uint8_t> rgba = to_rgba(frame.luma);
		CameraPipeline pipeline(nullptr, [] { return uint64_t(0); }, nullptr);

		deliver(pipeline, grey, 320, 180);
		CHECK_FALSE(pipeline.qr_data().valid);
		FrameSink &sink = pipeline;
		sink.on_frame(VideoFrame{rgba.data(), frame.width * 4, 0}, frame.width, frame.height,
			      LumaFormat::Green32);
		CHECK(pipeline.qr_data().valid);
		CHECK(pipeline.qr_data().index == 37);
	}
}

TEST_CASE("camera pipeline drops a frame with no width or no data with a warning")
{
	const Pgm frame = read_pgm("qr-frame.pgm");
	FakeClock clock;
	clock.now = 100;
	Log log;
	CameraPipeline pipeline(log.logger(), clock.clock(), nullptr);
	FrameSink &sink = pipeline;

	clock.now = 200;
	sink.on_frame(VideoFrame{frame.luma.data(), frame.width, 0}, 0, frame.height, LumaFormat::Luma8);
	REQUIRE(log.lines.size() == 1);
	CHECK(log.lines[0].first == LogLevel::Warning);
	CHECK(pipeline.last_frame_ns() == 100);

	clock.now = 300;
	sink.on_frame(VideoFrame{nullptr, frame.width, 0}, frame.width, frame.height, LumaFormat::Luma8);
	REQUIRE(log.lines.size() == 2);
	CHECK(log.lines[1].first == LogLevel::Warning);
	CHECK(pipeline.last_frame_ns() == 100);
	CHECK_FALSE(pipeline.qr_data().valid);
}

TEST_CASE("camera pipeline stamps the clock reading at construction and on each accepted frame")
{
	const std::vector<uint8_t> grey(320 * 180, 127);
	FakeClock clock;
	clock.now = 1000;
	CameraPipeline pipeline(nullptr, clock.clock(), nullptr);
	CHECK(pipeline.last_frame_ns() == 1000);

	// The frame's own time is not the clock's: the stamp is the clock reading when the frame arrives.
	clock.now = 5000;
	deliver(pipeline, grey, 320, 180, 42);
	CHECK(pipeline.last_frame_ns() == 5000);

	clock.now = 7500;
	deliver(pipeline, grey, 320, 180, 43);
	CHECK(pipeline.last_frame_ns() == 7500);
}

TEST_CASE("camera pipelines built together share no state")
{
	const Pgm frame = read_pgm("qr-frame.pgm");
	FakeClock clock;
	clock.now = 10;
	CameraPipeline first(nullptr, clock.clock(), nullptr);
	CameraPipeline second(nullptr, clock.clock(), nullptr);

	clock.now = 20;
	deliver(first, frame.luma, frame.width, frame.height);

	CHECK(first.qr_data().valid);
	CHECK_FALSE(second.qr_data().valid);
	CHECK(first.last_frame_ns() == 20);
	CHECK(second.last_frame_ns() == 10);
}
