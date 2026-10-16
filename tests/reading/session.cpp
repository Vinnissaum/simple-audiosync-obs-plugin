// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "reading/session.hpp"
#include "support/fixtures.hpp"

using camera_sync::AudioBlock;
using camera_sync::AudioMarker;
using camera_sync::AudioSink;
using camera_sync::CameraRow;
using camera_sync::CameraState;
using camera_sync::Clock;
using camera_sync::FrameRate;
using camera_sync::FrameSink;
using camera_sync::Logger;
using camera_sync::LogLevel;
using camera_sync::MicPipeline;
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
	uint64_t skipped_frames() const override;

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
	std::map<std::string, uint64_t> skipped; // by tap name: the frames each tap reports it skipped

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

uint64_t FakeTap::skipped_frames() const
{
	auto found = factory.skipped.find(name);
	return found == factory.skipped.end() ? 0 : found->second;
}

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

constexpr uint32_t sample_rate = 48000;
constexpr uint32_t block_frames = 480; // 10 ms

// Plays the audio fixture, the sound of the pattern above, into a mic's sink in 10 ms blocks from start_ns.
void hear_clip(AudioSink &sink, uint64_t start_ns)
{
	const std::vector<float> clip = read_audio_clip();
	for (size_t first = 0; first < clip.size(); first += block_frames) {
		const uint32_t frames = static_cast<uint32_t>(std::min<size_t>(block_frames, clip.size() - first));
		sink.on_audio(AudioBlock{{clip.data() + first, nullptr},
					 frames,
					 start_ns + camera_sync::mul_div64(first, 1000000000ULL, sample_rate)});
	}
}

// The audio markers the fixture yields when played from start_ns, heard by a mic pipeline of the test's own.
std::vector<AudioMarker> clip_markers(uint64_t start_ns)
{
	std::vector<AudioMarker> markers;
	MicPipeline mic(nullptr, sample_rate, [&](const AudioMarker &marker) { markers.push_back(marker); });
	mic.set_pattern(pattern.f, pattern.c, pattern.q_ms);
	hear_clip(mic, start_ns);
	REQUIRE(markers.size() == 10);
	return markers;
}

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

TEST_CASE("session adopts the first marker's pattern for the mic and the estimator")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({60, 1}) == StartResult::Started);
	REQUIRE(h.factory.audio_sink);
	const uint64_t start_ns = 10 * one_s;
	const std::vector<AudioMarker> heard = clip_markers(start_ns);

	// Cam A brings the pattern. A later marker of another pattern, on cam B, does not replace it.
	h.session.on_video_marker("cam-a", Harness::video(heard[0].index, heard[0].timestamp + 100 * one_ms));
	h.session.on_video_marker("cam-b", Harness::video(heard[1].index, heard[1].timestamp + 40 * one_ms,
							  PatternParams{1000, 2, 66, 256}));
	hear_clip(*h.factory.audio_sink, start_ns);

	// READ-17: the mic listened for that pattern's sound (f 884, c 2, q 66), so cam A's marker paired at +100 ms.
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").samples == 1);
	CHECK(h.row("cam-a").live_ms == 100);
	CHECK(h.row("cam-b").state == CameraState::Looking);
}

TEST_CASE("session gives the estimator the wrap of the adopted pattern")
{
	// A wrap of 16 indexes at q = 66 ms: half a wrap is 16 x 3 x 66 / 2 = 1,584 ms.
	const PatternParams short_wrap{884, 2, 66, 16};
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({60, 1}) == StartResult::Started);

	// READ-17: indexes 14 then 1 skip 15 and 0 when they wrap at 16.
	h.session.on_video_marker("cam-a", Harness::video(14, one_s, short_wrap));
	h.session.on_video_marker("cam-a", Harness::video(1, one_s + 600 * one_ms, short_wrap));
	CHECK(h.row("cam-a").missed == 2);

	// Markers half a wrap apart do not pair; one millisecond less does.
	h.sample("cam-b", 5, 10 * one_s, 1584 * one_ms, short_wrap);
	CHECK(h.row("cam-b").state == CameraState::Looking);
	h.sample("cam-b", 6, 20 * one_s, 1583 * one_ms, short_wrap);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 1583);
}

TEST_CASE("session ignores the markers of a pattern other than its own")
{
	PatternParams other = pattern;
	SUBCASE("another f")
	{
		other.f = 1000;
	}
	SUBCASE("another c")
	{
		other.c = 3;
	}
	SUBCASE("another q")
	{
		other.q_ms = 67;
	}
	SUBCASE("another index_max")
	{
		other.index_max = 128;
	}

	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.sample("cam-a", 0, one_s, 100 * one_ms);

	// READ-18: the other pattern's marker forms no sample and is not counted.
	h.sample("cam-b", 1, 2 * one_s, 40 * one_ms, other);
	CHECK(h.row("cam-b").state == CameraState::Looking);
	CHECK(h.row("cam-b").samples_formed == 0);
	CHECK(h.row("cam-b").received == 0);

	// The same camera's marker of the session's pattern is read.
	h.sample("cam-b", 2, 3 * one_s, 40 * one_ms);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);
}

