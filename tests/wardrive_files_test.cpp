// SPDX-License-Identifier: AGPL-3.0-only
#include "wardrive_files.h"
#include "gps_log.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

int main() {
    namespace fs = std::filesystem;
    const auto home = fs::temp_directory_path() /
        ("marauder-wardrive-files-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(home);
#if defined(_WIN32)
    _putenv_s("HOME", home.string().c_str());
#else
    setenv("HOME", home.string().c_str(), 1);
#endif
    const auto root = platform::gps::Log::directory();
    fs::create_directories(root);
    const auto name = std::string("wardrive_20261127_120000.csv");
    const auto file = root / name;
    std::ofstream(file) << "utc,radio,address,name\n2026-11-27T12:00:00Z,WiFi,AA:BB:CC:DD:EE:FF,Test\n";
    const auto modified = fs::last_write_time(file);
    const auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        modified - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::ofstream(file.string() + ".upload") << fs::file_size(file) << ' ' <<
        std::chrono::system_clock::to_time_t(system) << " uploaded accepted\n";
    auto files = platform::wardrive::list_files();
    assert(files.size() == 1 && files[0].name == name);
    assert(files[0].eligible && files[0].wigle == "Uploaded" && files[0].wdg == "Queued");
    std::string error;
    assert(!platform::wardrive::erase("../wardrive_20261127_120000.csv", error));
    assert(fs::exists(file));
    assert(platform::wardrive::erase(name, error));
    assert(!fs::exists(file) && !fs::exists(file.string() + ".upload"));
    fs::remove_all(home);
}
