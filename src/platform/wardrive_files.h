// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace platform::wardrive {
struct FileInfo {
    std::string name, modified, wigle{"Not uploaded"}, wdg{"Not uploaded"};
    std::uintmax_t bytes{};
};
std::vector<FileInfo> list_files();
bool erase(const std::string& name, std::string& error);
std::string upload(const std::string& name, const std::string& service);
}