TEST_CASE("session stop keeps every reading and releases every tap")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms);
	h.sample("cam-b", 20, one_s, 40 * one_ms);
	h.sample("cam-b", 21, one_s + 200 * one_ms, 45 * one_ms);
	REQUIRE(h.row("cam-a").state == CameraState::Stable);

	// READ-25
	h.session.stop();
	CHECK(h.snap().state == SessionState::Stopped);
	CHECK(h.factory.live.empty());
	CHECK(h.row("cam-a").state == CameraState::Stable);
	CHECK(h.row("cam-a").reading_ms == 100);
	CHECK(h.row("cam-a").samples == 5);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 45);
	CHECK(h.row("cam-b").samples == 2);
}

TEST_CASE("session start from stopped keeps every reading and listens for the kept pattern")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms);
	h.sample("cam-b", 20, one_s, 40 * one_ms);
	h.sample("cam-b", 21, one_s + 200 * one_ms, 45 * one_ms);
	h.session.stop();

	// READ-26: the taps are asked for again, and every reading and state is as Stop left it.
	CHECK(h.session.start({30, 1}) == StartResult::Started);
	CHECK(h.snap().state == SessionState::Measuring);
	CHECK(h.factory.requests.size() == 6);
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
	CHECK(h.row("cam-a").state == CameraState::Stable);
	CHECK(h.row("cam-a").reading_ms == 100);
	CHECK(h.row("cam-a").samples == 5);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 45);
	CHECK(h.row("cam-b").samples == 2);

	// READ-25: the pattern was kept. Another one is still ignored, and the new mic tap listens for the kept one.
	h.sample("cam-b", 30, 5 * one_s, 10 * one_ms, PatternParams{1000, 2, 66, 256});
	CHECK(h.row("cam-b").samples == 2);
	REQUIRE(h.factory.audio_sink);
	const uint64_t start_ns = 10 * one_s;
	const std::vector<AudioMarker> heard = clip_markers(start_ns);
	h.session.on_video_marker("cam-b", Harness::video(heard[0].index, heard[0].timestamp + 50 * one_ms));
	hear_clip(*h.factory.audio_sink, start_ns);
	CHECK(h.row("cam-b").samples == 3);
	CHECK(h.row("cam-b").live_ms == 50);
}

TEST_CASE("session reset clears every reading and marker and reports each camera Looking")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms);
	h.sample("cam-b", 20, one_s, 40 * one_ms);
	h.session.on_audio_marker(Harness::audio(30, 5 * one_s));
	REQUIRE(h.row("cam-a").state == CameraState::Stable);

	SessionState state = SessionState::Measuring;
	size_t live = 3;
	SUBCASE("while measuring") {}
	SUBCASE("while stopped")
	{
		h.session.stop();
		state = SessionState::Stopped;
		live = 0;
	}

	// READ-27: every camera Looking with no sample. The session stays where it was.
	h.session.reset();
	CHECK(h.snap().state == state);
	CHECK(h.factory.live.size() == live);
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
	CHECK(h.row("cam-b").state == CameraState::Looking);
	CHECK(h.row("cam-b").samples == 0);

	// The audio marker went too: index 30 finds nothing to pair with.
	if (state == SessionState::Stopped)
		REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.session.on_video_marker("cam-a", Harness::video(30, 5 * one_s + 100 * one_ms));
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples_formed == 0);
}

TEST_CASE("session follows another pattern after reset")
{
	Harness h;
	h.session.set_selection({"cam-a"}, "mic");
	REQUIRE(h.session.start({60, 1}) == StartResult::Started);
	h.sample("cam-a", 0, one_s, 100 * one_ms, PatternParams{1000, 2, 66, 256});
	REQUIRE(h.row("cam-a").state == CameraState::Settling);

	h.session.reset();

	// READ-19: until a camera reads a pattern again, the session takes no audio marker, so one the mic decoded
	// under the cleared pattern cannot pair.
	h.session.on_audio_marker(Harness::audio(50, 2 * one_s));
	h.session.on_video_marker("cam-a", Harness::video(50, 2 * one_s + 100 * one_ms));
	CHECK(h.row("cam-a").state == CameraState::Looking);

	// READ-27: that marker's pattern, the fixture's, is the session's now, and the mic listens for its sound.
	REQUIRE(h.factory.audio_sink);
	const uint64_t start_ns = 10 * one_s;
	const std::vector<AudioMarker> heard = clip_markers(start_ns);
	h.session.on_video_marker("cam-a", Harness::video(heard[0].index, heard[0].timestamp + 100 * one_ms));
	hear_clip(*h.factory.audio_sink, start_ns);
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").samples == 1);
	CHECK(h.row("cam-a").live_ms == 100);
}

