// SPDX-License-Identifier: GPL-2.0-or-later
#include "controller.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

namespace camera_sync {

namespace {

// The core's lines through OBS's log. obs_log adds the "[camera-sync] " prefix; Debug lines show when OBS logs
// verbosely.
void log_to_obs(LogLevel level, const std::string &message)
{
	int obs_level = LOG_INFO;
	switch (level) {
	case LogLevel::Error:
		obs_level = LOG_ERROR;
		break;
	case LogLevel::Warning:
		obs_level = LOG_WARNING;
		break;
	case LogLevel::Info:
		obs_level = LOG_INFO;
		break;
	case LogLevel::Debug:
		obs_level = LOG_DEBUG;
		break;
	}
	obs_log(obs_level, "%s", message.c_str());
}

uint64_t monotonic_ns()
{
	return os_gettime_ns();
}

// A source's name as OBS shows it, for the session's log lines.
std::string source_name(const SourceId &id)
{
	obs_source_t *source = obs_get_source_by_uuid(id.c_str());
	const char *name = source ? obs_source_get_name(source) : nullptr;
	std::string result = name ? name : "";
	obs_source_release(source);
	return result;
}

std::string translate(const char *key)
{
	return obs_module_text(key);
}

// The UUID of the source a libobs signal is about, or none.
std::optional<SourceId> signalled_source(calldata_t *data)
{
	obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(data, "source"));
	const char *uuid = source ? obs_source_get_uuid(source) : nullptr;
	if (!uuid)
		return std::nullopt;
	return SourceId(uuid);
}

} // namespace

// OBS's callbacks. The frontend calls on the UI thread; libobs signals fire on whatever thread made the change, so
// each signal is queued to the UI thread before it reaches the session.
struct ObsCallbacks {
	static void frontend_event(enum obs_frontend_event event, void *param)
	{
		Controller &controller = *static_cast<Controller *>(param);
		switch (event) {
		case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
			controller.on_collection_changing();
			break;
		case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
			controller.load_selection();
			break;
		case OBS_FRONTEND_EVENT_EXIT:
			controller.on_exit();
			break;
		default:
			break;
		}
	}

	static void source_removed(void *param, calldata_t *data)
	{
		Controller *controller = static_cast<Controller *>(param);
		if (std::optional<SourceId> source = signalled_source(data))
			QMetaObject::invokeMethod(
				&controller->context_,
				[controller, id = *source] { controller->on_source_removed(id); },
				Qt::QueuedConnection);
	}

	static void source_renamed(void *param, calldata_t *data)
	{
		Controller *controller = static_cast<Controller *>(param);
		if (std::optional<SourceId> source = signalled_source(data))
			QMetaObject::invokeMethod(
				&controller->context_,
				[controller, id = *source] { controller->on_source_renamed(id); },
				Qt::QueuedConnection);
	}

	// filter_add and filter_remove of a ticked camera.
	static void camera_filters_changed(void *param, calldata_t *data)
	{
		Controller *controller = static_cast<Controller *>(param);
		if (std::optional<SourceId> source = signalled_source(data))
			QMetaObject::invokeMethod(
				&controller->context_,
				[controller, id = *source] { controller->session_.on_camera_filters_changed(id); },
				Qt::QueuedConnection);
	}

	// audio_sync of the mic.
	static void mic_sync_offset_changed(void *param, calldata_t *)
	{
		Controller *controller = static_cast<Controller *>(param);
		QMetaObject::invokeMethod(
			&controller->context_, [controller] { controller->session_.on_mic_sync_offset_changed(); },
			Qt::QueuedConnection);
	}
};

Controller::Controller()
	: session_(factory_, monotonic_ns, log_to_obs, source_name),
	  store_(std::make_unique<SelectionStore>([this] { return to_saved(choice_); }))
{
	obs_frontend_add_event_callback(ObsCallbacks::frontend_event, this);
	signal_handler_t *handler = obs_get_signal_handler();
	signal_handler_connect(handler, "source_remove", ObsCallbacks::source_removed, this);
	signal_handler_connect(handler, "source_rename", ObsCallbacks::source_renamed, this);
	attached_ = true;

	// The dock refreshes about ten times a second (READ-96).
	QObject::connect(&timer_, &QTimer::timeout, &context_, [this] { refresh(); });
	timer_.start(100);
}

Controller::~Controller()
{
	timer_.stop();
	unwatch_all();
	detach();
}

void Controller::set_view_sink(ViewSink *sink, QObject *widget)
{
	sink_ = sink;
	sink_widget_ = widget;
	refresh();
}

void Controller::set_camera_ticked(const SourceId &camera, bool ticked)
{
	auto found = std::find(choice_.cameras.begin(), choice_.cameras.end(), camera);
	if (ticked == (found != choice_.cameras.end()))
		return;
	if (ticked)
		choice_.cameras.push_back(camera);
	else
		choice_.cameras.erase(found);
	apply_choice();
}

void Controller::choose_mic(const SourceId &mic)
{
	if (choice_.mic == mic)
		return;
	choice_.mic = mic;
	apply_choice();
}

void Controller::start_or_stop()
{
	if (session_.snapshot().state == SessionState::Measuring) {
		session_.stop();
	} else {
		// F is read from the canvas at each Start (READ-16).
		obs_video_info video;
		if (obs_get_video_info(&video))
			session_.start(FrameRate{video.fps_num, video.fps_den});
	}
	refresh();
}

