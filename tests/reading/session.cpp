// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "reading/session.hpp"

using camera_sync::AudioMarker;
using camera_sync::AudioSink;
using camera_sync::CameraRow;
using camera_sync::CameraState;
using camera_sync::Clock;
using camera_sync::FrameRate;
using camera_sync::FrameSink;
using camera_sync::Logger;
using camera_sync::LogLevel;
using camera_sync::PatternParams;
using camera_sync::Session;
using camera_sync::SessionSnapshot;
using camera_sync::SessionState;
using camera_sync::SourceId;
using camera_sync::StartResult;
using camera_sync::Tap;
using camera_sync::TapFactory;
using camera_sync::VideoMarker;

namespace {

// Sync Dock's pattern at q=4 frames and 60 fps, as its QR codes carry it. Half a wrap is 25.344 s.
const PatternParams pattern{884, 2, 66, 256};
constexpr uint64_t one_ms = 1000000;
constexpr uint64_t one_s = 1000000000;

// A clock the test moves by hand. A tap's thread may read it.
struct FakeClock {
	std::atomic<uint64_t> now{0};
	Clock clock()
	{
		return [this] {
			return now.load();
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
	std::vector<std::string> at(LogLevel level) const
	{
		std::vector<std::string> found;
		for (const auto &line : lines)
			if (line.first == level)
				found.push_back(line.second);
		return found;
	}
};

struct FakeFactory;

struct FakeTap : Tap {
	FakeTap(FakeFactory &factory, std::string name) : factory(factory), name(std::move(name)) {}
	~FakeTap() override;
	uint64_t skipped_frames() const override { return 0; }

	FakeFactory &factory;
	std::string name;
};

// Records every tap the session asks for and every tap it destroys, and refuses the sources the test names.
struct FakeFactory : TapFactory {
	std::vector<std::string> requests; // "camera:<id>" and "mic:<id>", in the order asked
	std::multiset<std::string> live;   // the taps made and not yet destroyed
	std::set<SourceId> refuse;         // the sources whose tap cannot be made
	std::map<SourceId, FrameSink *> frame_sinks;
	AudioSink *audio_sink = nullptr;

	// While set, each tap's destructor checks that the session's lock is free.
	Session *session = nullptr;
	int probes = 0;
	std::vector<std::shared_future<void>> stuck;

	std::unique_ptr<Tap> make(const std::string &name, const SourceId &source)
	{
		requests.push_back(name);
		if (refuse.count(source))
			return nullptr;
		live.insert(name);
		return std::make_unique<FakeTap>(*this, name);
	}

	std::unique_ptr<Tap> create_camera_tap(const SourceId &camera, FrameSink &sink) override
	{
		std::unique_ptr<Tap> tap = make("camera:" + camera, camera);
		if (tap)
			frame_sinks[camera] = &sink;
		return tap;
	}

	std::unique_ptr<Tap> create_mic_tap(const SourceId &mic, AudioSink &sink) override
	{
		std::unique_ptr<Tap> tap = make("mic:" + mic, mic);
		if (tap)
			audio_sink = &sink;
		return tap;
	}

	uint32_t mic_sample_rate() override { return 48000; }
};

FakeTap::~FakeTap()
{
	factory.live.erase(factory.live.find(name));
	Session *session = factory.session;
	if (!session)
		return;

	// A delivery in flight as the tap is destroyed: it takes the session's lock, and this destructor waits for it as a
	// real tap's does. Were the session still holding its lock, the delivery could never finish.
	std::promise<void> finished;
	std::shared_future<void> done = finished.get_future().share();
	std::thread delivery([session, finished = std::move(finished)]() mutable {
		session->snapshot();
		finished.set_value();
	});
	factory.probes++;
	if (done.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
		delivery.join();
		return;
	}
	FAIL_CHECK("tap ", name, " destroyed while the session held its lock");
	delivery.detach();
	factory.stuck.push_back(done);
}

struct Harness {
	FakeClock clock;
	Log log;
	FakeFactory factory;
	std::map<SourceId, std::string> names{{"cam-a", "Cam A"},
					      {"cam-b", "Cam B"},
					      {"cam-c", "Cam C"},
					      {"mic", "Shure MV7"},
					      {"mic-2", "Yeti"}};
	Session session;

	Harness() : session(factory, clock.clock(), log.logger(), [this](const SourceId &id) { return names.at(id); })
	{
		factory.session = &session;
	}

	~Harness()
	{
		factory.session = nullptr;
		for (const std::shared_future<void> &done : factory.stuck)
			done.wait();
	}

	SessionSnapshot snap() const { return session.snapshot(); }

	bool has_row(const SourceId &camera) const
	{
		for (const CameraRow &row : snap().cameras)
			if (row.id == camera)
				return true;
		return false;
	}

	CameraRow row(const SourceId &camera) const
	{
		for (const CameraRow &row : snap().cameras)
			if (row.id == camera)
				return row;
		FAIL("no row for camera ", camera);
		return {};
	}

	// One sample on a camera: the camera sees index at audio_ns + offset_ns, then the mic hears it at audio_ns.
	void sample(const SourceId &camera, uint32_t index, uint64_t audio_ns, int64_t offset_ns,
		    const PatternParams &params = pattern)
	{
		session.on_video_marker(camera, video(index, audio_ns + offset_ns, params));
		session.on_audio_marker(audio(index, audio_ns));
	}

	static VideoMarker video(uint32_t index, uint64_t time_ns, const PatternParams &params = pattern)
	{
		VideoMarker marker{};
		marker.timestamp = time_ns;
		marker.qr_data.f = params.f;
		marker.qr_data.c = params.c;
		marker.qr_data.q_ms = params.q_ms;
		marker.qr_data.index = index;
		marker.qr_data.index_max = params.index_max;
		marker.qr_data.valid = true;
		return marker;
	}

	static AudioMarker audio(uint32_t index, uint64_t time_ns)
	{
		return AudioMarker{time_ns, static_cast<int>(index), 0.0f};
	}
};

} // namespace

TEST_CASE("session start measures every ticked camera and the mic through a tap each")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");