TEST_CASE("session keeps the pattern after the camera that brought it leaves, until reset")
{
	const PatternParams other{1000, 2, 66, 256};
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.sample("cam-a", 0, one_s, 100 * one_ms);
	h.session.set_selection({"cam-b"}, "mic");
	REQUIRE_FALSE(h.has_row("cam-a"));

	// Edge case: the other cameras keep measuring against the pattern cam A brought.
	h.sample("cam-b", 1, 2 * one_s, 40 * one_ms, other);
	CHECK(h.row("cam-b").state == CameraState::Looking);
	h.sample("cam-b", 2, 3 * one_s, 40 * one_ms);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);

	// Reset forgets it.
	h.session.reset();
	h.sample("cam-b", 3, 4 * one_s, 60 * one_ms, other);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").samples == 1);
	CHECK(h.row("cam-b").live_ms == 60);
}

TEST_CASE("session measures a camera ticked while measuring at once")
{
	Harness h;
	h.session.set_selection({"cam-a"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.sample("cam-a", 0, one_s, 100 * one_ms);

	// READ-32: one tap for the new camera, which starts Looking; nothing else is asked for again.
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	CHECK(h.factory.requests == std::vector<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
	CHECK(h.row("cam-b").state == CameraState::Looking);
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").live_ms == 100);
	h.sample("cam-b", 1, 2 * one_s, 40 * one_ms);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);
}

TEST_CASE("session adds a camera ticked while stopped to the selection only")
{
	Harness h;
	h.session.set_selection({"cam-a"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.session.stop();

	// READ-32: no tap while stopped; the camera is ticked, Looking, and measured at the next Start.
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	CHECK(h.factory.requests.size() == 2);
	CHECK(h.factory.live.empty());
	CHECK(h.row("cam-b").state == CameraState::Looking);
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
}

TEST_CASE("session releases an unticked camera's tap and reading and leaves the others")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms);
	h.sample("cam-b", 20, one_s, 40 * one_ms);
	REQUIRE(h.row("cam-a").state == CameraState::Stable);

	std::multiset<std::string> live;
	SUBCASE("while measuring")
	{
		live = {"mic:mic", "camera:cam-b"};
	}
	SUBCASE("while stopped")
	{
		h.session.stop();
	}

	// READ-33: cam A's tap and reading go; cam B's tap and reading and the mic's tap stay, none asked for again.
	h.session.set_selection({"cam-b"}, "mic");
	CHECK_FALSE(h.has_row("cam-a"));
	CHECK(h.factory.live == live);
	CHECK(h.factory.requests.size() == 3);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);

	// The reading was dropped, not hidden: ticked again, cam A starts over.
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
}

TEST_CASE("session replaces the mic's tap and clears every reading and the pattern when another mic is chosen")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({60, 1}) == StartResult::Started);
	const PatternParams first{1000, 2, 66, 256};
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms, first);
	h.sample("cam-b", 20, one_s, 40 * one_ms, first);
	REQUIRE(h.row("cam-a").state == CameraState::Stable);

	// READ-34: the new mic's tap replaces the old one, the cameras keep theirs, and every camera is Looking.
	h.session.set_selection({"cam-a", "cam-b"}, "mic-2");
	CHECK(h.snap().state == SessionState::Measuring);
	CHECK(h.factory.requests == std::vector<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b", "mic:mic-2"});
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic-2", "camera:cam-a", "camera:cam-b"});
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
	CHECK(h.row("cam-b").state == CameraState::Looking);
	CHECK(h.row("cam-b").samples == 0);

	// The pattern went too: the fixture's pattern is followed now, and the new mic listens for its sound.
	REQUIRE(h.factory.audio_sink);
	const uint64_t start_ns = 10 * one_s;
	const std::vector<AudioMarker> heard = clip_markers(start_ns);
	h.session.on_video_marker("cam-a", Harness::video(heard[0].index, heard[0].timestamp + 100 * one_ms));
	hear_clip(*h.factory.audio_sink, start_ns);
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").live_ms == 100);
}

TEST_CASE("session clears every reading and the pattern when another mic is chosen while stopped")
{
	Harness h;
	h.session.set_selection({"cam-a"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms);
	h.session.stop();

	// READ-34: no tap while stopped, and no reading kept.
	h.session.set_selection({"cam-a"}, "mic-2");
	CHECK(h.snap().state == SessionState::Stopped);
	CHECK(h.factory.requests.size() == 2);
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);

	// The next Start listens to the new mic and follows whatever pattern comes first.
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic-2", "camera:cam-a"});
	h.sample("cam-a", 1, 3 * one_s, 40 * one_ms, PatternParams{1000, 2, 66, 256});
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").live_ms == 40);
}