void Controller::reset()
{
	session_.reset();
	refresh();
}

void Controller::set_details(bool details)
{
	details_ = details;
	refresh();
}

void Controller::on_collection_changing()
{
	// Every tap goes before OBS clears the collection (READ-28). The choice stays as it is: OBS saves the old
	// collection after this event, and the store writes the choice into it. What the old collection loaded with is
	// forgotten, so a new or duplicated collection, which OBS does not load, is judged as having saved nothing.
	unwatch_all();
	session_.collection_changing();
	store_->forget();
}

void Controller::load_selection()
{
	// The collection's sources exist and the store holds what it saved, if anything (READ-50 to READ-53).
	sources_ = catalog_.list();
	choice_ = initial_choice(sources_, store_->saved());
	apply_choice();
}

void Controller::on_exit()
{
	// Every tap goes before libobs does (READ-29), and the callbacks are removed while the frontend still exists.
	timer_.stop();
	unwatch_all();
	session_.shutdown();
	store_.reset();
	detach();
	sink_ = nullptr;
}

void Controller::on_source_removed(const SourceId &source)
{
	// A removed camera leaves the choice and the session drops it (READ-55); a removed mic ends measuring (READ-35).
	auto camera = std::find(choice_.cameras.begin(), choice_.cameras.end(), source);
	if (camera != choice_.cameras.end()) {
		choice_.cameras.erase(camera);
		session_.on_camera_removed(source);
		watch_selection();
	}
	if (choice_.mic == source) {
		choice_.mic.reset();
		session_.on_mic_removed();
		watch_selection();
	}
}

void Controller::on_source_renamed(const SourceId &source)
{
	// A renamed camera is unticked and keeps its place in the list under its new name (READ-56).
	auto camera = std::find(choice_.cameras.begin(), choice_.cameras.end(), source);
	if (camera == choice_.cameras.end())
		return;
	choice_.cameras.erase(camera);
	session_.on_camera_renamed(source);
	watch_selection();
}

void Controller::apply_choice()
{
	session_.set_selection(choice_.cameras, choice_.mic);
	watch_selection();
	refresh();
}

// Connects the signals of the ticked cameras and the chosen mic, and disconnects the others'.
void Controller::watch_selection()
{
	unwatch_all();
	if (!attached_)
		return;
	for (const SourceId &camera : choice_.cameras)
		watch(camera, false);
	if (choice_.mic)
		watch(*choice_.mic, true);
}

void Controller::watch(const SourceId &id, bool mic)
{
	obs_source_t *source = obs_get_source_by_uuid(id.c_str());
	if (!source)
		return;
	signal_handler_t *handler = obs_source_get_signal_handler(source);
	if (mic) {
		signal_handler_connect(handler, "audio_sync", ObsCallbacks::mic_sync_offset_changed, this);
	} else {
		signal_handler_connect(handler, "filter_add", ObsCallbacks::camera_filters_changed, this);
		signal_handler_connect(handler, "filter_remove", ObsCallbacks::camera_filters_changed, this);
	}
	// A weak reference, so a watched source can still be destroyed.
	watched_.push_back(Watched{obs_source_get_weak_source(source), mic});
	obs_source_release(source);
}

void Controller::unwatch_all()
{
	for (const Watched &watched : watched_) {
		obs_source_t *source = obs_weak_source_get_source(watched.source);
		if (source) {
			signal_handler_t *handler = obs_source_get_signal_handler(source);
			if (watched.mic) {
				signal_handler_disconnect(handler, "audio_sync", ObsCallbacks::mic_sync_offset_changed,
							  this);
			} else {
				signal_handler_disconnect(handler, "filter_add", ObsCallbacks::camera_filters_changed,
							  this);
				signal_handler_disconnect(handler, "filter_remove",
							  ObsCallbacks::camera_filters_changed, this);
			}
			obs_source_release(source);
		}
		obs_weak_source_release(watched.source);
	}
	watched_.clear();
}

void Controller::detach()
{
	if (!attached_)
		return;
	obs_frontend_remove_event_callback(ObsCallbacks::frontend_event, this);
	signal_handler_t *handler = obs_get_signal_handler();
	signal_handler_disconnect(handler, "source_remove", ObsCallbacks::source_removed, this);
	signal_handler_disconnect(handler, "source_rename", ObsCallbacks::source_renamed, this);
	attached_ = false;
}

void Controller::refresh()
{
	if (!attached_)
		return;
	session_.tick();
	const SessionSnapshot snapshot = session_.snapshot();

	// The session also unticks on its own, a camera that delivered no frame for 2 s (READ-57); the choice follows it.
	std::vector<SourceId> kept;
	for (const SourceId &camera : choice_.cameras) {
		const bool measured = std::any_of(snapshot.cameras.begin(), snapshot.cameras.end(),
						  [&camera](const CameraRow &row) { return row.id == camera; });
		if (measured)
			kept.push_back(camera);
	}
	if (kept.size() != choice_.cameras.size() || snapshot.mic != choice_.mic) {
		choice_.cameras = std::move(kept);
		choice_.mic = snapshot.mic;
		watch_selection();
	}

	if (!sink_ || !sink_widget_)
		return;
	sources_ = catalog_.list();
	sink_->render(build_view(snapshot, sources_, choice_, details_, translate));
}

} // namespace camera_sync
