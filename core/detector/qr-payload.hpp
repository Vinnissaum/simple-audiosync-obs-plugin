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
// Derived from Audio Video Sync Dock, src/sync-test-output.hpp at e4ad2941ed5f2982e9f17b76ebaf22096319f3cb
// Changed for Camera Sync: libobs's blog is the injected Logger. The payload parsing and its range checks are unchanged.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "log.hpp"

namespace camera_sync {

// The payload of one of the pattern's QR codes: the audio carrier and its cycles, the QR frames' duration, the marker
// index and its wrap, and the type flags.
struct QrPayload {
	uint32_t f = 0;
	uint32_t c = 0;
	uint32_t q_ms = 0;
	uint32_t index = -1;
	uint32_t index_max = 256;
	uint32_t type_flags = 0;
	bool valid = 0;

	bool decode_kv(char *param)
	{
		char *saveptr;
		char *key = tokenize(param, "=", &saveptr);
		if (!key || key[1] != 0)
			return false;

		char *val = tokenize(NULL, "=", &saveptr);
		if (!val)
			return false;

		switch (key[0]) {
		case 'f':
			f = (uint32_t)atoi(val);
			return true;
		case 'c':
			c = (uint32_t)atoi(val);
			return true;
		case 'q':
			q_ms = (uint32_t)atoi(val);
			return true;
		case 'i':
			index = (uint32_t)atoi(val);
			return true;
		case 'I':
			index_max = (uint32_t)atoi(val);
			return true;
		case 't':
			type_flags = (uint32_t)atoi(val);
			return true;
		default:
			/* Ignored */
			return true;
		}

		return false;
	}

	bool check(const Logger &log)
	{
		if (f < 10 || 32000 < f) {
			write_log(log, LogLevel::Warning, "f: out of range: " + std::to_string(f));
			return false;
		}
		if (c < 1 || f < c) {
			write_log(log, LogLevel::Warning, "c: out of range: " + std::to_string(c));
			return false;
		}
		if (q_ms < 1 || 1000 < q_ms) {
			write_log(log, LogLevel::Warning, "q: out of range: " + std::to_string(q_ms));
			return false;
		}
		if (index & ~0xFFu) {
			write_log(log, LogLevel::Warning, "i: out of range: " + std::to_string(index));
			return false;
		}
		return true;
	}

	bool decode(char *payload, const Logger &log)
	{
		valid = false;
		char *saveptr;
		char *param = tokenize(payload, ",", &saveptr);
		while (param) {
			if (!decode_kv(param))
				return false;
			param = tokenize(NULL, ",", &saveptr);
		}
		if (!check(log))
			return false;
		valid = true;
		return true;
	}

private:
	// strtok_r where the platform has it and strtok_s where it does not.
	static char *tokenize(char *str, const char *delim, char **saveptr)
	{
#ifdef _MSC_VER
		return strtok_s(str, delim, saveptr);
#else
		return strtok_r(str, delim, saveptr);
#endif
	}
};

} // namespace camera_sync