TEST_CASE("session stops measuring when a change leaves it no mic to listen to")
{
	Harness h;
	h.factory.refuse = {"mic-2"};
	h.session.set_selection({"cam-a"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);

	SUBCASE("the new mic's tap cannot be made")
	{
		// READ-34 with READ-68: a warning, and the failure for the dock's message.
		h.session.set_selection({"cam-a"}, "mic-2");
		CHECK(h.snap().mic_tap_failed);
		CHECK(h.snap().mic == std::optional<SourceId>("mic-2"));
		CHECK(h.log.at(LogLevel::Warning) == std::vector<std::string>{"tap.failed mic=\"Yeti\""});
	}
	SUBCASE("no mic is chosen")
	{
		h.session.set_selection({"cam-a"}, std::nullopt);
		CHECK_FALSE(h.snap().mic_tap_failed);
		CHECK(h.log.at(LogLevel::Warning).empty());
	}

	// Measuring ends as it does when the mic is removed: every tap released, and Idle.
	CHECK(h.snap().state == SessionState::Idle);
	CHECK(h.factory.live.empty());
}

TEST_CASE("session forgets every reading and the pattern and goes Idle when the mic is removed")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms);
	REQUIRE(h.row("cam-a").state == CameraState::Stable);

	SUBCASE("while measuring") {}
	SUBCASE("while stopped")
	{
		h.session.stop();
	}

	// READ-35: every tap goes, no reading is kept, and the session is Idle with no mic. The cameras stay ticked.
	h.session.on_mic_removed();
	CHECK(h.snap().state == SessionState::Idle);
	CHECK(h.factory.live.empty());
	CHECK_FALSE(h.snap().mic.has_value());
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
	CHECK(h.has_row("cam-b"));

	// The pattern went too: with a mic chosen again, another pattern is followed.
	h.session.set_selection({"cam-a", "cam-b"}, "mic-2");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.sample("cam-a", 1, 40 * one_s, 40 * one_ms, PatternParams{1000, 2, 66, 256});
	CHECK(h.row("cam-a").state == CameraState::Settling);
	CHECK(h.row("cam-a").live_ms == 40);
}

namespace {

// Starts measuring cam A against the mic with the clock at 0.
void start_measuring(Harness &h)
{
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
}

// Cam A sees a marker every 200 ms from from_ms to to_ms, the clock reading each one's time.
void see_markers(Harness &h, uint64_t from_ms, uint64_t to_ms)
{
	for (uint64_t t = from_ms; t <= to_ms; t += 200) {
		h.clock.now = t * one_ms;
		h.session.on_video_marker("cam-a", Harness::video(static_cast<uint32_t>(t / 200 % 256), t * one_ms));
	}
}

bool no_sound_at(Harness &h, uint64_t ms)
{
	h.clock.now = ms * one_ms;
	return h.snap().no_pattern_sound;
}

} // namespace

TEST_CASE("session reports no pattern sound 5 s after the first video marker the mic did not answer")
{
	Harness h;
	start_measuring(h);

	// READ-42: video markers from t = 0 and no audio marker.
	see_markers(h, 0, 4800);
	CHECK_FALSE(no_sound_at(h, 4999));
	CHECK(no_sound_at(h, 5000));
	CHECK(h.row("cam-a").state == CameraState::Looking);
}

TEST_CASE("session clears no pattern sound when the mic hears a marker and waits again")
{
	Harness h;
	start_measuring(h);
	see_markers(h, 0, 4800);
	REQUIRE(no_sound_at(h, 5000));

	// READ-43: an audio marker clears it, and the wait starts again at the next video marker, at 5.2 s.
	h.session.on_audio_marker(Harness::audio(100, 5 * one_s));
	CHECK_FALSE(h.snap().no_pattern_sound);
	see_markers(h, 5200, 10000);
	CHECK_FALSE(no_sound_at(h, 10199));
	CHECK(no_sound_at(h, 10200));
}

TEST_CASE("session clears no pattern sound when the video markers stop for 5 s")
{
	Harness h;
	start_measuring(h);
	see_markers(h, 0, 4800);
	REQUIRE(no_sound_at(h, 5000));

	// READ-43: the last video marker was at 4.8 s.
	CHECK(no_sound_at(h, 9799));
	CHECK_FALSE(no_sound_at(h, 9800));

	// Markers that come back start a new wait.
	see_markers(h, 12000, 16800);
	CHECK_FALSE(no_sound_at(h, 16999));
	CHECK(no_sound_at(h, 17000));
}

TEST_CASE("session clears no pattern sound at stop, reset, start and another mic")
{
	Harness h;
	start_measuring(h);
	see_markers(h, 0, 4800);
	REQUIRE(no_sound_at(h, 5000));

	// READ-43
	SUBCASE("stop, then start")
	{
		h.session.stop();
		CHECK_FALSE(h.snap().no_pattern_sound);
		REQUIRE(h.session.start({30, 1}) == StartResult::Started);
		CHECK_FALSE(h.snap().no_pattern_sound);
	}
	SUBCASE("start after the collection changed")
	{
		h.session.collection_changing();
		h.session.set_selection({"cam-a", "cam-b"}, "mic");
		REQUIRE(h.session.start({30, 1}) == StartResult::Started);
		CHECK_FALSE(h.snap().no_pattern_sound);
	}
	SUBCASE("reset")
	{
		h.session.reset();
		CHECK(h.snap().state == SessionState::Measuring);
		CHECK_FALSE(h.snap().no_pattern_sound);
	}
	SUBCASE("another mic")
	{
		h.session.set_selection({"cam-a", "cam-b"}, "mic-2");
		CHECK(h.snap().state == SessionState::Measuring);
		CHECK_FALSE(h.snap().no_pattern_sound);
	}

	// The wait starts again at the next video marker.
	see_markers(h, 6000, 10800);
	CHECK_FALSE(no_sound_at(h, 10999));
	CHECK(no_sound_at(h, 11000));
}

