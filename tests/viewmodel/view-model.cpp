// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "viewmodel/view-model.hpp"

using camera_sync::build_view;
using camera_sync::CameraRow;
using camera_sync::CameraRowView;
using camera_sync::CameraState;
using camera_sync::Choice;
using camera_sync::DockView;
using camera_sync::format_signed_ms;
using camera_sync::SessionSnapshot;
using camera_sync::SessionState;
using camera_sync::SourceInfo;

namespace {

// The en-US text the spec writes out, so a row is compared with the words the user reads.
std::string en_us(const char *key)
{
	static const std::map<std::string, std::string> text = {
		{"CameraSync.Row.Looking", "Looking for the pattern"},
		{"CameraSync.Row.Settling", "{value} ms - settling"},
		{"CameraSync.Row.VideoLate", "{value} ms - video late"},
		{"CameraSync.Row.VideoEarly", "{value} ms - video early"},
		{"CameraSync.Row.InSync", "0 ms - in sync"},
		{"CameraSync.Row.Details", "received {n}, missed {m}"},
		{"CameraSync.Message.Pick", "Pick at least one camera and the mic"},
		{"CameraSync.Message.NoSound", "No pattern sound from {mic} - bring the phone closer or unmute it"},
		{"CameraSync.Message.MicTap", "Could not listen to {mic}"},
		{"CameraSync.Button.Start", "Start"},
		{"CameraSync.Button.Stop", "Stop"},
		{"CameraSync.Row.Aligned", "\xE2\x9C\x93 {status}"},
		{"CameraSync.Message.Waiting", "Waiting for {camera}"},
		{"CameraSync.Message.AlreadyAligned", "Already aligned"},
		{"CameraSync.Message.OverLimit", "{camera} needs more delay than OBS allows"},
		{"CameraSync.Message.ApplyFailed", "Apply failed - nothing changed"},
		{"CameraSync.Message.ApplyRestoreFailed", "Apply failed - check the log"},
		{"CameraSync.Message.UndoFailed", "Undo failed - nothing changed"},
		{"CameraSync.Message.UndoRestoreFailed", "Undo failed - check the log"},
		{"CameraSync.Message.Applied", "Applied - {values}"},
		{"CameraSync.Message.Undone", "Undone - {values}"},
		{"CameraSync.Message.Value", "{name} {value} ms"},
		{"CameraSync.Message.Aligned", "Aligned"},
		{"CameraSync.Question.CamerasAndMic", "Shift {n} cameras and the mic while on air?"},
		{"CameraSync.Question.CameraAndMic", "Shift 1 camera and the mic while on air?"},
		{"CameraSync.Question.Cameras", "Shift {n} cameras while on air?"},
		{"CameraSync.Question.Camera", "Shift 1 camera while on air?"},
		{"CameraSync.Question.Mic", "Shift the mic while on air?"},
		{"CameraSync.Question.Pause", "Cameras whose delay changes pause until their new delay fills."},
	};
	const auto found = text.find(key);
	return found == text.end() ? std::string(key) : found->second;
}

const std::string minus = "\xE2\x88\x92"; // U+2212

SourceInfo camera(const char *id, const char *name)
{
	return SourceInfo{id, name, true, false, true};
}

CameraRow row(const char *id, CameraState state, int64_t live_ms = 0, int64_t reading_ms = 0)
{
	CameraRow result;
	result.id = id;
	result.state = state;
	result.live_ms = live_ms;
	result.reading_ms = reading_ms;
	return result;
}

// One ticked camera "cam" measured as `measured`: its row's status.
std::string status_of(const CameraRow &measured)
{
	SessionSnapshot snapshot;
	snapshot.cameras = {measured};
	const DockView view = build_view(snapshot, {camera("cam", "Cam")}, Choice{{"cam"}, std::nullopt}, false, en_us);
	REQUIRE(view.cameras.size() == 1);
	return view.cameras[0].status;
}

} // namespace

TEST_CASE("format signed ms writes the sign the dock shows")
{
	CHECK(format_signed_ms(142) == "+142");
	CHECK(format_signed_ms(-60) == minus + "60");
	CHECK(format_signed_ms(0) == "0");
	// The minus is U+2212, bytes E2 88 92, and not the ASCII hyphen.
	CHECK(format_signed_ms(-60).substr(0, 3) == "\xE2\x88\x92");
	CHECK(format_signed_ms(-60).find('-') == std::string::npos);
}

