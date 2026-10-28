// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs-env.hpp"

#include <doctest/doctest.h>

#include <obs-module.h>

// OBS 30.0.2's Video Delay (Async), compiled from plugins/obs-filters/async-delay-filter.c in .deps.
extern "C" struct obs_source_info async_delay_filter;

// The filter's text lookup. The tests read no text, so the key is the text.
extern "C" const char *obs_module_text(const char *key)
{
	return key;
}

namespace obs_env {

namespace {

enum class State { NotStarted, Running, Failed };
State state = State::NotStarted;

int instance; // what every test source's create returns

const char *test_name(void *)
{
	return "Camera Sync test source";
}

void *test_create(obs_data_t *, obs_source_t *)
{
	return &instance;
}

void test_destroy(void *) {}

void register_test_type(const char *id, enum obs_source_type type, uint32_t flags)
{
	obs_source_info info = {};
	info.id = id;
	info.type = type;
	info.output_flags = flags;
	info.get_name = test_name;
	info.create = test_create;
	info.destroy = test_destroy;
	obs_register_source(&info);
}

void describe_filter(obs_source_t *, obs_source_t *filter, void *param)
{
	auto *found = static_cast<std::vector<std::string> *>(param);
	obs_data_t *settings = obs_source_get_settings(filter);
	found->push_back(std::string(obs_source_get_unversioned_id(filter)) + " " + obs_source_get_name(filter) + " " +
			 obs_data_get_json(settings) + (obs_source_enabled(filter) ? " on" : " off"));
	obs_data_release(settings);
}

} // namespace

bool ready()
{
	if (state == State::NotStarted) {
		state = State::Failed;
		// libobs's X11 hotkeys open the display, so CI runs the tests under xvfb-run.
		if (!obs_startup("en-US", nullptr, nullptr)) {
			MESSAGE("obs_startup failed: no display? ALIGN-80 moves this case to the rig");
			return false;
		}
		// Video Delay (Async) reads the audio rate when it is created.
		obs_audio_info audio = {};
		audio.samples_per_sec = 48000;
		audio.speakers = SPEAKERS_STEREO;
		if (!obs_reset_audio(&audio)) {
			MESSAGE("obs_reset_audio failed: ALIGN-80 moves this case to the rig");
			return false;
		}
		obs_register_source(&async_delay_filter);
		register_test_type(camera_type, OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_ASYNC_VIDEO);
		register_test_type(mic_type, OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_AUDIO);
		register_test_type(render_delay_type, OBS_SOURCE_TYPE_FILTER, OBS_SOURCE_VIDEO);
		register_test_type(other_filter_type, OBS_SOURCE_TYPE_FILTER, OBS_SOURCE_VIDEO);
		state = State::Running;
	}
	if (state == State::Failed)
		MESSAGE("libobs did not start, so this case ran nothing: ALIGN-80 moves it to the rig");
	return state == State::Running;
}

void shutdown()
{
	if (state == State::Running)
		obs_shutdown();
	state = State::NotStarted;
}

Source::Source(const char *type, const char *name) : source_(obs_source_create(type, name, nullptr, nullptr)) {}

Source::~Source()
{
	remove();
}

std::string Source::uuid() const
{
	return obs_source_get_uuid(source_);
}

void Source::remove()
{
	obs_source_release(source_);
	source_ = nullptr;
}

void Source::add_filter(const char *type, const char *name, int64_t delay_ms, bool enabled)
{
	obs_data_t *settings = obs_data_create();
	if (std::string(type) == delay_type)
		obs_data_set_int(settings, "delay_ms", delay_ms);
	obs_source_t *filter = obs_source_create(type, name, settings, nullptr);
	obs_data_release(settings);
	obs_source_filter_add(source_, filter);
	obs_source_set_enabled(filter, enabled);
	obs_source_release(filter);
}

std::vector<std::string> Source::filters() const
{
	std::vector<std::string> found;
	obs_source_enum_filters(source_, describe_filter, &found);
	return found;
}

} // namespace obs_env