TEST_CASE("session never reports no pattern sound while stopped or idle")
{
	Harness h;
	SessionState state = SessionState::Idle;
	SUBCASE("idle")
	{
		h.session.set_selection({"cam-a", "cam-b"}, "mic");
	}
	SUBCASE("stopped")
	{
		start_measuring(h);
		see_markers(h, 0, 4800);
		h.session.stop();
		state = SessionState::Stopped;
	}
	SUBCASE("idle after the collection changed")
	{
		start_measuring(h);
		see_markers(h, 0, 4800);
		h.session.collection_changing();
		h.session.set_selection({"cam-a", "cam-b"}, "mic");
	}
	SUBCASE("idle after the mic was removed")
	{
		start_measuring(h);
		see_markers(h, 0, 4800);
		h.session.on_mic_removed();
	}

	// Markers a tap delivers late, after measuring ended, change nothing.
	see_markers(h, 5000, 9800);
	REQUIRE(h.snap().state == state);
	CHECK_FALSE(no_sound_at(h, 5000));
	CHECK_FALSE(no_sound_at(h, 9900));
}

TEST_CASE("session reports no pattern sound for a mic that was heard and then muted")
{
	Harness h;
	start_measuring(h);

	// The mic hears the marker at t = 0, then nothing while the video markers go on.
	h.session.on_video_marker("cam-a", Harness::video(0, 0));
	h.session.on_audio_marker(Harness::audio(0, 0));
	REQUIRE(h.row("cam-a").state == CameraState::Settling);
	see_markers(h, 200, 5200);

	// 5 s after the first video marker that followed the audio marker, the one at 0.2 s.
	CHECK_FALSE(no_sound_at(h, 5199));
	CHECK(no_sound_at(h, 5200));
}

namespace {

// The camera's tap delivers one grey frame, with no pattern in it, as the clock reads ms.
void deliver_frame(Harness &h, const SourceId &camera, uint64_t ms)
{
	static const std::vector<uint8_t> grey(320 * 180, 127);
	h.clock.now = ms * one_ms;
	REQUIRE(h.factory.frame_sinks.count(camera) == 1);
	h.factory.frame_sinks[camera]->on_frame(camera_sync::VideoFrame{grey.data(), 320, ms * one_ms}, 320, 180,
						camera_sync::LumaFormat::Luma8);
}

void tick_at(Harness &h, uint64_t ms)
{
	h.clock.now = ms * one_ms;
	h.session.tick();
}

// Cam A Stable at +100 ms and cam B Settling at +40 ms.
void read_both(Harness &h)
{
	for (uint32_t i = 0; i < 5; i++)
		h.sample("cam-a", 20 + i, one_s + i * 200 * one_ms, 100 * one_ms);
	h.sample("cam-b", 20, one_s, 40 * one_ms);
	REQUIRE(h.row("cam-a").state == CameraState::Stable);
	REQUIRE(h.row("cam-b").state == CameraState::Settling);
}

} // namespace

TEST_CASE("session drops a removed camera's tap and reading and unticks it")
{
	Harness h;
	start_measuring(h);
	read_both(h);

	std::multiset<std::string> live;
	SUBCASE("while measuring")
	{
		live = {"mic:mic", "camera:cam-b"};
	}
	SUBCASE("while stopped")
	{
		h.session.stop();
	}

	// READ-55: cam A is no longer ticked; cam B keeps its tap and reading.
	h.session.on_camera_removed("cam-a");
	CHECK_FALSE(h.has_row("cam-a"));
	CHECK(h.factory.live == live);
	CHECK(h.factory.requests.size() == 3);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);
}

TEST_CASE("session keeps the pattern when the camera that brought it is removed")
{
	Harness h;
	start_measuring(h);
	h.sample("cam-a", 0, one_s, 100 * one_ms);

	// Edge case: cam B still measures against the pattern cam A brought, until Reset.
	h.session.on_camera_removed("cam-a");
	h.sample("cam-b", 1, 2 * one_s, 40 * one_ms, PatternParams{1000, 2, 66, 256});
	CHECK(h.row("cam-b").state == CameraState::Looking);
	h.sample("cam-b", 2, 3 * one_s, 40 * one_ms);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);
}

