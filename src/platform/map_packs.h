// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace platform::maps {
struct Pack {
    const char* name;
    const char* directory;
    const char* url;
    std::uint64_t archive_bytes;
    std::uint64_t installed_bytes;
};
const Pack& pack(unsigned index);
constexpr unsigned pack_count() { return 3; }
std::filesystem::path root();
std::filesystem::path tile(unsigned index, unsigned zoom, unsigned x, unsigned y);
bool installed(unsigned index);
std::uint64_t free_bytes();
std::string install(unsigned index);
std::uint64_t local_estimate(int32_t lat_e7, int32_t lon_e7, unsigned max_zoom);
std::uint64_t local_size();
std::string install_local(int32_t lat_e7, int32_t lon_e7, unsigned max_zoom);
}