	// READ-24: Measuring, with one tap for each ticked camera and one for the mic.
	CHECK(h.session.start({30, 1}) == StartResult::Started);
	CHECK(h.snap().state == SessionState::Measuring);
	CHECK(h.factory.requests == std::vector<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-b").state == CameraState::Looking);
}

TEST_CASE("session start takes the frame tolerance from the canvas frame rate")
{
	// READ-16: F is 1000 x fps_den / fps_num ms. Five samples spanning just under F are Stable, just over it are not.
	FrameRate rate;
	int64_t within_ns = 0, beyond_ns = 0;
	SUBCASE("60 fps, F = 16.667 ms")
	{
		rate = {60, 1};
		within_ns = 16666000;
		beyond_ns = 16667000;
	}
	SUBCASE("59.94 fps, F = 16.683 ms")
	{
		rate = {60000, 1001};
		within_ns = 16683000;
		beyond_ns = 16684000;
	}

	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start(rate) == StartResult::Started);

	for (uint32_t i = 0; i < 5; i++) {
		const uint64_t audio_ns = one_s + i * 200 * one_ms;
		const int64_t spread_ns = i == 4 ? 1 : 0;
		h.session.on_video_marker("cam-a", Harness::video(i, audio_ns + 100 * one_ms + spread_ns * within_ns));
		h.session.on_video_marker("cam-b", Harness::video(i, audio_ns + 100 * one_ms + spread_ns * beyond_ns));
		h.session.on_audio_marker(Harness::audio(i, audio_ns));
	}

