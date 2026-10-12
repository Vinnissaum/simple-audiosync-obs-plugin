// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <string>

namespace camera_sync {

enum class LogLevel { Error, Warning, Info, Debug };

// The core logs through whatever the caller hands it, so it builds without libobs. The plugin hands it obs_log.
// An empty Logger drops the message.
using Logger = std::function<void(LogLevel, const std::string &)>;

inline void write_log(const Logger &log, LogLevel level, const std::string &message)
{
	if (log)
		log(level, message);
}

} // namespace camera_sync
