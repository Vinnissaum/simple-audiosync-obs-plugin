// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs/obs-settings-port.hpp"

#include <cstring>
#include <utility>

#include <obs.h>

namespace camera_sync {

namespace {

// OBS's Video Delay (Async), from obs-filters, and its one setting.
constexpr const char *delay_filter_id = "async_delay_filter";
constexpr const char *delay_setting = "delay_ms";

// A source found by its UUID, released when the holder goes.
class SourceRef {
public:
	explicit SourceRef(const SourceId &id) : source_(obs_get_source_by_uuid(id.c_str())) {}
	~SourceRef() { obs_source_release(source_); }
	SourceRef(const SourceRef &) = delete;
	SourceRef &operator=(const SourceRef &) = delete;

	obs_source_t *get() const { return source_; }

private:
	obs_source_t *source_;
};

bool recognised(const char *name)
{
	if (!name)
		return false;
	for (const char *known : recognised_filter_names) {
		if (std::strcmp(name, known) == 0)
			return true;
	}
	return false;
}

// obs_source_enum_filters walks the chain in order, the first filter applied first. It holds the source's filter lock,
// so this only takes a reference. obs_source_get_filter_by_name is not used: it ignores the type and returns the last
// filter of that name.
void take_first_plugin_filter(obs_source_t *, obs_source_t *filter, void *param)
{
	obs_source_t **found = static_cast<obs_source_t **>(param);
	if (*found)
		return;
	const char *id = obs_source_get_unversioned_id(filter);
	if (id && std::strcmp(id, delay_filter_id) == 0 && recognised(obs_source_get_name(filter)))
		*found = obs_source_get_ref(filter);
}

// The camera's plugin filter with a reference the caller releases, or null.
obs_source_t *plugin_filter(obs_source_t *camera)
{
	obs_source_t *found = nullptr;
	obs_source_enum_filters(camera, take_first_plugin_filter, &found);
	return found;
}

} // namespace

ObsSettingsPort::ObsSettingsPort(std::function<bool()> on_air) : on_air_(std::move(on_air)) {}

bool ObsSettingsPort::resolves(const SourceId &source)
{
	return SourceRef(source).get() != nullptr;
}

bool ObsSettingsPort::delay_filter_available()
{
	// Without obs-filters, obs_source_create still makes a placeholder that obs_source_filter_add accepts and that
	// delays nothing, so a write would read back as landed (ALIGN-14).
	return obs_source_get_display_name(delay_filter_id) != nullptr;
}

std::optional<FilterState> ObsSettingsPort::read_filter(const SourceId &camera)
{
	SourceRef source(camera);
	if (!source.get())
		return std::nullopt;
	obs_source_t *filter = plugin_filter(source.get());
	if (!filter)
		return FilterState{};
	obs_data_t *settings = obs_source_get_settings(filter);
	const FilterState state{true, obs_data_get_int(settings, delay_setting), obs_source_enabled(filter)};
	obs_data_release(settings);
	obs_source_release(filter);
	return state;
}

bool ObsSettingsPort::write_filter(const SourceId &camera, int64_t delay_ms, bool enabled)
{
	SourceRef source(camera);
	if (!source.get())
		return false;
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, delay_setting, delay_ms);
	obs_source_t *filter = plugin_filter(source.get());
	if (filter) {
		obs_source_update(filter, settings);
	} else {
		// A new filter goes at the end of the chain, after every filter the user added (ALIGN-21).
		filter = obs_source_create(delay_filter_id, recognised_filter_names[0], settings, nullptr);
		if (filter)
			obs_source_filter_add(source.get(), filter);
	}
	obs_data_release(settings);
	if (!filter)
		return false;
	obs_source_set_enabled(filter, enabled);
	obs_source_release(filter);
	return true;
}

bool ObsSettingsPort::remove_filter(const SourceId &camera)
{
	SourceRef source(camera);
	if (!source.get())
		return false;
	obs_source_t *filter = plugin_filter(source.get());
	if (!filter)
		return true;
	obs_source_filter_remove(source.get(), filter);
	obs_source_release(filter);
	return true;
}

std::optional<int64_t> ObsSettingsPort::read_sync_offset_ns(const SourceId &mic)
{
	SourceRef source(mic);
	if (!source.get())
		return std::nullopt;
	return obs_source_get_sync_offset(source.get());
}

bool ObsSettingsPort::write_sync_offset_ns(const SourceId &mic, int64_t ns)
{
	// libobs lets an audio_sync handler rewrite the offset before storing it; the read-back catches that.
	SourceRef source(mic);
	if (!source.get())
		return false;
	obs_source_set_sync_offset(source.get(), ns);
	return true;
}

bool ObsSettingsPort::on_air()
{
	return on_air_ && on_air_();
}

} // namespace camera_sync