TEST_CASE("a camera that is looking reads looking for the pattern")
{
	CHECK(status_of(row("cam", CameraState::Looking)) == "Looking for the pattern");
}

TEST_CASE("a settling camera shows its live value and not its reading")
{
	CHECK(status_of(row("cam", CameraState::Settling, 142, 7)) == "+142 ms - settling");

	// The spec asks for an explicit sign: a negative live value takes the minus sign, as a reading does.
	CHECK(status_of(row("cam", CameraState::Settling, -60, 7)) == minus + "60 ms - settling");
	CHECK(status_of(row("cam", CameraState::Settling, 0, 7)) == "0 ms - settling");
}

TEST_CASE("a stable camera above zero reads video late")
{
	CHECK(status_of(row("cam", CameraState::Stable, 99, 142)) == "+142 ms - video late");
}

TEST_CASE("a stable camera below zero reads video early with the minus sign")
{
	CHECK(status_of(row("cam", CameraState::Stable, 99, -60)) == minus + "60 ms - video early");
}

TEST_CASE("a stable camera at zero reads in sync")
{
	CHECK(status_of(row("cam", CameraState::Stable, 99, 0)) == "0 ms - in sync");
}

TEST_CASE("an unticked camera shows no status even when the session holds a row for it")
{
	SessionSnapshot snapshot;
	snapshot.cameras = {row("a", CameraState::Stable, 0, 142), row("b", CameraState::Stable, 0, 142)};
	const DockView view = build_view(snapshot, {camera("a", "Cam A"), camera("b", "Cam B")},
					 Choice{{"a"}, std::nullopt}, false, en_us);

	REQUIRE(view.cameras.size() == 2);
	CHECK(view.cameras[0].ticked);
	CHECK(view.cameras[0].status == "+142 ms - video late");
	CHECK_FALSE(view.cameras[1].ticked);
	CHECK(view.cameras[1].status == "");
}

TEST_CASE("a ticked camera the session dropped shows no status")
{
	SessionSnapshot snapshot;
	snapshot.cameras = {row("b", CameraState::Looking)};
	const DockView view = build_view(snapshot, {camera("a", "Cam A"), camera("b", "Cam B")},
					 Choice{{"a", "b"}, std::nullopt}, false, en_us);

	REQUIRE(view.cameras.size() == 2);
	CHECK(view.cameras[0].status == "");
	CHECK(view.cameras[1].status == "Looking for the pattern");
}

TEST_CASE("rows come in name order and the chosen mic is not one")
{
	SourceInfo mic = camera("mic", "A Mic");
	mic.audio = true;
	const DockView view = build_view(SessionSnapshot{}, {camera("c", "Cam C"), mic, camera("b", "cam b")},
					 Choice{{}, std::string("mic")}, false, en_us);

	REQUIRE(view.cameras.size() == 2);
	CHECK(view.cameras[0].name == "cam b");
	CHECK(view.cameras[0].id == "b");
	CHECK(view.cameras[1].name == "Cam C");
}

namespace {

SourceInfo mic_source(const char *id, const char *name)
{
	return SourceInfo{id, name, false, true, false};
}

// A catalog of two cameras and the mic "Shure MV7", with the cameras and mic as chosen.
DockView view_of(const SessionSnapshot &snapshot, const std::vector<std::string> &cameras,
		 const std::optional<std::string> &mic)
{
	return build_view(snapshot, {camera("a", "Cam A"), camera("b", "Cam B"), mic_source("mic", "Shure MV7")},
			  Choice{cameras, mic}, false, en_us);
}

SessionSnapshot snapshot_in(SessionState state)
{
	SessionSnapshot snapshot;
	snapshot.state = state;
	snapshot.mic = "mic";
	return snapshot;
}

} // namespace

TEST_CASE("start is disabled and the message asks for a pick when no camera is ticked")
{
	const DockView view = view_of(snapshot_in(SessionState::Idle), {}, std::string("mic"));

	CHECK_FALSE(view.start_enabled);
	CHECK(view.message == "Pick at least one camera and the mic");
}

