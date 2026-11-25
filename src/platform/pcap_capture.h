// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace platform::pcap {

struct FileInfo {
    std::string name;
    std::uintmax_t bytes{};
    std::string modified;
};

std::filesystem::path directory();
std::vector<FileInfo> list_files();

class Writer {
public:
    bool open(const std::filesystem::path& path);
    bool append(const uint8_t* data, std::size_t length, std::uint64_t timestamp_us);
    bool flush();
    void close();
    bool good() const { return file_.good(); }
private:
    std::ofstream file_;
};

class Capture {
public:
    ~Capture();
    Capture(const Capture&) = delete;
    Capture& operator=(const Capture&) = delete;
    Capture() = default;
    std::string start(int socket, unsigned reserve_mib);
    std::string stop();
    bool active() const { return active_; }
    std::uint64_t frames() const { return frames_; }
    const std::string& path() const { return path_; }
    std::string take_error();
private:
    std::atomic<bool> stop_{false}, active_{false}, failed_{false};
    std::atomic<std::uint64_t> frames_{0};
    std::thread thread_;
    std::string path_, error_;
};

} // namespace platform::pcap
