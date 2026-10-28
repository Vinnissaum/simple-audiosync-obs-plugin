// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "alignment/alignment.hpp"
#include "obs/obs-catalog.hpp"
#include "obs/obs-settings-port.hpp"
#include "obs/obs-tap-factory.hpp"
#include "obs/selection-store.hpp"
#include "reading/session.hpp"
#include "selection/selection-policy.hpp"
#include "viewmodel/view-model.hpp"

struct obs_weak_source;

namespace camera_sync {

// What shows the dock's view. The dock implements it.
class ViewSink {
public:
	virtual ~ViewSink() = default;
	virtual void render(const DockView &view) = 0;
	// Asks the on-air question modally: true when the user accepts. OBS keeps running while it is open.
	virtual bool confirm(const std::string &question, const std::string &detail) = 0;
};

// Connects OBS to the session. It owns the session, the catalog, the selection store and the tap factory, turns
// OBS's events and signals into session calls on the UI thread, and every 100 ms ticks the session and hands a fresh
// view to the dock. It holds no rule: the session, the selection policy and the view-model decide.
//
// Build it on the UI thread, from obs_module_load. It lets go of OBS at the exit event and is destroyed at unload.
class Controller {
public:
	Controller();
	~Controller();
	Controller(const Controller &) = delete;
	Controller &operator=(const Controller &) = delete;

	// The dock and the widget it lives in. The controller stops handing it views once the widget is destroyed, so the
	// dock never has to reach the controller from its destructor: OBS may destroy the dock after unloading the plugin.
	void set_view_sink(ViewSink *sink, QObject *widget);

	// The dock's requests.
	void set_camera_ticked(const SourceId &camera, bool ticked);
	void choose_mic(const SourceId &mic);
	void start_or_stop();
	void reset();
	void set_details(bool details);
	void apply();
	void undo();

private:
	friend struct ObsCallbacks;

	// The session as Apply and Undo see it.
	class Measuring : public MeasuringControl {
	public:
		explicit Measuring(Session &session) : session_(session) {}
		void clear_readings() override { session_.clear_readings(); }
		void resume(FrameRate rate) override { session_.start(rate); }

	private:
		Session &session_;
	};

	// A source whose own signals are connected: a ticked camera's filter changes, or the mic's Sync Offset.
	struct Watched {
		struct obs_weak_source *source;
		bool mic;
	};

	void on_collection_changing();
	void load_selection();
	void on_exit();
	void on_source_removed(const SourceId &source);
	void on_source_renamed(const SourceId &source);
	void apply_choice();
	void watch_selection();
	void watch(const SourceId &source, bool mic);
	void unwatch_all();
	void detach();
	void refresh();
	AlignmentRequest request();
	bool confirm(const Counts &counts);

	ObsTapFactory factory_;
	ObsCatalog catalog_;
	Session session_;
	ObsSettingsPort port_;
	Measuring measuring_;
	Alignment alignment_;
	std::unique_ptr<SelectionStore> store_;
	QObject context_; // runs the calls queued from OBS's threads on the UI thread; they die with it
	QTimer timer_;
	ViewSink *sink_ = nullptr;
	QPointer<QObject> sink_widget_;

	Choice choice_;
	bool details_ = false;
	std::vector<SourceInfo> sources_;
	std::vector<Watched> watched_;
	bool attached_ = false; // registered with the frontend and libobs's signals
};

} // namespace camera_sync
