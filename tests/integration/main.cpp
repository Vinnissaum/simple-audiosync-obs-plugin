// SPDX-License-Identifier: GPL-2.0-or-later
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "obs-env.hpp"

int main(int argc, char **argv)
{
	doctest::Context context(argc, argv);
	const int result = context.run();
	// libobs started on the first case that needed it, if any did; listing the cases at build time starts nothing.
	obs_env::shutdown();
	return result;
}
