// SPDX-License-Identifier: AGPL-3.0-only
#include "pcap_capture.h"
#include "raw_wifi.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>

#if defined(__linux__) && !USE_DESKTOP
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#endif

namespace platform::pcap {
namespace {
void put32(std::ofstream& file, std::uint32_t value) {
    const std::array<char, 4> bytes{{static_cast<char>(value), static_cast<char>(value >> 8),
                                     static_cast<char>(value >> 16), static_cast<char>(value >> 24)}};
    file.write(bytes.data(), bytes.size());
}

std::string local_stamp(std::time_t value, const char* pattern) {
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &value);
#else
    localtime_r(&value, &tm);
#endif
    std::array<char, 32> buffer{};
    std::strftime(buffer.data(), buffer.size(), pattern, &tm);
    return buffer.data();
}
} // namespace

std::filesystem::path directory() {
    const char* home = std::getenv("HOME");
    return (home && *home ? std::filesystem::path(home) : std::filesystem::temp_directory_path()) /
           "Documents" / "Marauder" / "pcap";
}

bool matches_packet(const uint8_t* data, std::size_t length, const Filter& filter) {
    if (filter.scope == Scope::All) return true;
    platform::wireless::FrameSummary frame;
    return platform::wireless::parse_frame_summary(data, length, frame) &&
           platform::wireless::frame_matches_target(frame, filter.mac, filter.frequency_mhz);
}

std::vector<FileInfo> list_files() {
    std::vector<FileInfo> files;
    std::error_code error;
    const auto root = directory();
    if (!std::filesystem::exists(root, error)) return files;
    for (std::filesystem::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        if (!it->is_regular_file(error) || it->path().extension() != ".pcap") continue;
        FileInfo item;
        item.name = it->path().filename().string();
        item.bytes = it->file_size(error);
        const auto modified = it->last_write_time(error);
        if (error) { error.clear(); continue; }
        const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            modified - decltype(modified)::clock::now() + std::chrono::system_clock::now());
        item.modified = local_stamp(std::chrono::system_clock::to_time_t(system_time), "%Y-%m-%d %H:%M");
        files.push_back(std::move(item));
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.name > b.name; });
    return files;
}

bool Writer::open(const std::filesystem::path& path) {
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_) return false;
    put32(file_, 0xa1b2c3d4);
    const std::array<char, 4> version{{2, 0, 4, 0}};
    file_.write(version.data(), version.size());
    put32(file_, 0); // timezone
    put32(file_, 0); // timestamp accuracy
    put32(file_, 65535);
    put32(file_, 127); // LINKTYPE_IEEE802_11_RADIOTAP
    return file_.good();
}

bool Writer::append(const uint8_t* data, std::size_t length, std::uint64_t timestamp_us) {
    if (!file_ || !data || length == 0 || length > 65535) return false;
    put32(file_, static_cast<std::uint32_t>(timestamp_us / 1000000));
    put32(file_, static_cast<std::uint32_t>(timestamp_us % 1000000));
    put32(file_, static_cast<std::uint32_t>(length));
    put32(file_, static_cast<std::uint32_t>(length));
    file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(length));
    return file_.good();
}

void Writer::close() { file_.close(); }
bool Writer::flush() { file_.flush(); return file_.good(); }

Capture::~Capture() { stop(); }

std::string Capture::start(int socket, unsigned reserve_mib, Filter filter) {
#if defined(__linux__) && !USE_DESKTOP
    if (active_) { close(socket); return "Capture already active"; }
    if (filter.scope != Scope::All && (filter.mac.empty() || filter.frequency_mhz <= 0))
        { close(socket); return "Invalid capture target"; }
    if (thread_.joinable()) thread_.join();
    std::error_code error;
    const auto root = directory();
    std::filesystem::create_directories(root, error);
    if (error) { close(socket); return "Cannot create PCAP directory"; }
    const auto space = std::filesystem::space(root, error);
    if (error || space.available < (static_cast<std::uint64_t>(reserve_mib) + 4) * 1048576ULL)
        { close(socket); return "Storage reserve reached"; }
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::ostringstream name;
    const std::string prefix = filter.scope == Scope::AccessPoint ? "ap_" :
                               filter.scope == Scope::Station ? "station_" : "capture_";
    std::string identity = filter.mac;
    identity.erase(std::remove(identity.begin(), identity.end(), ':'), identity.end());
    name << prefix << (filter.scope == Scope::All ? "" : identity + "_")
         << local_stamp(std::chrono::system_clock::to_time_t(now), "%Y%m%d_%H%M%S")
         << '_' << std::setw(3) << std::setfill('0') << millis << ".pcap";
    auto file = root / name.str();
    for (unsigned suffix = 1; std::filesystem::exists(file, error) && suffix <= 99; ++suffix)
        file = root / (name.str().substr(0, name.str().size() - 5) + "_" + std::to_string(suffix) + ".pcap");
    if (error || std::filesystem::exists(file)) { close(socket); return "Cannot name PCAP file"; }
    Writer writer;
    if (!writer.open(file)) { close(socket); return "Cannot open PCAP file"; }
    path_ = file.string();
    filter_ = std::move(filter);
    frames_ = 0;
    failed_ = false;
    error_.clear();
    stop_ = false;
    active_ = true;
    try {
        thread_ = std::thread([this, socket, reserve_mib, writer = std::move(writer), root, filter = filter_]() mutable {
            std::array<uint8_t, 65535> buffer{};
            unsigned check = 0;
            while (!stop_) {
                pollfd ready{socket, POLLIN, 0};
                const int result = poll(&ready, 1, 150);
                if (result < 0) { if (errno == EINTR) continue; error_ = "PCAP socket failed"; break; }
                if (result == 0) continue;
                if (ready.revents & (POLLERR | POLLHUP | POLLNVAL)) { error_ = "PCAP socket closed"; break; }
                if (!(ready.revents & POLLIN)) continue;
                const auto count = read(socket, buffer.data(), buffer.size());
                if (count <= 0) { error_ = "PCAP read failed"; break; }
                if (!matches_packet(buffer.data(), static_cast<std::size_t>(count), filter)) continue;
                const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (!writer.append(buffer.data(), static_cast<std::size_t>(count), static_cast<std::uint64_t>(stamp)))
                    { error_ = "PCAP write failed"; break; }
                ++frames_;
                if (++check >= 64) {
                    check = 0;
                    if (!writer.flush()) { error_ = "PCAP write failed"; break; }
                    std::error_code space_error;
                    const auto available = std::filesystem::space(root, space_error).available;
                    if (space_error || available < (static_cast<std::uint64_t>(reserve_mib) + 4) * 1048576ULL)
                        { error_ = "Storage reserve reached"; break; }
                }
            }
            writer.close();
            close(socket);
            if (!stop_ && !error_.empty()) failed_ = true;
            active_ = false;
        });
    } catch (const std::system_error&) {
        active_ = false;
        writer.close();
        close(socket);
        return "Cannot start PCAP writer";
    }
    return {};
#else
    (void)socket; (void)reserve_mib;
    return "Capture available on Cardputer only";
#endif
}

std::string Capture::stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    active_ = false;
    return path_;
}

std::string Capture::take_error() {
    if (!failed_.exchange(false)) return {};
    if (thread_.joinable()) thread_.join();
    return error_;
}

} // namespace platform::pcap
