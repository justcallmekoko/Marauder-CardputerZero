// SPDX-License-Identifier: AGPL-3.0-only
#include "map_packs.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace platform::maps {
namespace {
const std::array<Pack, pack_count()> packs = {{
    {"OSM Bright", "openstreetmap", "https://raw.githubusercontent.com/meshtastic/device-ui/fb3c069926505c1d59a9ab7b547edfda9f82e70e/maps/osm.zip", 26104110, 25286324},
    {"Dark Matter", "dark-matter-brown", "https://raw.githubusercontent.com/meshtastic/device-ui/fb3c069926505c1d59a9ab7b547edfda9f82e70e/maps/dark-matter-brown.zip", 12425866, 11920338},
    {"Around GPS", "local", "https://tiles.coalition.space/raster/styles/dark-matter", 0, 0},
    {"North America", "area-north-america", "", 0, 0, "REGION", -169, 7, -52, 72},
    {"South America", "area-south-america", "", 0, 0, "REGION", -82, -56, -34, 13},
    {"Europe", "area-europe", "", 0, 0, "REGION", -12, 34, 42, 72},
    {"Africa", "area-africa", "", 0, 0, "REGION", -18, -35, 52, 38},
    {"Asia", "area-asia", "", 0, 0, "REGION", 25, -11, 180, 78},
    {"Oceania", "area-oceania", "", 0, 0, "REGION", 110, -48, 179, 0},
    {"United States (lower 48)", "area-us-lower-48", "", 0, 0, "COUNTRY", -125, 24, -66, 50},
    {"Canada (south)", "area-canada-south", "", 0, 0, "COUNTRY", -140, 41, -52, 60},
    {"Mexico", "area-mexico", "", 0, 0, "COUNTRY", -118, 14, -86, 33},
    {"Brazil", "area-brazil", "", 0, 0, "COUNTRY", -74, -34, -34, 6},
    {"United Kingdom", "area-uk", "", 0, 0, "COUNTRY", -9, 49, 2, 59},
    {"France (mainland)", "area-france", "", 0, 0, "COUNTRY", -6, 41, 10, 52},
    {"Germany", "area-germany", "", 0, 0, "COUNTRY", 5, 47, 16, 56},
    {"Spain", "area-spain", "", 0, 0, "COUNTRY", -10, 35, 5, 45},
    {"Italy", "area-italy", "", 0, 0, "COUNTRY", 6, 36, 19, 48},
    {"Netherlands", "area-netherlands", "", 0, 0, "COUNTRY", 3, 50, 8, 54},
    {"Japan", "area-japan", "", 0, 0, "COUNTRY", 123, 24, 146, 46},
    {"South Korea", "area-south-korea", "", 0, 0, "COUNTRY", 126, 33, 130, 39},
    {"Australia", "area-australia", "", 0, 0, "COUNTRY", 112, -44, 154, -10},
    {"New Zealand", "area-new-zealand", "", 0, 0, "COUNTRY", 166, -48, 179, -34},
    {"California", "area-california", "", 0, 0, "US STATE", -125, 32, -114, 43},
    {"Texas", "area-texas", "", 0, 0, "US STATE", -107, 25, -93, 37},
    {"Florida", "area-florida", "", 0, 0, "US STATE", -88, 24, -80, 32},
    {"New York", "area-new-york", "", 0, 0, "US STATE", -80, 40, -71, 46},
}};
struct Bounds { int min_x, max_x, min_y, max_y; };
int tile_x(double longitude, unsigned zoom) {
    return std::clamp(static_cast<int>(std::floor((longitude + 180.0) / 360.0 * (1u << zoom))), 0, (1 << zoom) - 1);
}
int tile_y(double latitude, unsigned zoom) {
    const double radians = std::clamp(latitude, -85.0, 85.0) * 3.141592653589793 / 180.0;
    return std::clamp(static_cast<int>(std::floor((1.0 - std::asinh(std::tan(radians)) / 3.141592653589793) * (1u << zoom) / 2.0)), 0, (1 << zoom) - 1);
}
Bounds area_bounds(const Pack& area, unsigned zoom) {
    return {tile_x(area.west, zoom), tile_x(area.east, zoom), tile_y(area.north, zoom), tile_y(area.south, zoom)};
}
Bounds local_bounds(int32_t lat_e7, int32_t lon_e7, unsigned zoom) {
    const double latitude_deg = std::clamp(static_cast<double>(lat_e7) / 1e7, -85.0, 85.0);
    const double latitude = latitude_deg * 3.141592653589793 / 180.0;
    const double world = 256.0 * (1u << zoom);
    const double x = world * (static_cast<double>(lon_e7) / 1e7 + 180.0) / 360.0;
    const double y = world * (1.0 - std::asinh(std::tan(latitude)) / 3.141592653589793) / 2.0;
    const double radius = 3000.0 * world / (40075017.0 * std::max(0.05, std::cos(latitude)));
    const int limit = (1u << zoom) - 1;
    return {static_cast<int>(std::floor((x - radius) / 256.0)),
            static_cast<int>(std::floor((x + radius) / 256.0)),
            std::clamp(static_cast<int>(std::floor((y - radius) / 256.0)), 0, limit),
            std::clamp(static_cast<int>(std::floor((y + radius) / 256.0)), 0, limit)};
}
bool png(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    char signature[8]{};
    file.read(signature, 8);
    return file.gcount() == 8 && std::string(signature, 8) == "\x89PNG\r\n\x1a\n";
}
#ifndef _WIN32
bool run(const std::vector<std::string>& arguments) {
    std::vector<char*> args;
    for (const auto& arg : arguments) args.push_back(const_cast<char*>(arg.c_str()));
    args.push_back(nullptr);
    const auto child = fork();
    if (child == 0) { execvp(args[0], args.data()); _exit(127); }
    if (child < 0) return false;
    int status{};
    return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif
}
const Pack& pack(unsigned index) { return packs.at(index); }
std::filesystem::path root() {
    const char* home = std::getenv("HOME");
    return home && *home ? std::filesystem::path(home) / "Documents/Marauder/maps" : std::filesystem::path{};
}
std::filesystem::path tile(unsigned index, unsigned zoom, unsigned x, unsigned y) {
    if (index >= packs.size() || zoom < 1 || zoom > (index == 2 ? 16u : index >= 3 ? 10u : 6u) || x >= (1u << zoom) || y >= (1u << zoom)) return {};
    return root() / pack(index).directory / std::to_string(zoom) / std::to_string(x) / (std::to_string(y) + ".png");
}
bool installed(unsigned index) {
    return index < packs.size() && !root().empty() && (index >= 2 ?
        std::filesystem::is_regular_file(root() / pack(index).directory / ".ready") :
        std::filesystem::is_regular_file(tile(index, 1, 0, 0)));
}
unsigned installed_zoom(unsigned index) {
    if (!installed(index)) return 0;
    if (index < 2) return 6;
    std::ifstream marker(root() / pack(index).directory / ".ready");
    unsigned zoom{};
    marker >> zoom;
    return zoom;
}
std::uint64_t free_bytes() {
    const auto directory = root();
    if (directory.empty()) return 0;
    auto probe = directory;
    while (!probe.empty() && !std::filesystem::exists(probe)) probe = probe.parent_path();
    std::error_code error;
    if (probe.empty()) return 0;
    const auto capacity = std::filesystem::space(probe, error);
    return error ? 0 : capacity.available;
}
std::string install(unsigned index) {
    if (index >= 2 || root().empty()) return "Map storage unavailable";
    if (installed(index)) return "Already installed";
    const auto& selected = pack(index);
    if (free_bytes() < selected.archive_bytes + selected.installed_bytes + 64u * 1024u * 1024u)
        return "Not enough free space";
#ifdef _WIN32
    return "Download on the Cardputer";
#else
    std::error_code error;
    std::filesystem::create_directories(root(), error);
    if (error) return "Cannot create map folder";
    const auto temp = root() / (std::string(selected.directory) + ".part.zip");
    const auto stage = root() / (std::string(selected.directory) + ".staging");
    std::filesystem::remove(temp, error);
    std::filesystem::remove_all(stage, error);
    std::filesystem::create_directories(stage, error);
    if (error) return "Cannot prepare download";
    const bool fetched = run({"curl", "--fail", "--location", "--silent", "--show-error", "--retry", "2", "--max-time", "180", "--output", temp.string(), selected.url});
    if (!fetched || std::filesystem::file_size(temp, error) != selected.archive_bytes || error) {
        std::filesystem::remove(temp, error);
        std::filesystem::remove_all(stage, error);
        return "Map download failed";
    }
    const bool extracted = run({"unzip", "-q", temp.string(), "-d", stage.string()});
    const auto source = stage / "maps" / selected.directory;
    if (!extracted || !std::filesystem::is_regular_file(source / "1/0/0.png")) {
        std::filesystem::remove(temp, error);
        std::filesystem::remove_all(stage, error);
        return "Map pack invalid";
    }
    std::filesystem::rename(source, root() / selected.directory, error);
    std::filesystem::remove(temp, error);
    std::filesystem::remove_all(stage, error);
    return installed(index) ? "Map installed" : "Could not install map";
#endif
}
std::uint64_t local_estimate(int32_t lat_e7, int32_t lon_e7, unsigned max_zoom) {
    if (max_zoom < 6 || max_zoom > 16 || lat_e7 < -850000000 || lat_e7 > 850000000 ||
        lon_e7 < -1800000000 || lon_e7 > 1800000000) return 0;
    std::uint64_t tiles{};
    for (unsigned zoom = 6; zoom <= max_zoom; ++zoom) {
        const auto bounds = local_bounds(lat_e7, lon_e7, zoom);
        tiles += static_cast<std::uint64_t>(bounds.max_x - bounds.min_x + 1) *
                 static_cast<std::uint64_t>(bounds.max_y - bounds.min_y + 1);
    }
    return tiles * 40960;
}
std::uint64_t local_size() {
    const auto directory = root() / "local";
    std::error_code error;
    std::uint64_t total{};
    if (!std::filesystem::exists(directory, error)) return 0;
    for (auto it = std::filesystem::recursive_directory_iterator(directory, error);
         !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        if (it->is_regular_file(error) && it->path().extension() == ".png")
            total += it->file_size(error);
    }
    return total;
}
std::string install_local(int32_t lat_e7, int32_t lon_e7, unsigned max_zoom) {
    const auto estimate = local_estimate(lat_e7, lon_e7, max_zoom);
    if (!estimate) return "GPS fix required";
    if (free_bytes() < estimate + 64u * 1024u * 1024u) return "Not enough free space";
#ifdef _WIN32
    return "Download on the Cardputer";
#else
    std::error_code error;
    for (unsigned zoom = 6; zoom <= max_zoom; ++zoom) {
        const auto bounds = local_bounds(lat_e7, lon_e7, zoom);
        std::vector<std::string> args = {"curl", "--parallel", "--parallel-immediate", "--parallel-max", "4",
            "--fail", "--location", "--silent", "--show-error", "--retry", "2", "--max-time", "30"};
        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> pending;
        for (int y = bounds.min_y; y <= bounds.max_y; ++y) for (int x = bounds.min_x; x <= bounds.max_x; ++x) {
            const unsigned wrapped_x = static_cast<unsigned>((x % (1 << zoom) + (1 << zoom)) % (1 << zoom));
            const auto final = tile(2, zoom, wrapped_x, static_cast<unsigned>(y));
            if (png(final)) continue;
            std::filesystem::create_directories(final.parent_path(), error);
            if (error) return "Cannot create map folder";
            const auto temp = final.string() + ".part";
            pending.emplace_back(temp, final);
            args.insert(args.end(), {"--output", temp,
                std::string(pack(2).url) + "/" + std::to_string(zoom) + "/" +
                std::to_string(wrapped_x) + "/" + std::to_string(y) + ".png"});
        }
        if (!pending.empty() && !run(args)) return "Tile download failed; retry to resume";
        for (const auto& [temp, final] : pending) {
            if (!png(temp)) return "Tile download invalid; retry";
            std::filesystem::rename(temp, final, error);
            if (error) return "Could not save map tile";
        }
    }
    std::ofstream ready(root() / "local/.ready");
    ready << max_zoom << "\nOpenStreetMap contributors\n";
    return ready ? "Local map installed" : "Could not finish map pack";
#endif
}
std::uint64_t area_estimate(unsigned index, unsigned max_zoom) {
    if (index < 3 || index >= packs.size() || max_zoom < 1 || max_zoom > 10) return 0;
    std::uint64_t count{};
    for (unsigned zoom = 1; zoom <= max_zoom; ++zoom) {
        const auto bounds = area_bounds(pack(index), zoom);
        count += static_cast<std::uint64_t>(bounds.max_x - bounds.min_x + 1) *
                 static_cast<std::uint64_t>(bounds.max_y - bounds.min_y + 1);
    }
    return count * 40960;
}
std::uint64_t area_size(unsigned index) {
    if (index < 3 || index >= packs.size()) return 0;
    std::error_code error;
    const auto directory = root() / pack(index).directory;
    if (!std::filesystem::exists(directory, error)) return 0;
    std::uint64_t total{};
    for (auto it = std::filesystem::recursive_directory_iterator(directory, error);
         !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        if (it->is_regular_file(error) && it->path().extension() == ".png") total += it->file_size(error);
    }
    return total;
}
std::string install_area(unsigned index, unsigned max_zoom) {
    const auto estimate = area_estimate(index, max_zoom);
    if (!estimate || root().empty()) return "Map area unavailable";
    if (estimate > 2000ULL * 40960) return "Area too large; lower zoom";
    if (free_bytes() < estimate + 64ULL * 1024 * 1024) return "Not enough free space";
#ifdef _WIN32
    return "Download on the Cardputer";
#else
    std::error_code error;
    const auto& selected = pack(index);
    for (unsigned zoom = 1; zoom <= max_zoom; ++zoom) {
        const auto bounds = area_bounds(selected, zoom);
        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> pending;
        auto fetch = [&]() -> bool {
            if (pending.empty()) return true;
            std::vector<std::string> args = {"curl", "--parallel", "--parallel-immediate", "--parallel-max", "4",
                "--fail", "--location", "--silent", "--show-error", "--retry", "2", "--max-time", "30"};
            for (const auto& [temp, final] : pending) {
                const auto y = final.stem().string();
                const auto x = final.parent_path().filename().string();
                args.insert(args.end(), {"--output", temp.string(),
                    "https://tiles.coalition.space/raster/styles/dark-matter/" + std::to_string(zoom) + "/" + x + "/" + y + ".png"});
            }
            if (!run(args)) return false;
            for (const auto& [temp, final] : pending) {
                if (!png(temp)) return false;
                std::filesystem::rename(temp, final, error);
                if (error) return false;
            }
            pending.clear();
            return true;
        };
        for (int y = bounds.min_y; y <= bounds.max_y; ++y) for (int x = bounds.min_x; x <= bounds.max_x; ++x) {
            const auto final = tile(index, zoom, static_cast<unsigned>(x), static_cast<unsigned>(y));
            if (png(final)) continue;
            std::filesystem::create_directories(final.parent_path(), error);
            if (error) return "Cannot create map folder";
            pending.emplace_back(final.string() + ".part", final);
            if (pending.size() == 32 && !fetch()) return "Tile download failed; retry to resume";
        }
        if (!fetch()) return "Tile download failed; retry to resume";
    }
    std::ofstream marker(root() / selected.directory / ".ready");
    marker << max_zoom << "\nOpenStreetMap contributors\n";
    return marker ? "Area map installed" : "Could not finish map pack";
#endif
}
}
