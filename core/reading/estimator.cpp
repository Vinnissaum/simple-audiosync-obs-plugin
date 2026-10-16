// SPDX-License-Identifier: GPL-2.0-or-later
#include "reading/estimator.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace camera_sync {

namespace {

// Each marker list holds one wrap of indexes and drops the oldest.
constexpr size_t marker_cap = 256;
constexpr size_t window_size = 5;

uint64_t distance(uint64_t a, uint64_t b)
{
	return a > b ? a - b : b - a;
}

template<typename T> void push_capped(std::deque<T> &list, T item)
{
	list.push_back(std::move(item));
	if (list.size() > marker_cap)
		list.pop_front();
}

} // namespace

void ReadingEstimator::set_pattern(const PatternParams &pattern)
{
	std::lock_guard<std::mutex> lock(mutex_);
	pattern_ = pattern;
}

void ReadingEstimator::set_frame_interval_ms(double ms)
{
	std::lock_guard<std::mutex> lock(mutex_);
	frame_interval_ms_ = ms;
}

void ReadingEstimator::add_camera(const SourceId &camera)
{
	std::lock_guard<std::mutex> lock(mutex_);
	cameras_.emplace(camera, Camera{});
}

void ReadingEstimator::remove_camera(const SourceId &camera)
{
	std::lock_guard<std::mutex> lock(mutex_);
	cameras_.erase(camera);
}

void ReadingEstimator::clear_camera(const SourceId &camera)
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto found = cameras_.find(camera);
	if (found != cameras_.end())
		found->second = Camera{};
}

void ReadingEstimator::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto &entry : cameras_)
		entry.second = Camera{};
	audio_.clear();
}

void ReadingEstimator::reset_counters()
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto &entry : cameras_) {
		entry.second.received = 0;
		entry.second.missed = 0;
		entry.second.last_index.reset();
	}
}

SampleResult ReadingEstimator::on_video_marker(const SourceId &id, uint32_t index, uint64_t time_ns)
{
	std::lock_guard<std::mutex> lock(mutex_);
	SampleResult result;
	auto found = cameras_.find(id);
	if (found == cameras_.end())
		return result;
	Camera &camera = found->second;
	const Marker video{index, time_ns};

	// The indexes skipped since the camera's last marker, counted across the wrap. A repeated index skips none.
	camera.received++;
	const uint32_t index_max = pattern_.index_max;
	if (camera.last_index && index != *camera.last_index && index_max)
		camera.missed += (index + index_max - *camera.last_index - 1) % index_max;
	camera.last_index = index;

	// The nearest audio marker of the index that this camera has not paired with yet.
	AudioEntry *nearest = nullptr;
	uint64_t nearest_ns = 0;
	for (AudioEntry &audio : audio_) {
		if (!pairs(video, audio.marker))
			continue;
		if (std::find(audio.paired.begin(), audio.paired.end(), id) != audio.paired.end())
			continue;
		const uint64_t apart_ns = distance(time_ns, audio.marker.time_ns);
		if (!nearest || apart_ns < nearest_ns) {
			nearest = &audio;
			nearest_ns = apart_ns;
		}
	}

	if (!nearest) {
		push_capped(camera.waiting, video);
		return result;
	}
	nearest->paired.push_back(id);
	result.formed_sample = true;
	if (const std::optional<int64_t> spread_ms = add_sample(camera, time_ns, nearest->marker.time_ns)) {
		result.stable_changed = true;
		result.reading_ms = camera.reading_ms;
		result.spread_ms = *spread_ms;
	}
	return result;
}

std::vector<StableChange> ReadingEstimator::on_audio_marker(uint32_t index, uint64_t time_ns)
{
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<StableChange> changes;
	AudioEntry entry{{index, time_ns}, {}};

	// Each camera's nearest waiting video marker of the index pairs with it.
	for (auto &[id, camera] : cameras_) {
		auto nearest = camera.waiting.end();
		uint64_t nearest_ns = 0;
		for (auto it = camera.waiting.begin(); it != camera.waiting.end(); ++it) {
			if (!pairs(*it, entry.marker))
				continue;
			const uint64_t apart_ns = distance(it->time_ns, time_ns);
			if (nearest == camera.waiting.end() || apart_ns < nearest_ns) {
				nearest = it;
				nearest_ns = apart_ns;
			}
		}
		if (nearest == camera.waiting.end())
			continue;

		const uint64_t video_ns = nearest->time_ns;
		camera.waiting.erase(nearest);
		entry.paired.push_back(id);
		if (const std::optional<int64_t> spread_ms = add_sample(camera, video_ns, time_ns))
			changes.push_back(StableChange{id, camera.reading_ms, *spread_ms});
	}

	push_capped(audio_, std::move(entry));
	return changes;
}

EstimatorSnapshot ReadingEstimator::snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	EstimatorSnapshot snapshot;
	for (const auto &[id, camera] : cameras_) {
		CameraRow row;
		row.id = id;
		row.samples = static_cast<uint32_t>(camera.window.size());
		row.samples_formed = camera.samples_formed;
		row.received = camera.received;
		row.missed = camera.missed;
		if (!camera.window.empty()) {
			row.state = camera.stable ? CameraState::Stable : CameraState::Settling;
			row.live_ms = round_half_away(camera.window.back().ms);
		}
		row.reading_ms = camera.reading_ms;
		snapshot.cameras.push_back(row);
	}
	return snapshot;
}

bool ReadingEstimator::pairs(const Marker &video, const Marker &audio) const
{
	return video.index == audio.index && distance(video.time_ns, audio.time_ns) < pattern_.half_wrap_ns();
}

std::optional<int64_t> ReadingEstimator::add_sample(Camera &camera, uint64_t video_ns, uint64_t audio_ns)
{
	camera.samples_formed++;

	// Each difference is taken in the order that keeps it unsigned.
	const double ms = video_ns >= audio_ns ? (video_ns - audio_ns) / 1e6 : -((audio_ns - video_ns) / 1e6);

	// Inserted by video time, and the oldest leaves a full window, so a sample older than all five is the one that
	// leaves.
	std::vector<Sample> &window = camera.window;
	const auto earlier = [](uint64_t time, const Sample &other) {
		return time < other.video_ns;
	};
	window.insert(std::upper_bound(window.begin(), window.end(), video_ns, earlier), Sample{video_ns, ms});
	if (window.size() > window_size)
		window.erase(window.begin());

	// A full window within one frame, F included, makes the camera Stable at the median. A wider one changes
	// nothing, so a Stable camera keeps its reading.
	if (window.size() < window_size)
		return std::nullopt;
	std::array<double, window_size> sorted;
	for (size_t i = 0; i < window_size; i++)
		sorted[i] = window[i].ms;
	std::sort(sorted.begin(), sorted.end());
	const double spread = sorted.back() - sorted.front();
	if (spread > frame_interval_ms_)
		return std::nullopt;

	const int64_t reading_ms = round_half_away(sorted[window_size / 2]);
	if (camera.stable && camera.reading_ms == reading_ms)
		return std::nullopt;
	camera.stable = true;
	camera.reading_ms = reading_ms;
	return round_half_away(spread);
}

} // namespace camera_sync
