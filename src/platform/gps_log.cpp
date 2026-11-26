// SPDX-License-Identifier: AGPL-3.0-only
#include "gps_log.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace platform::gps {
namespace {
std::string timestamp(const char* pattern) {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), pattern, &tm);
    return buffer;
}
std::string quote(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        if (c == '"') result += "\"\"";
        else if (c == '\r' || c == '\n') result += ' ';
        else result += c;
    }
    return result + '"';
}
}
std::filesystem::path Log::directory() {
    const char* home = std::getenv("HOME");
    return (home && *home ? std::filesystem::path(home) : std::filesystem::temp_directory_path()) /
           "Documents" / "Marauder" / "gps";
}
bool Log::start(LogKind kind, unsigned reserve_mib, std::string& error) {
    stop();
    std::error_code ec;
    const auto root = directory();
    std::filesystem::create_directories(root, ec);
    if (ec) { error = "Cannot create GPS folder"; return false; }
    const auto space = std::filesystem::space(root, ec);
    if (ec || space.available < (static_cast<std::uint64_t>(reserve_mib) + 1) * 1048576ULL)
        { error = "Storage reserve reached"; return false; }
    kind_ = kind;
    count_ = 0;
    reserve_mib_ = reserve_mib;
    const auto stem = std::string(kind == LogKind::Track ? "track_" : "wardrive_") +
                      timestamp("%Y%m%d_%H%M%S");
    path_ = root / (stem + ".csv");
    for (unsigned i = 1; std::filesystem::exists(path_, ec) && i < 100; ++i)
        path_ = root / (stem + "_" + std::to_string(i) + ".csv");
    if (ec || std::filesystem::exists(path_)) { error = "Cannot name GPS file"; return false; }
    file_.open(path_, std::ios::out | std::ios::trunc);
    if (!file_) { error = "Cannot open GPS file"; return false; }
    std::filesystem::permissions(path_, std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, ec);
    if (ec) { stop(); std::filesystem::remove(path_); error = "Cannot protect GPS file"; return false; }
    file_ << (kind == LogKind::Track ? "utc,latitude,longitude\n" :
             "utc,bssid,ssid,channel,rssi_dbm,security,latitude,longitude\n");
    file_.flush();
    if (!file_) { stop(); error = "Cannot write GPS file"; return false; }
    error.clear();
    return true;
}
bool Log::write_position(int32_t latitude_e7, int32_t longitude_e7) {
    if (!file_) return false;
    if (count_ % 32 == 0) {
        std::error_code ec;
        const auto space = std::filesystem::space(path_.parent_path(), ec);
        if (ec || space.available < (static_cast<std::uint64_t>(reserve_mib_) + 1) * 1048576ULL)
            { stop(); return false; }
    }
    file_ << std::fixed << std::setprecision(7) << static_cast<double>(latitude_e7) / 1e7 << ','
          << static_cast<double>(longitude_e7) / 1e7 << '\n';
    file_.flush();
    if (!file_) { stop(); return false; }
    ++count_;
    return true;
}
bool Log::position(int32_t latitude_e7, int32_t longitude_e7) {
    if (!active() || kind_ != LogKind::Track) return false;
    file_ << timestamp("%Y-%m-%dT%H:%M:%SZ") << ',';
    return write_position(latitude_e7, longitude_e7);
}
bool Log::access_point(const std::string& bssid, const std::string& ssid,
                       int channel, int rssi, const std::string& security,
                       int32_t latitude_e7, int32_t longitude_e7) {
    if (!active() || kind_ != LogKind::Wardrive || bssid.empty()) return false;
    if (!seen_.insert(bssid).second) return true;
    file_ << timestamp("%Y-%m-%dT%H:%M:%SZ") << ',' << quote(bssid) << ',' << quote(ssid)
          << ',' << channel << ',';
    if (rssi > -128) file_ << rssi;
    file_ << ',' << quote(security) << ',';
    return write_position(latitude_e7, longitude_e7);
}
void Log::stop() {
    if (file_.is_open()) file_.close();
    seen_.clear();
}
}
