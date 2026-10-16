// SPDX-License-Identifier: GPL-2.0-or-later
// Runs every file of a seed corpus through the fuzz target's function, so CTest replays the corpus with any compiler.
// It fails when the directory holds no file, so a wrong path cannot pass silently. The target aborts on an accepted
// out-of-range value, which fails the test.
// Usage: qr-payload-replay <corpus directory>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::fprintf(stderr, "usage: qr-payload-replay <corpus directory>\n");
		return 2;
	}

	std::vector<std::filesystem::path> files;
	for (const auto &entry : std::filesystem::directory_iterator(argv[1])) {
		if (entry.is_regular_file())
			files.push_back(entry.path());
	}
	if (files.empty()) {
		std::fprintf(stderr, "no corpus file in %s\n", argv[1]);
		return 1;
	}

	for (const auto &path : files) {
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			std::fprintf(stderr, "cannot read %s\n", path.string().c_str());
			return 1;
		}
		const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t *>(content.data()), content.size());
	}
	std::printf("replayed %zu corpus files\n", files.size());
	return 0;
}
