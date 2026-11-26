// SPDX-License-Identifier: AGPL-3.0-only
#include "gps_log.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("marauder-gps-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
#if defined(_WIN32)
    _putenv_s("HOME", root.string().c_str());
#else
    setenv("HOME", root.string().c_str(), 1);
#endif
    platform::gps::Log log;
    std::string error;
    assert(log.start(platform::gps::LogKind::Wardrive, 0, error));
    const auto wardrive = log.path();
    assert(log.access_point("AA:BB:CC:DD:EE:FF", "Test,\"AP\"", 6, -52, "WPA2", 407123456, -740123456));
    assert(log.access_point("AA:BB:CC:DD:EE:FF", "Test", 6, -60, "WPA2", 407123456, -740123456));
    assert(log.count() == 1);
    log.stop();
    std::ifstream file(wardrive);
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    assert(text.find("\"Test,\"\"AP\"\"\",6,-52,\"WPA2\",40.7123456,-74.0123456") != std::string::npos);
    assert(log.start(platform::gps::LogKind::Track, 0, error));
    assert(log.position(407123456, -740123456));
    assert(log.position(407123457, -740123456));
    assert(log.count() == 2);
    assert(log.path() != wardrive);
    log.stop();
    std::filesystem::remove_all(root);
}