TEST_CASE("session drops a renamed camera's tap and reading and unticks it")
{
	Harness h;
	start_measuring(h);
	read_both(h);

	// READ-56
	h.session.on_camera_renamed("cam-a");
	CHECK_FALSE(h.has_row("cam-a"));
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-b"});
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);

	// Ticked again under its new name, it starts over.
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
}

TEST_CASE("session drops a camera that delivers no frame for 2 s")
{
	Harness h;
	start_measuring(h);
	h.sample("cam-a", 0, 0, 100 * one_ms);
	deliver_frame(h, "cam-b", 1000);
	deliver_frame(h, "cam-b", 1900);

	// READ-57: cam A's tap has delivered nothing since it was made at t = 0.
	tick_at(h, 1999);
	CHECK(h.has_row("cam-a"));
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
	tick_at(h, 2000);
	CHECK_FALSE(h.has_row("cam-a"));
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-b"});

	// Cam B's last frame was at 1.9 s.
	CHECK(h.has_row("cam-b"));
	tick_at(h, 3899);
	CHECK(h.has_row("cam-b"));
	tick_at(h, 3900);
	CHECK_FALSE(h.has_row("cam-b"));
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic"});
	CHECK(h.snap().state == SessionState::Measuring);
}

TEST_CASE("session keeps a camera that delivers frames without the pattern")
{
	Harness h;
	start_measuring(h);

	// READ-57: frames flow, so the camera is measured, Looking, however long it sees no pattern.
	for (uint64_t ms = 0; ms <= 10000; ms += 100) {
		if (ms % 500 == 0)
			deliver_frame(h, "cam-b", ms);
		tick_at(h, ms);
	}
	CHECK(h.has_row("cam-b"));
	CHECK(h.row("cam-b").state == CameraState::Looking);
	CHECK(h.factory.live.count("camera:cam-b") == 1);
}

TEST_CASE("session tick changes nothing while stopped or idle")
{
	Harness h;
	SessionState state = SessionState::Idle;
	SUBCASE("idle")
	{
		h.session.set_selection({"cam-a", "cam-b"}, "mic");
	}
	SUBCASE("stopped")
	{
		start_measuring(h);
		h.sample("cam-a", 0, one_s, 100 * one_ms);
		h.session.stop();
		state = SessionState::Stopped;
	}

	tick_at(h, 10000);
	CHECK(h.snap().state == state);
	CHECK(h.has_row("cam-a"));
	CHECK(h.has_row("cam-b"));
	if (state == SessionState::Stopped)
		CHECK(h.row("cam-a").live_ms == 100);
}

TEST_CASE("session clears a camera's reading when its filters change")
{
	Harness h;
	start_measuring(h);
	read_both(h);

	std::multiset<std::string> live;
	SUBCASE("while measuring")
	{
		live = {"mic:mic", "camera:cam-a", "camera:cam-b"};
	}
	SUBCASE("while stopped")
	{
		h.session.stop();
	}

	// READ-58: cam A is Looking again and still ticked, with its tap; cam B keeps its reading.
	h.session.on_camera_filters_changed("cam-a");
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
	CHECK(h.factory.live == live);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 40);
}

TEST_CASE("session clears every reading when the mic's sync offset changes and keeps the pattern")
{
	Harness h;
	start_measuring(h);
	read_both(h);
	h.session.on_audio_marker(Harness::audio(40, 5 * one_s));

	// READ-59: every camera Looking; measuring goes on with every tap.
	h.session.on_mic_sync_offset_changed();
	CHECK(h.snap().state == SessionState::Measuring);
	CHECK(h.factory.live == std::multiset<std::string>{"mic:mic", "camera:cam-a", "camera:cam-b"});
	CHECK(h.row("cam-a").state == CameraState::Looking);
	CHECK(h.row("cam-a").samples == 0);
	CHECK(h.row("cam-b").state == CameraState::Looking);
	CHECK(h.row("cam-b").samples == 0);

	// An audio marker stamped with the old offset forms no sample.
	h.session.on_video_marker("cam-a", Harness::video(40, 5 * one_s + 100 * one_ms));
	CHECK(h.row("cam-a").state == CameraState::Looking);

	// The pattern stays: another one is still ignored, the session's own is read.
	h.sample("cam-b", 41, 6 * one_s, 30 * one_ms, PatternParams{1000, 2, 66, 256});
	CHECK(h.row("cam-b").state == CameraState::Looking);
	h.sample("cam-b", 42, 7 * one_s, 30 * one_ms);
	CHECK(h.row("cam-b").state == CameraState::Settling);
	CHECK(h.row("cam-b").live_ms == 30);
}

namespace {

std::vector<std::string> lines_at(const Harness &h, LogLevel level, const std::string &prefix)
{
	std::vector<std::string> found;
	for (const std::string &line : h.log.at(level))
		if (line.rfind(prefix, 0) == 0)
			found.push_back(line);
	return found;
}

} // namespace