	CHECK(h.row("cam-a").state == CameraState::Stable);
	CHECK(h.row("cam-a").reading_ms == 100);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").samples == 5);
}

TEST_CASE("session start zeroes each camera's marker counts")
{
	Harness h;
	h.session.set_selection({"cam-a"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	for (uint32_t index : {3, 4, 7})
		h.session.on_video_marker("cam-a", Harness::video(index, one_s + index * 200 * one_ms));
	REQUIRE(h.row("cam-a").received == 3);
	REQUIRE(h.row("cam-a").missed == 2);

	h.session.stop();
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);

	CHECK(h.row("cam-a").received == 0);
	CHECK(h.row("cam-a").missed == 0);
	// The count starts over: the first marker after Start counts no gap from index 7.
	h.session.on_video_marker("cam-a", Harness::video(9, 3 * one_s));
	CHECK(h.row("cam-a").received == 1);
	CHECK(h.row("cam-a").missed == 0);
}

TEST_CASE("session start without a camera or the mic changes nothing")
{
	Harness h;
	SessionState before = SessionState::Idle;
	size_t requests_before = 0;
	SUBCASE("idle with no camera")
	{
		h.session.set_selection({}, "mic");
	}
	SUBCASE("idle with no mic")
	{
		h.session.set_selection({"cam-a"}, std::nullopt);
	}
	SUBCASE("idle with neither") {}
	SUBCASE("stopped with no camera")
	{
		h.session.set_selection({"cam-a"}, "mic");
		REQUIRE(h.session.start({30, 1}) == StartResult::Started);
		h.session.stop();
		h.session.set_selection({}, "mic");
		before = SessionState::Stopped;
		requests_before = 2;
	}

	// READ-30: no tap is asked for and the state stays.
	CHECK(h.session.start({30, 1}) == StartResult::NothingChosen);
	CHECK(h.snap().state == before);
	CHECK(h.factory.requests.size() == requests_before);
	CHECK(h.factory.live.empty());
}

TEST_CASE("session skips a camera whose tap cannot be made and warns naming it")
{
	Harness h;
	h.factory.refuse = {"cam-a"};
	h.session.set_selection({"cam-a", "cam-b"}, "mic");

	// READ-67: the other camera and the mic are measured, and the camera that has no tap stays Looking.
	CHECK(h.session.start({30, 1}) == StartResult::Started);
	CHECK(h.snap().state == SessionState::Measuring);
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-b"});
	CHECK(h.log.at(LogLevel::Warning) == std::vector<std::string>{"tap.failed camera=\"Cam A\""});

	h.sample("cam-b", 0, one_s, 100 * one_ms);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 100);
	CHECK(h.row("cam-a").state == CameraState::Looking);
}

TEST_CASE("session measures nothing when the mic's tap cannot be made")
{
	Harness h;
	h.factory.refuse = {"mic"};
	h.session.set_selection({"cam-a", "cam-b"}, "mic");

	// READ-68: not Measuring, no tap left, a warning, and the failure in the snapshot for the dock's message.
	CHECK(h.session.start({30, 1}) == StartResult::MicTapFailed);
	CHECK(h.snap().state == SessionState::Idle);
	CHECK(h.factory.live.empty());
	CHECK(h.snap().mic_tap_failed);
	CHECK(h.log.at(LogLevel::Warning) == std::vector<std::string>{"tap.failed mic=\"Shure MV7\""});

	// The next Start that can listen to the mic measures, and the failure is gone.
	h.factory.refuse.clear();
	CHECK(h.session.start({30, 1}) == StartResult::Started);
	CHECK(h.snap().state == SessionState::Measuring);
	CHECK_FALSE(h.snap().mic_tap_failed);
}

TEST_CASE("session collection change releases every tap and forgets the readings and the selection")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.sample("cam-a", 0, one_s, 100 * one_ms);
	REQUIRE(h.row("cam-a").state == CameraState::Settling);

	// READ-28: every tap goes, the selection is empty and the session is Idle.
	h.session.collection_changing();
	CHECK(h.snap().state == SessionState::Idle);
	CHECK(h.factory.live.empty());
	CHECK(h.snap().cameras.empty());
	CHECK_FALSE(h.snap().mic.has_value());

	// The new collection's selection starts with no reading and no pattern: another pattern is followed.
	h.session.set_selection({"cam-a"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
	h.sample("cam-a", 1, 3 * one_s, 40 * one_ms, PatternParams{1000, 2, 66, 256});
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").live_ms == 40);
}

TEST_CASE("session exit releases every tap")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	REQUIRE(h.factory.live.size() == 3);

	// READ-29
	h.session.shutdown();
	CHECK(h.factory.live.empty());
}

TEST_CASE("session stop and reset change nothing where they do not apply")
{
	Harness h;
	h.session.set_selection({"cam-a"}, "mic");

	// READ-31: Stop and Reset in Idle.
	h.session.stop();
	h.session.reset();
	CHECK(h.snap().state == SessionState::Idle);
	CHECK(h.factory.requests.empty());
	CHECK(h.snap().cameras.size() == 1);
	CHECK(h.snap().mic == std::optional<SourceId>("mic"));

	// Stop in Stopped.
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.sample("cam-a", 0, one_s, 100 * one_ms);
	h.session.stop();
	REQUIRE(h.snap().state == SessionState::Stopped);
	h.session.stop();
	CHECK(h.snap().state == SessionState::Stopped);
	CHECK(h.factory.requests.size() == 2);
	CHECK(h.factory.live.empty());
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").live_ms == 100);
}

TEST_CASE("session destroys taps only after releasing its lock")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);

	// Each tap's destructor waits for a delivery that takes the session's lock; it fails the case if that delivery
	// cannot finish.
	SUBCASE("stop")
	{
		h.session.stop();
	}
	SUBCASE("collection change")
	{
		h.session.collection_changing();
	}
	SUBCASE("exit")
	{
		h.session.shutdown();
	}

	CHECK(h.factory.probes == 3);
	CHECK(h.factory.live.empty());
}
