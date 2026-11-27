// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

namespace platform::maps {
// shortcut: tile size varies by detail and filesystem, recalibrate with broader pack measurements.
constexpr std::uint64_t kEstimatedTileBytes = 20480;
struct Pack {
    const char* name;
    const char* directory;
    const char* url;
    std::uint64_t archive_bytes;
    std::uint64_t installed_bytes;
    const char* group{};
    double west{}, south{}, east{}, north{};
};
struct Progress {
    std::atomic<std::uint64_t> done{0}, total{0};
    std::atomic<bool> cancel{false};
};
const Pack& pack(unsigned index);
constexpr unsigned pack_count() { return 27; }
std::filesystem::path root();
std::filesystem::path tile(unsigned index, unsigned zoom, unsigned x, unsigned y);
bool installed(unsigned index);
unsigned installed_zoom(unsigned index);
std::uint64_t free_bytes();
std::string install(unsigned index, Progress* progress = nullptr);
std::uint64_t local_estimate(int32_t lat_e7, int32_t lon_e7, unsigned max_zoom);
std::uint64_t local_size();
std::string install_local(int32_t lat_e7, int32_t lon_e7, unsigned max_zoom, Progress* progress = nullptr);
std::uint64_t area_estimate(unsigned index, unsigned max_zoom);
std::uint64_t area_size(unsigned index);
std::string install_area(unsigned index, unsigned max_zoom, Progress* progress = nullptr);
std::uint64_t storage_size(unsigned index);
std::string erase(unsigned index);
}