TEST_CASE("session logs measure.start with the ticked cameras, the mic and the canvas rate")
{
	Harness h;
	h.factory.refuse = {"mic"};
	h.session.set_selection({"cam-a", "cam-b", "cam-c"}, "mic");

	// A Start that measures nothing logs no start.
	REQUIRE(h.session.start({60000, 1001}) == StartResult::MicTapFailed);
	CHECK(h.log.at(LogLevel::Info).empty());

	// READ-84
	h.factory.refuse.clear();
	REQUIRE(h.session.start({60000, 1001}) == StartResult::Started);
	CHECK(h.log.at(LogLevel::Info) ==
	      std::vector<std::string>{"measure.start cameras=3 mic=\"Shure MV7\" canvas_fps=59.94"});
}

TEST_CASE("session logs measure.stop and measure.reset when they apply")
{
	Harness h;
	h.session.set_selection({"cam-a"}, "mic");
	h.session.stop();
	h.session.reset();
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);
	h.session.reset();
	h.session.stop();
	h.session.stop();
	h.session.reset();

	// READ-85: a Reset while measuring and one while stopped, one Stop; Stop and Reset in Idle and a second Stop log
	// nothing.
	CHECK(h.log.at(LogLevel::Info) ==
	      std::vector<std::string>{"measure.start cameras=1 mic=\"Shure MV7\" canvas_fps=30", "measure.reset",
				       "measure.stop", "measure.reset"});
}

TEST_CASE("session logs reading.stable when a camera becomes stable or its reading changes")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({60, 1}) == StartResult::Started);

	// A camera reads the pattern first, so the mic's markers are taken.
	h.session.on_video_marker("cam-a", Harness::video(200, 0));

	// Cam A: each video marker completes its pair, so the estimator answers the video marker.
	uint32_t index = 0;
	for (int64_t offset_ms : {99, 100, 100, 101, 101}) {
		h.session.on_audio_marker(Harness::audio(index, one_s + index * 200 * one_ms));
		h.session.on_video_marker("cam-a",
					  Harness::video(index, one_s + index * 200 * one_ms + offset_ms * one_ms));
		index++;
	}
	// Cam B: each audio marker completes its pair, so the estimator answers the audio marker.
	for (int64_t offset_ms : {-60, -60, -61, -59, -60}) {
		h.sample("cam-b", index, one_s + index * 200 * one_ms, offset_ms * static_cast<int64_t>(one_ms));
		index++;
	}
	// Cam A moves to +130: one line once its window agrees again, none while it stays there.
	for (int i = 0; i < 6; i++) {
		h.sample("cam-a", index, one_s + index * 200 * one_ms, 130 * one_ms);
		index++;
	}

	// READ-86: the spread is the window's largest sample minus its smallest.
	CHECK(lines_at(h, LogLevel::Info, "reading.stable") ==
	      std::vector<std::string>{"reading.stable camera=\"Cam A\" reading_ms=+100 spread_ms=2",
				       "reading.stable camera=\"Cam B\" reading_ms=-60 spread_ms=2",
				       "reading.stable camera=\"Cam A\" reading_ms=+130 spread_ms=0"});
}

TEST_CASE("session logs every marker at debug level with its source, index, time and pattern")
{
	Harness h;
	h.session.set_selection({"cam-a", "cam-b"}, "mic");
	REQUIRE(h.session.start({30, 1}) == StartResult::Started);

	h.session.on_video_marker("cam-a", Harness::video(3, 1100 * one_ms));
	h.session.on_audio_marker(Harness::audio(3, 1000 * one_ms));
	h.session.on_video_marker("cam-b", Harness::video(4, 1300 * one_ms, PatternParams{1000, 2, 66, 256}));
	h.session.reset();
	h.session.on_audio_marker(Harness::audio(5, 1400 * one_ms));

	// READ-87: the markers read, the one of another pattern, and the one heard with no pattern to listen for.
	CHECK(lines_at(h, LogLevel::Debug, "marker.") ==
	      std::vector<std::string>{
		      "marker.video camera=\"Cam A\" index=3 time_ns=1100000000 f=884 c=2 q_ms=66 index_max=256",
		      "marker.audio mic=\"Shure MV7\" index=3 time_ns=1000000000",
		      "marker.ignored camera=\"Cam B\" index=4 time_ns=1300000000 f=1000 c=2 q_ms=66 index_max=256",
		      "marker.ignored mic=\"Shure MV7\" index=5 time_ns=1400000000"});

	// A rejected payload is logged at debug level by the camera's detector ("qr payload logs a rejected value at
	// debug level" in tests/detector/video-detector.cpp) through the logger the session gives each pipeline, which
	// is the session's: the pipeline's own warning arrives in it.
	REQUIRE(h.factory.frame_sinks.count("cam-a") == 1);
	h.factory.frame_sinks["cam-a"]->on_frame(camera_sync::VideoFrame{nullptr, 320, 0}, 320, 180,
						 camera_sync::LumaFormat::Luma8);
	CHECK(h.log.at(LogLevel::Warning) ==
	      std::vector<std::string>{"camera frame dropped: width=320 height=180 no data"});
}

