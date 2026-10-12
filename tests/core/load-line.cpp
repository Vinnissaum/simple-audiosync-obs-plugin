// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include "load-line.hpp"

using camera_sync::format_version;
using camera_sync::load_line;

// libobs packs a version as major << 24 | minor << 16 | patch (MAKE_SEMANTIC_VERSION in util/c99defs.h).
static constexpr uint32_t pack(uint32_t major, uint32_t minor, uint32_t patch)
{
	return (major << 24) | (minor << 16) | patch;
}

TEST_CASE("format_version turns a packed libobs version into major.minor.patch")
{
	CHECK(format_version(pack(30, 0, 2)) == "30.0.2");
}

TEST_CASE("format_version keeps a zero patch")
{
	CHECK(format_version(pack(30, 0, 0)) == "30.0.0");
}

TEST_CASE("format_version keeps a two-digit minor")
{
	CHECK(format_version(pack(31, 12, 3)) == "31.12.3");
}

TEST_CASE("load_line formats the fields of the load line")
{
	const std::string expected = "load version=0.1.0 obs_built=30.0.2 obs_running=32.0.1 quirc=542848d";
	CHECK(load_line("0.1.0", pack(30, 0, 2), "32.0.1", "542848d") == expected);
}

TEST_CASE("load_line puts the plugin version in the version field")
{
	const std::string expected = "load version=1.4.7 obs_built=30.0.2 obs_running=32.0.1 quirc=542848d";
	CHECK(load_line("1.4.7", pack(30, 0, 2), "32.0.1", "542848d") == expected);
}

TEST_CASE("load_line formats the compiled-against version in the obs_built field")
{
	const std::string expected = "load version=0.1.0 obs_built=30.0.0 obs_running=32.0.1 quirc=542848d";
	CHECK(load_line("0.1.0", pack(30, 0, 0), "32.0.1", "542848d") == expected);
}

TEST_CASE("load_line copies the running version string into the obs_running field")
{
	const std::string expected = "load version=0.1.0 obs_built=30.0.2 obs_running=30.0.0 quirc=542848d";
	CHECK(load_line("0.1.0", pack(30, 0, 2), "30.0.0", "542848d") == expected);
}

TEST_CASE("load_line puts the quirc commit in the quirc field")
{
	const std::string expected = "load version=0.1.0 obs_built=30.0.2 obs_running=32.0.1 quirc=927d680";
	CHECK(load_line("0.1.0", pack(30, 0, 2), "32.0.1", "927d680") == expected);
}

TEST_CASE("load_line leaves the log prefix to obs_log")
{
	CHECK(load_line("0.1.0", pack(30, 0, 2), "32.0.1", "542848d").find("[camera-sync]") == std::string::npos);
}