TEST_CASE("start is disabled and the message asks for a pick when no mic is chosen")
{
	const DockView view = view_of(snapshot_in(SessionState::Stopped), {"a", "b"}, std::nullopt);

	CHECK_FALSE(view.start_enabled);
	CHECK(view.message == "Pick at least one camera and the mic");
}

TEST_CASE("start is enabled and labelled start when idle or stopped with a camera and the mic chosen")
{
	for (const SessionState state : {SessionState::Idle, SessionState::Stopped}) {
		const DockView view = view_of(snapshot_in(state), {"a"}, std::string("mic"));

		CHECK(view.start_enabled);
		CHECK(view.start_label == "Start");
		CHECK(view.message == "");
	}
}

TEST_CASE("the button reads stop and stays enabled while measuring")
{
	const DockView view = view_of(snapshot_in(SessionState::Measuring), {"a"}, std::string("mic"));
	CHECK(view.start_label == "Stop");
	CHECK(view.start_enabled);

	// The spec leaves the button unspecified while measuring: it stays enabled so a session can always be stopped, even
	// when the last camera has been unticked.
	const DockView none = view_of(snapshot_in(SessionState::Measuring), {}, std::string("mic"));
	CHECK(none.start_label == "Stop");
	CHECK(none.start_enabled);
}

TEST_CASE("reset is disabled when idle and enabled when measuring or stopped")
{
	CHECK_FALSE(view_of(snapshot_in(SessionState::Idle), {"a"}, std::string("mic")).reset_enabled);
	CHECK(view_of(snapshot_in(SessionState::Measuring), {"a"}, std::string("mic")).reset_enabled);
	CHECK(view_of(snapshot_in(SessionState::Stopped), {"a"}, std::string("mic")).reset_enabled);
}

TEST_CASE("a mic that hears no pattern sound is named on the message line until the flag clears")
{
	SessionSnapshot snapshot = snapshot_in(SessionState::Measuring);
	snapshot.no_pattern_sound = true;
	CHECK(view_of(snapshot, {"a"}, std::string("mic")).message ==
	      "No pattern sound from Shure MV7 - bring the phone closer or unmute it");

	snapshot.no_pattern_sound = false;
	CHECK(view_of(snapshot, {"a"}, std::string("mic")).message == "");
}

TEST_CASE("a mic that could not be listened to is named on the message line")
{
	SessionSnapshot snapshot = snapshot_in(SessionState::Idle);
	snapshot.mic_tap_failed = true;

	CHECK(view_of(snapshot, {"a"}, std::string("mic")).message == "Could not listen to Shure MV7");
}

TEST_CASE("the message line shows the pick prompt, then the mic tap failure, then no sound")
{
	SessionSnapshot snapshot = snapshot_in(SessionState::Measuring);
	snapshot.mic_tap_failed = true;
	snapshot.no_pattern_sound = true;

	CHECK(view_of(snapshot, {}, std::string("mic")).message == "Pick at least one camera and the mic");
	CHECK(view_of(snapshot, {"a"}, std::string("mic")).message == "Could not listen to Shure MV7");

	snapshot.mic_tap_failed = false;
	CHECK(view_of(snapshot, {"a"}, std::string("mic")).message ==
	      "No pattern sound from Shure MV7 - bring the phone closer or unmute it");
}

TEST_CASE("the mic list leaves out ticked cameras and marks the chosen mic")
{
	SourceInfo webcam = camera("webcam", "Webcam");
	webcam.audio = true;
	const DockView view =
		build_view(snapshot_in(SessionState::Idle),
			   {webcam, camera("a", "Cam A"), mic_source("mic", "Shure MV7"), mic_source("usb", "usb mic")},
			   Choice{{"webcam"}, std::string("usb")}, false, en_us);

	REQUIRE(view.mics.size() == 2);
	CHECK(view.mics[0].name == "Shure MV7");
	CHECK_FALSE(view.mics[0].chosen);
	CHECK(view.mics[1].name == "usb mic");
	CHECK(view.mics[1].id == "usb");
	CHECK(view.mics[1].chosen);
}