TEST_CASE("session logs a camera's skipped frames at debug level at most once per 5 s")
{
	Harness h;
	start_measuring(h);

	// Both cameras deliver frames, so neither is dropped; only cam A skips any.
	const auto at = [&h](uint64_t ms) {
		deliver_frame(h, "cam-a", ms);
		deliver_frame(h, "cam-b", ms);
		tick_at(h, ms);
	};
	h.factory.skipped["camera:cam-a"] = 3;
	at(1000);
	h.factory.skipped["camera:cam-a"] = 5;
	at(2000);
	h.factory.skipped["camera:cam-a"] = 7;
	at(4000);
	at(5999);
	at(6000);
	at(11000);
	at(12000);

	// READ-87: the first count at once, the count 5 s after it, and nothing once the count stops growing.
	CHECK(lines_at(h, LogLevel::Debug, "frames.") ==
	      std::vector<std::string>{"frames.skipped camera=\"Cam A\" skipped=3",
				       "frames.skipped camera=\"Cam A\" skipped=7"});
}

TEST_CASE("session logs only the allowed formats at info and warning level, naming only the sources it was given")
{
	Harness h;
	h.names["cam-b"] = "Desk \"Cam\" \\ 2";
	h.factory.refuse = {"cam-c"};
	h.session.set_selection({"cam-a", "cam-b", "cam-c"}, "mic");
	REQUIRE(h.session.start({60000, 1001}) == StartResult::Started);
	for (uint32_t i = 0; i < 5; i++) {
		h.sample("cam-a", i, one_s + i * 200 * one_ms, 100 * one_ms);
		h.session.on_audio_marker(Harness::audio(10 + i, 2 * one_s + i * 200 * one_ms));
		h.session.on_video_marker("cam-b", Harness::video(10 + i, 2 * one_s + i * 200 * one_ms + 40 * one_ms));
	}
	h.sample("cam-b", 30, 4 * one_s, 10 * one_ms, PatternParams{1000, 2, 66, 256});
	h.factory.frame_sinks["cam-a"]->on_frame(camera_sync::VideoFrame{nullptr, 0, 0}, 0, 0,
						 camera_sync::LumaFormat::Luma8);
	h.factory.skipped["camera:cam-a"] = 2;
	deliver_frame(h, "cam-a", 500);
	deliver_frame(h, "cam-b", 500);
	tick_at(h, 500);
	h.session.stop();
	h.session.reset();
	h.factory.refuse.insert("mic-2");
	h.session.set_selection({"cam-a", "cam-b", "cam-c"}, "mic-2");
	REQUIRE(h.session.start({60000, 1001}) == StartResult::MicTapFailed);
	h.session.collection_changing();

	// READ-89: every Info and Warning line is one of these formats, and each format was written.
	const std::string name = R"("(?:[^"\\]|\\.)*")";
	const std::vector<std::regex> allowed = {
		std::regex("measure\\.start cameras=[0-9]+ mic=" + name + " canvas_fps=[0-9]+(\\.[0-9]{1,2})?"),
		std::regex("measure\\.stop"),
		std::regex("measure\\.reset"),
		std::regex("reading\\.stable camera=" + name + " reading_ms=[+-]?[0-9]+ spread_ms=[0-9]+"),
		std::regex("tap\\.failed (camera|mic)=" + name),
		std::regex("camera frame dropped: width=[0-9]+ height=[0-9]+( no data)?"),
	};
	std::vector<int> written(allowed.size(), 0);
	for (const auto &line : h.log.lines) {
		if (line.first != LogLevel::Info && line.first != LogLevel::Warning)
			continue;
		bool matched = false;
		for (size_t i = 0; i < allowed.size(); i++)
			if (std::regex_match(line.second, allowed[i])) {
				matched = true;
				written[i]++;
			}
		CHECK_MESSAGE(matched, line.second);
	}
	CHECK(written == std::vector<int>{1, 1, 1, 2, 2, 1});

	// No line, at any level, names a source the session was not given.
	std::set<std::string> given;
	for (const auto &entry : h.names)
		given.insert(entry.second);
	const std::regex quoted(name);
	int names_seen = 0;
	for (const auto &line : h.log.lines)
		for (auto it = std::sregex_iterator(line.second.begin(), line.second.end(), quoted);
		     it != std::sregex_iterator(); ++it) {
			std::string text;
			const std::string found = it->str();
			for (size_t i = 1; i + 1 < found.size(); i++) {
				if (found[i] == '\\')
					i++;
				text += found[i];
			}
			CHECK_MESSAGE(given.count(text) == 1, line.second);
			names_seen++;
		}
	CHECK(names_seen > 0);
	CHECK(lines_at(h, LogLevel::Info, "reading.stable") ==
	      std::vector<std::string>{"reading.stable camera=\"Cam A\" reading_ms=+100 spread_ms=0",
				       "reading.stable camera=\"Desk \\\"Cam\\\" \\\\ 2\" reading_ms=+40 spread_ms=0"});
}
