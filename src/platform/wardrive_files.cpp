// SPDX-License-Identifier: AGPL-3.0-only
#include "wardrive_files.h"
#include "gps_log.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstring>
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
        {
            std::ifstream log(it->path());
            std::string header, first_row;
            item.eligible = static_cast<bool>(std::getline(log, header) &&
                                               std::getline(log, first_row) && !first_row.empty());
        }
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
            if (wigle == "uploaded" || wigle == "accepted") item.wigle = wigle == "uploaded" ? "Uploaded" : "Queued";
            if (wdg == "uploaded" || wdg == "accepted") item.wdg = wdg == "uploaded" ? "Uploaded" : "Queued";
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

namespace {
Result run_helper(const std::string& path, const std::string& service, Progress* progress) {
#if defined(__linux__) && !USE_DESKTOP
    int output[2];
    if (pipe(output) != 0) return {false, "Network check unavailable"};
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
    if (child < 0) { close(output[0]); return {false, "Network check unavailable"}; }
    std::string message;
    FILE* stream = fdopen(output[0], "r");
    if (stream) {
        char line[512];
        while (std::fgets(line, sizeof(line), stream)) {
            unsigned long long sent{}, total{};
            int phase{};
            if (progress && std::sscanf(line, "PROGRESS %llu %llu", &sent, &total) == 2) {
                progress->sent = sent;
                progress->total = total;
            } else if (progress && std::sscanf(line, "PHASE %d", &phase) == 1 && phase >= 0 && phase <= 3) {
                progress->phase_started_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                progress->phase = phase;
            } else if (std::strncmp(line, "PROGRESS ", 9) != 0 && std::strncmp(line, "PHASE ", 6) != 0) message = line;
        }
        fclose(stream);
    } else close(output[0]);
    int status{};
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) message.pop_back();
    const bool okay = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return {okay, message.empty() ? (okay ? "Completed" : "Request failed") : message.substr(0, 180)};
#else
    (void)path; (void)service; (void)progress;
    return {false, "Network checks require the Cardputer"};
#endif
}

}

Result upload(const std::string& name, const std::string& service, Progress* progress) {
    if (!safe_name(name) || (service != "wigle" && service != "wdg"))
        return {false, "Invalid upload request"};
    const auto path = path_for(name);
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || fs::is_symlink(path, ec)) return {false, "Log is missing"};
    return run_helper(path.string(), service, progress);
}

Result check_credentials(const std::string& service) {
    if (service != "wigle" && service != "wdg") return {false, "Invalid service"};
    return run_helper("--check", service, nullptr);
}
}