namespace {

// Camera "a" ticked and measured with 12 markers received and 2 missed, camera "b" not ticked but still in the session.
DockView counted_view(bool details)
{
	CameraRow measured = row("a", CameraState::Stable, 0, 142);
	measured.received = 12;
	measured.missed = 2;
	CameraRow other = row("b", CameraState::Looking);
	other.received = 5;
	other.missed = 1;
	SessionSnapshot snapshot = snapshot_in(SessionState::Measuring);
	snapshot.cameras = {measured, other};
	return build_view(snapshot, {camera("a", "Cam A"), camera("b", "Cam B"), mic_source("mic", "Shure MV7")},
			  Choice{{"a"}, std::string("mic")}, details, en_us);
}

} // namespace

TEST_CASE("details on adds the marker counts beside the unchanged status")
{
	const DockView view = counted_view(true);

	REQUIRE(view.cameras.size() == 2);
	CHECK(view.show_details);
	CHECK(view.cameras[0].details == "received 12, missed 2");
	CHECK(view.cameras[0].status == "+142 ms - video late");
	// A camera that is not ticked has neither a status nor counts.
	CHECK(view.cameras[1].details == "");
}

TEST_CASE("details off leaves the detail text empty")
{
	const DockView view = counted_view(false);

	REQUIRE(view.cameras.size() == 2);
	CHECK_FALSE(view.show_details);
	CHECK(view.cameras[0].details == "");
	CHECK(view.cameras[0].status == "+142 ms - video late");
}

namespace {

using camera_sync::AlignmentStatus;
using camera_sync::confirmation_detail;
using camera_sync::confirmation_question;
using camera_sync::Counts;
using camera_sync::format_value_ms;
using camera_sync::NamedValue;
using camera_sync::Outcome;
using camera_sync::OutcomeKind;

const std::string check_mark = "\xE2\x9C\x93 "; // U+2713 and a space

// Cam A and Cam B ticked against the mic Shure MV7, measuring at F = 33.333 ms, both Stable at 0 unless a test says
// otherwise.
struct Dock {
	std::vector<SourceInfo> sources{camera("a", "Cam A"), camera("b", "Cam B"), mic_source("mic", "Shure MV7")};
	Choice choice{{"a", "b"}, std::string("mic")};
	SessionSnapshot snapshot;
	AlignmentStatus alignment;

	Dock()
	{
		snapshot = snapshot_in(SessionState::Measuring);
		snapshot.frame_interval_ms = 33.333;
		snapshot.cameras = {row("a", CameraState::Stable, 0, 0), row("b", CameraState::Stable, 0, 0)};
	}

	DockView view() const { return build_view(snapshot, sources, choice, false, en_us, alignment); }
	std::string message() const { return view().message; }

	void last(OutcomeKind kind, std::string source = {}, std::vector<NamedValue> values = {})
	{
		Outcome outcome;
		outcome.kind = kind;
		outcome.source = std::move(source);
		outcome.values = std::move(values);
		alignment.last = outcome;
	}
};

} // namespace

TEST_CASE("apply is enabled only with a mic and every ticked camera stable")
{
	// ALIGN-47.
	Dock dock;
	CHECK(dock.view().apply_enabled);

	dock.snapshot.cameras[1].state = CameraState::Settling;
	CHECK_FALSE(dock.view().apply_enabled);
	dock.snapshot.cameras[1].state = CameraState::Stable;
	dock.snapshot.cameras[0].state = CameraState::Looking;
	CHECK_FALSE(dock.view().apply_enabled);
	dock.snapshot.cameras[0].state = CameraState::Stable;

	// A ticked camera the session has no row for has no stable reading.
	dock.snapshot.cameras.pop_back();
	CHECK_FALSE(dock.view().apply_enabled);
	dock.snapshot.cameras.push_back(row("b", CameraState::Stable, 0, 0));

	dock.choice.mic.reset();
	CHECK_FALSE(dock.view().apply_enabled);
	dock.choice.mic = "mic";

	dock.choice.cameras.clear();
	CHECK_FALSE(dock.view().apply_enabled);

	// Stable and already within one frame: Apply stays enabled and answers "Already aligned".
	dock.choice.cameras = {"a", "b"};
	CHECK(dock.view().apply_enabled);
}

