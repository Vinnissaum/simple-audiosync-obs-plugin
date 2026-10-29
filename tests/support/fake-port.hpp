// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "alignment/settings-port.hpp"

// A settings port over plain maps. It records every call in order, and can refuse a chosen write, read back a wrong
// value once after a chosen write, or forget a source. Writes are counted across write_filter, remove_filter and
// write_sync_offset_ns, from 0.
struct FakePort : camera_sync::SettingsPort {
	std::map<camera_sync::SourceId, camera_sync::FilterState> filters; // each camera's plugin filter, if any
	std::map<camera_sync::SourceId, int64_t> offsets;                  // each mic's Sync Offset in ns
	std::set<camera_sync::SourceId> sources;                           // every source that resolves
	bool filter_available = true;
	bool live = false; // streaming or recording

	std::vector<std::string> calls;
	std::set<size_t> refuse_writes;         // these writes return false and change nothing
	std::set<size_t> misread_after;         // the next read after these writes returns a wrong value
	std::set<size_t> misread_enabled_after; // the next filter read after these writes flips only enabled
	size_t writes = 0;
	std::function<void()> on_write; // runs at each write, before it lands

	void camera(const camera_sync::SourceId &id, camera_sync::FilterState filter = {})
	{
		sources.insert(id);
		if (filter.present)
			filters[id] = filter;
	}

	void mic(const camera_sync::SourceId &id, int64_t ns = 0)
	{
		sources.insert(id);
		offsets[id] = ns;
	}

	// The source leaves OBS with everything on it.
	void forget(const camera_sync::SourceId &id)
	{
		sources.erase(id);
		filters.erase(id);
		offsets.erase(id);
	}

	camera_sync::FilterState filter(const camera_sync::SourceId &id) const
	{
		auto found = filters.find(id);
		return found == filters.end() ? camera_sync::FilterState{} : found->second;
	}

	// The calls that change a value, in order.
	std::vector<std::string> write_calls() const
	{
		std::vector<std::string> found;
		for (const std::string &call : calls)
			if (call.rfind("write_", 0) == 0 || call.rfind("remove_", 0) == 0)
				found.push_back(call);
		return found;
	}

	bool resolves(const camera_sync::SourceId &source) override
	{
		calls.push_back("resolves " + source);
		return sources.count(source) == 1;
	}

	bool delay_filter_available() override
	{
		calls.push_back("available");
		return filter_available;
	}

	std::optional<camera_sync::FilterState> read_filter(const camera_sync::SourceId &camera) override
	{
		calls.push_back("read_filter " + camera);
		if (!sources.count(camera))
			return std::nullopt;
		camera_sync::FilterState state = filter(camera);
		if (misread_pending) {
			misread_pending = false;
			state = state.present ? camera_sync::FilterState{true, state.delay_ms + 1, state.enabled}
					      : camera_sync::FilterState{true, 1, true};
		}
		if (misread_enabled_pending) {
			misread_enabled_pending = false;
			state.enabled = !state.enabled;
		}
		return state;
	}

	bool write_filter(const camera_sync::SourceId &camera, int64_t delay_ms, bool enabled) override
	{
		calls.push_back("write_filter " + camera + " " + std::to_string(delay_ms) + (enabled ? " on" : " off"));
		if (!begin_write() || !sources.count(camera))
			return false;
		filters[camera] = camera_sync::FilterState{true, delay_ms, enabled};
		return true;
	}

	bool remove_filter(const camera_sync::SourceId &camera) override
	{
		calls.push_back("remove_filter " + camera);
		if (!begin_write() || !sources.count(camera))
			return false;
		filters.erase(camera);
		return true;
	}

	std::optional<int64_t> read_sync_offset_ns(const camera_sync::SourceId &mic) override
	{
		calls.push_back("read_offset " + mic);
		if (!sources.count(mic))
			return std::nullopt;
		int64_t ns = offsets[mic];
		if (misread_pending) {
			misread_pending = false;
			ns += 1;
		}
		return ns;
	}

	bool write_sync_offset_ns(const camera_sync::SourceId &mic, int64_t ns) override
	{
		calls.push_back("write_offset " + mic + " " + std::to_string(ns));
		if (!begin_write() || !sources.count(mic))
			return false;
		offsets[mic] = ns;
		return true;
	}

	bool on_air() override
	{
		calls.push_back("on_air");
		return live;
	}

private:
	bool misread_pending = false;
	bool misread_enabled_pending = false;

	// Counts the write, runs on_write, and says whether it may land.
	bool begin_write()
	{
		const size_t index = writes++;
		if (on_write)
			on_write();
		if (misread_after.count(index))
			misread_pending = true;
		if (misread_enabled_after.count(index))
			misread_enabled_pending = true;
		return refuse_writes.count(index) == 0;
	}
};
