// SPDX-License-Identifier: AGPL-3.0-only
#include "wardrive_files.h"
#include "gps_log.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>
#if defined(__linux__) && !USE_DESKTOP
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace platform::wardrive {
namespace {
namespace fs = std::filesystem;
bool safe_name(const std::string& name) {
    return name.size() > 14 && name.rfind("wardrive_", 0) == 0 &&
           name.substr(name.size() - 4) == ".csv" &&
           name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == std::string::npos;
}
fs::path path_for(const std::string& name) { return platform::gps::Log::directory() / name; }
std::time_t modified_time(const fs::file_time_type& time) {
    const auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(system);
}
}

std::vector<FileInfo> list_files() {
    std::vector<FileInfo> files;
    std::error_code ec;
    const auto root = platform::gps::Log::directory();
    if (!fs::exists(root, ec)) return files;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        if (!safe_name(name) || !it->is_regular_file(ec) || it->is_symlink(ec)) continue;
        FileInfo item;
        item.name = name;
        item.bytes = it->file_size(ec);
        const auto when = it->last_write_time(ec);
        if (ec) { ec.clear(); continue; }
        const auto stamp = modified_time(when);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &stamp);
#else
        localtime_r(&stamp, &tm);
#endif
        char buffer[24]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &tm);
        item.modified = buffer;
        std::ifstream status(it->path().string() + ".upload");
        std::uintmax_t size{};
        std::time_t time{};
        std::string wigle, wdg;
        status >> size >> time >> wigle >> wdg;
        if (status && size == item.bytes && std::abs(std::difftime(time, stamp)) <= 1) {
            if (wigle == "uploaded" || wigle == "accepted") item.wigle = wigle == "uploaded" ? "Uploaded" : "Accepted";
            if (wdg == "uploaded" || wdg == "accepted") item.wdg = wdg == "uploaded" ? "Uploaded" : "Accepted";
            if (wigle == "failed") item.wigle = "Failed";
            if (wdg == "failed") item.wdg = "Failed";
        }
        files.push_back(std::move(item));
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.name > b.name; });
    return files;
}

bool erase(const std::string& name, std::string& error) {
    if (!safe_name(name)) { error = "Invalid log name"; return false; }
    const auto path = path_for(name);
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || fs::is_symlink(path, ec) || !fs::remove(path, ec) || ec) {
        error = "Could not delete log"; return false;
    }
    fs::remove(path.string() + ".upload", ec);
    error.clear();
    return true;
}

std::string upload(const std::string& name, const std::string& service) {
    if (!safe_name(name) || (service != "wigle" && service != "wdg")) return "Invalid upload request";
    const auto path = path_for(name);
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || fs::is_symlink(path, ec)) return "Log is missing";
#if defined(__linux__) && !USE_DESKTOP
    int output[2];
    if (pipe(output) != 0) return "Upload unavailable";
    const pid_t child = fork();
    if (child == 0) {
        close(output[0]);
        dup2(output[1], STDOUT_FILENO);
        dup2(output[1], STDERR_FILENO);
        close(output[1]);
        execl("/usr/bin/python3", "python3", "-I", "/usr/lib/marauder/wardrive-upload",
              path.c_str(), service.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    close(output[1]);
    if (child < 0) { close(output[0]); return "Upload unavailable"; }
    std::string message;
    char buffer[256];
    ssize_t count;
    while ((count = read(output[0], buffer, sizeof(buffer))) > 0)
        if (message.size() < 256) message.append(buffer, std::min<std::size_t>(static_cast<std::size_t>(count), 256 - message.size()));
    close(output[0]);
    int status{};
    waitpid(child, &status, 0);
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) message.pop_back();
    return message.empty() ? (WIFEXITED(status) && WEXITSTATUS(status) == 0 ? "Uploaded" : "Upload failed") : message;
#else
    return "Uploads require the Cardputer";
#endif
}
}