TEST_CASE("the message line waits for the first camera in checklist order that is not stable")
{
	// ALIGN-48: checklist order ignores case, so "cam b" comes before "Cam C".
	Dock dock;
	dock.sources = {camera("c", "Cam C"), camera("b", "cam b"), camera("a", "Brio"), mic_source("mic", "Shure")};
	dock.choice.cameras = {"c", "b", "a"};
	dock.snapshot.cameras = {row("a", CameraState::Stable, 0, 50), row("b", CameraState::Settling, 40),
				 row("c", CameraState::Looking)};
	CHECK_FALSE(dock.view().apply_enabled);
	CHECK(dock.message() == "Waiting for cam b");

	// A message that outranks it shows instead.
	dock.snapshot.no_pattern_sound = true;
	CHECK(dock.message() == "No pattern sound from Shure - bring the phone closer or unmute it");
}

TEST_CASE("undo is enabled exactly while an undo snapshot exists")
{
	// ALIGN-49.
	Dock dock;
	CHECK_FALSE(dock.view().undo_enabled);
	dock.alignment.undo_available = true;
	CHECK(dock.view().undo_enabled);
	// Whatever else the dock shows.
	dock.choice.cameras.clear();
	CHECK(dock.view().undo_enabled);
}

TEST_CASE("the message line reads already aligned")
{
	Dock dock;
	dock.last(OutcomeKind::AlreadyAligned);
	CHECK(dock.message() == "Already aligned");
}

TEST_CASE("the message line names the camera or the mic over the limit")
{
	Dock dock;
	dock.last(OutcomeKind::OverLimit, "Cam B");
	CHECK(dock.message() == "Cam B needs more delay than OBS allows");
	dock.last(OutcomeKind::OverLimit, "Shure MV7");
	CHECK(dock.message() == "Shure MV7 needs more delay than OBS allows");
}

TEST_CASE("the message line reads apply failed nothing changed")
{
	Dock dock;
	dock.last(OutcomeKind::ApplyFailed);
	CHECK(dock.message() == "Apply failed - nothing changed");
}

TEST_CASE("the message line reads apply failed check the log after a failed restore")
{
	Dock dock;
	dock.last(OutcomeKind::ApplyRestoreFailed);
	CHECK(dock.message() == "Apply failed - check the log");
}

TEST_CASE("the message line lists every applied value in order")
{
	// ALIGN-54.
	Dock dock;
	dock.last(OutcomeKind::Applied, {}, {{"Cam A", 0}, {"Cam B", 60}, {"Shure MV7", 100}});
	CHECK(dock.message() == "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms");
}

TEST_CASE("the message line lists every undone value with a true minus sign")
{
	// ALIGN-55: a negative value takes U+2212, and no value takes a plus sign.
	Dock dock;
	dock.last(OutcomeKind::Undone, {}, {{"Cam A", 0}, {"Cam B", 0}, {"Shure MV7", -200}});
	CHECK(dock.message() == "Undone - Cam A 0 ms, Cam B 0 ms, Shure MV7 " + minus + "200 ms");

	CHECK(format_value_ms(60) == "60");
	CHECK(format_value_ms(0) == "0");
	CHECK(format_value_ms(-200) == minus + "200");
	CHECK(format_value_ms(-200).find('-') == std::string::npos);
}

TEST_CASE("the message line words a failed undo and a failed undo restore")
{
	Dock dock;
	dock.last(OutcomeKind::UndoFailed);
	CHECK(dock.message() == "Undo failed - nothing changed");
	dock.last(OutcomeKind::UndoRestoreFailed);
	CHECK(dock.message() == "Undo failed - check the log");
}

TEST_CASE("the message line shows no result before any apply or undo")
{
	Dock dock;
	CHECK(dock.message() == "");
}

TEST_CASE("a ticked camera within one frame after an apply gets a check mark")
{
	// ALIGN-58, F = 33.333 ms.
	Dock dock;
	dock.sources = {camera("a", "Cam A"), camera("b", "Cam B"), camera("c", "Cam C"), camera("d", "Cam D"),
			mic_source("mic", "Shure MV7")};
	dock.choice.cameras = {"a", "b", "c", "d"};
	dock.snapshot.cameras = {row("a", CameraState::Stable, 0, 4), row("b", CameraState::Stable, 0, 0),
				 row("c", CameraState::Stable, 0, 34), row("d", CameraState::Settling, 3)};

	DockView view = dock.view();
	REQUIRE(view.cameras.size() == 4);
	CHECK(view.cameras[0].status == "+4 ms - video late");
	CHECK(view.cameras[1].status == "0 ms - in sync");

	dock.alignment.aligned_after_apply = true;
	view = dock.view();
	CHECK(view.cameras[0].status == check_mark + "+4 ms - video late");
	CHECK(view.cameras[1].status == check_mark + "0 ms - in sync");
	CHECK(view.cameras[2].status == "+34 ms - video late");
	CHECK(view.cameras[3].status == "+3 ms - settling");

	// Both bounds: -33 is within F, -34 is not.
	dock.snapshot.cameras[2].reading_ms = -33;
	CHECK(dock.view().cameras[2].status == check_mark + minus + "33 ms - video early");
	dock.snapshot.cameras[2].reading_ms = -34;
	CHECK(dock.view().cameras[2].status == minus + "34 ms - video early");
}

