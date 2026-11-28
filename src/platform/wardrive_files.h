// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <atomic>
#include <string>
#include <vector>

namespace platform::wardrive {
struct FileInfo {
    std::string name, modified, wigle{"Not uploaded"}, wdg{"Not uploaded"};
    std::uintmax_t bytes{};
    bool eligible{};
};
struct Progress {
    std::atomic<std::uint64_t> sent{0}, total{0};
};
struct Result { bool ok{}; std::string message; };
std::vector<FileInfo> list_files();
bool erase(const std::string& name, std::string& error);
Result upload(const std::string& name, const std::string& service, Progress* progress = nullptr);
Result check_credentials(const std::string& service);
}
