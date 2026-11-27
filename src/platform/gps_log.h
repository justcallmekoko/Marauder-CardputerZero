// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

namespace platform::gps {
enum class LogKind { Track, Wardrive };

class Log {
public:
    bool start(LogKind kind, unsigned reserve_mib, std::string& error);
    bool position(int32_t latitude_e7, int32_t longitude_e7);
    bool access_point(const std::string& bssid, const std::string& ssid,
                      int channel, int frequency_mhz, int rssi, const std::string& security,
                      int32_t latitude_e7, int32_t longitude_e7);
    bool bluetooth_device(const std::string& address, const std::string& name,
                          int rssi, const std::string& address_type,
                          int32_t latitude_e7, int32_t longitude_e7);
    void stop();
    void reset();
    bool active() const { return file_.is_open(); }
    std::size_t count() const { return count_; }
    std::size_t bluetooth_count() const { return bluetooth_count_; }
    const std::filesystem::path& path() const { return path_; }
    static std::filesystem::path directory();
private:
    bool write_position(int32_t latitude_e7, int32_t longitude_e7, int frequency_mhz = 0);
    std::ofstream file_;
    std::filesystem::path path_;
    std::unordered_set<std::string> seen_;
    LogKind kind_{LogKind::Track};
    std::size_t count_{};
    std::size_t bluetooth_count_{};
    unsigned reserve_mib_{};
};
}