TEST_CASE("the message line reads aligned once every ticked camera is marked")
{
	// ALIGN-59.
	Dock dock;
	dock.alignment.aligned_after_apply = true;
	dock.last(OutcomeKind::Applied, {}, {{"Cam A", 0}, {"Cam B", 60}, {"Shure MV7", 100}});
	dock.snapshot.cameras = {row("a", CameraState::Stable, 0, 4), row("b", CameraState::Stable, 0, -2)};
	CHECK(dock.message() == "Aligned");

	// One unmarked: the last result.
	dock.snapshot.cameras[1].reading_ms = 40;
	CHECK(dock.message() == "Applied - Cam A 0 ms, Cam B 60 ms, Shure MV7 100 ms");

	// One not Stable and no result: waiting.
	dock.alignment.last.reset();
	dock.snapshot.cameras[1].state = CameraState::Settling;
	CHECK(dock.message() == "Waiting for Cam B");

	// Without an Apply, readings within one frame are not "Aligned".
	dock.alignment.aligned_after_apply = false;
	dock.snapshot.cameras[1] = row("b", CameraState::Stable, 0, 0);
	CHECK(dock.message() == "");
}

TEST_CASE("the message line ranks its messages")
{
	// ALIGN-60: pick, could not listen, no pattern sound, Aligned, the last result, waiting.
	Dock dock;
	dock.alignment.aligned_after_apply = true;
	dock.last(OutcomeKind::ApplyFailed);

	SUBCASE("pick over could not listen")
	{
		dock.snapshot.mic_tap_failed = true;
		dock.choice.cameras.clear();
		CHECK(dock.message() == "Pick at least one camera and the mic");
	}
	SUBCASE("could not listen over no pattern sound")
	{
		dock.snapshot.mic_tap_failed = true;
		dock.snapshot.no_pattern_sound = true;
		CHECK(dock.message() == "Could not listen to Shure MV7");
	}
	SUBCASE("no pattern sound over Aligned")
	{
		dock.snapshot.no_pattern_sound = true;
		CHECK(dock.message() == "No pattern sound from Shure MV7 - bring the phone closer or unmute it");
	}
	SUBCASE("Aligned over the last result")
	{
		CHECK(dock.message() == "Aligned");
	}
	SUBCASE("the last result over waiting")
	{
		dock.snapshot.cameras[0].state = CameraState::Settling;
		CHECK(dock.message() == "Apply failed - nothing changed");
	}
}

TEST_CASE("the on-air question words each count")
{
	// ALIGN-61.
	CHECK(confirmation_question(Counts{3, true}, en_us) == "Shift 3 cameras and the mic while on air?");
	CHECK(confirmation_question(Counts{1, true}, en_us) == "Shift 1 camera and the mic while on air?");
	CHECK(confirmation_question(Counts{2, false}, en_us) == "Shift 2 cameras while on air?");
	CHECK(confirmation_question(Counts{1, false}, en_us) == "Shift 1 camera while on air?");
	CHECK(confirmation_question(Counts{0, true}, en_us) == "Shift the mic while on air?");
}

TEST_CASE("the on-air question adds the pause line only when a camera changes")
{
	// ALIGN-62.
	const std::string pause = "Cameras whose delay changes pause until their new delay fills.";
	CHECK(confirmation_detail(Counts{1, false}, en_us) == pause);
	CHECK(confirmation_detail(Counts{3, true}, en_us) == pause);
	CHECK(confirmation_detail(Counts{0, true}, en_us) == "");
}
