/*
OBS Audio Video Sync Dock
Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
// Derived from Audio Video Sync Dock, src/peak-finder.hpp at e4ad2941ed5f2982e9f17b76ebaf22096319f3cb
// Changed for Camera Sync: the struct is named PeakFinder and sits in namespace camera_sync. The peak finding is unchanged.
#pragma once

#include <cstdint>

namespace camera_sync {

struct PeakFinder {
	uint64_t cand_ts = 0;
	uint64_t last_ts = 0;

	float cand_score = 0.0f;
	float last_score = 0.0f;

	uint64_t dumping_range = 2000000000;

	float dumping(uint64_t ts_last, uint64_t ts_next) const
	{
		if (ts_next <= ts_last)
			return 1.0f;
		if (ts_next - ts_last > dumping_range)
			return 0.0f;
		float f = (float)(ts_next - ts_last) / (float)dumping_range;
		return 1.0f - f * f;
	}

	bool append(float score, uint64_t ts, uint64_t wait_ts)
	{
		if (score > cand_score * dumping(cand_ts, ts)) {
			cand_ts = ts;
			cand_score = score;
			/* When updating candidate, which is the recent peak,
			 * there might be larger score coming next. */
			return false;
		}

		if (cand_ts + wait_ts > ts)
			return false;

		if (cand_ts > last_ts && cand_score > last_score * dumping(last_ts, cand_ts)) {
			last_ts = cand_ts;
			last_score = cand_score;
			return true;
		}

		return false;
	}
};

} // namespace camera_sync
