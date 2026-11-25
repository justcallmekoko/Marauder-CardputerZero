/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wireless_scan.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <unordered_map>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#endif

namespace platform::wireless {
namespace {

bool safe_interface_name(const std::string& value) {
    return !value.empty() && value.size() <= 32 &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
           });
}

std::string sanitize_text(std::string_view value) {
    std::string clean;
    clean.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        const auto byte = static_cast<unsigned char>(value[index]);
        if (byte < 0x20 || byte == 0x7f) {
            clean.push_back(' ');
            ++index;
            continue;
        }
        if (byte < 0x80) {
            clean.push_back(static_cast<char>(byte));
            ++index;
            continue;
        }
        std::size_t length = 0;
        if ((byte & 0xe0) == 0xc0) length = 2;
        else if ((byte & 0xf0) == 0xe0) length = 3;
        else if ((byte & 0xf8) == 0xf0) length = 4;
        bool valid = length > 0 && index + length <= value.size();
        for (std::size_t offset = 1; valid && offset < length; ++offset) {
            valid = (static_cast<unsigned char>(value[index + offset]) & 0xc0) == 0x80;
        }
        if (!valid) {
            clean.push_back('?');
            ++index;
            continue;
        }
        clean.append(value.substr(index, length));
        index += length;
    }
    return clean;
}

#if !defined(_WIN32)
std::string registered_vendor(const std::string& address) {
    std::string digits;
    for (unsigned char c : address) {
        if (c == ':') continue;
        if (!std::isxdigit(c)) return "Unknown";
        digits.push_back(static_cast<char>(std::toupper(c)));
    }
    if (digits.size() != 12) return "Unknown";
    if (std::string_view("2367ABEF").find(digits[1]) != std::string_view::npos)
        return "Private/local address";

    static const auto names = [] {
        std::unordered_map<std::string, std::string> prefixes;
        for (const char* file : {"oui", "mam", "oui36", "iab"}) {
            std::ifstream input(std::string("/usr/share/ieee-data/") + file + ".csv");
            std::string line;
            while (std::getline(input, line)) {
                const auto registry = line.find(',');
                if (registry == std::string::npos) continue;
                const auto end = line.find(',', registry + 1);
                if (end == std::string::npos || end + 1 == line.size()) continue;
                const auto prefix = line.substr(registry + 1, end - registry - 1);
                if (prefix.size() != 6 && prefix.size() != 7 && prefix.size() != 9) continue;
                std::string name;
                const bool quoted = line[end + 1] == '"';
                for (auto i = end + 1 + quoted; i < line.size(); ++i) {
                    if (!quoted && line[i] == ',') break;
                    if (quoted && line[i] == '"') {
                        if (i + 1 == line.size() || line[i + 1] != '"') break;
                        ++i;
                    }
                    name.push_back(line[i]);
                }
                if (!name.empty()) prefixes.emplace(prefix, sanitize_text(name));
            }
        }
        return prefixes;
    }();
    for (auto length : {9, 7, 6}) {
        const auto it = names.find(digits.substr(0, length));
        if (it != names.end()) return it->second;
    }
    return "Unknown";
}
#endif
} // namespace

namespace {
std::string field(std::string_view reply, std::string_view key) {
    while (!reply.empty()) {
        const auto end = reply.find('\n');
        const auto line = reply.substr(0, end);
        if (line.size() > key.size() && line.substr(0, key.size()) == key && line[key.size()] == '=')
            return sanitize_text(line.substr(key.size() + 1));
        if (end == reply.npos) break;
        reply.remove_prefix(end + 1);
    }
    return {};
}

bool number(const std::string& text, int& value) {
    auto end = text.data() + text.size();
    auto result = std::from_chars(text.data(), end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool mac_address(std::string_view s) {
    if (s.size() != 17) return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (i % 3 == 2 ? s[i] != ':' : !std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

#if defined(__linux__)
class Control {
public:
    explicit Control(const std::string& path) {
        fd_ = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (fd_ < 0 || bind(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(sa_family_t)) < 0) return;
        if (path.size() >= sizeof(address.sun_path)) return;
        std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
        ready = connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    }
    ~Control() { if (fd_ >= 0) close(fd_); }
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    std::string receive(int timeout_ms) {
        pollfd p{fd_, POLLIN, 0};
        if (poll(&p, 1, timeout_ms) <= 0) return "CONTROL-ERROR";
        std::array<char, 32768> data;
        const auto n = recv(fd_, data.data(), data.size(), MSG_TRUNC);
        return n >= 0 && static_cast<size_t>(n) <= data.size() ? std::string(data.data(), n) : "CONTROL-ERROR";
    }
    std::string query(const std::string& command) {
        if (!ready || send(fd_, command.data(), command.size(), 0) != static_cast<ssize_t>(command.size())) return "CONTROL-ERROR";
        return receive(1200);
    }
    bool ready{};
private:
    int fd_{-1};
};
#endif
}

ConnectionUse connection_use(const std::string& interface_name) {
#if defined(__linux__)
    if (safe_interface_name(interface_name)) {
        Control client("/run/wpa_supplicant/" + interface_name);
        const auto reply = client.query("STATUS");
        const auto state = field(reply, "wpa_state");
        if (field(reply, "mode") == "AP") return ConnectionUse::Hotspot;
        if (state == "COMPLETED" || state == "ASSOCIATED" || state == "4WAY_HANDSHAKE" ||
            state == "GROUP_HANDSHAKE") return ConnectionUse::Connected;
        if (!state.empty()) return ConnectionUse::Idle;
        Control hotspot("/run/hostapd/" + interface_name);
        if (field(hotspot.query("STATUS"), "state") == "ENABLED") return ConnectionUse::Hotspot;
    }
#endif
    return ConnectionUse::Unknown;
}

bool parse_access_point(std::string_view reply, WifiNetwork& ap) {
    ap.bssid = field(reply, "bssid");
    if (!mac_address(ap.bssid) || !number(field(reply, "freq"), ap.frequency_mhz) ||
        !number(field(reply, "level"), ap.rssi)) return false;
    ap.ssid = field(reply, "ssid");
    if (ap.ssid.empty()) ap.ssid = "<hidden>";
    const int f = ap.frequency_mhz;
    ap.channel = f == 2484 ? 14 : f >= 2412 && f <= 2472 ? (f-2407)/5 :
                 f == 5935 ? 2 : f >= 5955 && f <= 7115 ? (f-5950)/5 :
                 f >= 5000 && f < 5925 ? (f-5000)/5 : 0;
    ap.signal_percent = std::clamp(2 * (std::clamp(ap.rssi,-110,0) + 100), 0, 100);
    const auto flags = field(reply, "flags");
    ap.security = flags.empty() ? "Unknown" : flags;
    return true;
}

WifiScanResult scan_wifi_networks(const std::string& interface_name,
                                 const std::function<void(WifiNetwork)>& discovered) {
    if (!safe_interface_name(interface_name)) return {{}, "Invalid adapter name"};
#if !defined(__linux__)
    return {{}, "WiFi inventory requires Linux"};
#else
    const std::string path = "/run/wpa_supplicant/" + interface_name;
    Control wifi(path);
    {
        if (!wifi.ready) return {{}, "WiFi service unavailable"};
        Control events(path);
        if (events.query("ATTACH") != "OK\n") return {{}, "Cannot watch WiFi scan"};
        const auto scan = wifi.query("SCAN");
        if (scan != "OK\n" && scan.find("FAIL-BUSY") == scan.npos) return {{}, "Scan unavailable"};
        // Existing service records are available while the radio runs its new scan.
        if (discovered) {
            auto reply = wifi.query("BSS FIRST");
            for (size_t i = 0; i < 1024; ++i) {
                WifiNetwork ap;
                if (!parse_access_point(reply, ap)) break;
                discovered(std::move(ap));
                const auto id = field(reply, "id");
                if (id.empty()) break;
                reply = wifi.query("BSS NEXT-" + id);
            }
        }
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(12);
        for (;;) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - std::chrono::steady_clock::now()).count();
            if (left <= 0) return {{}, "Scan timed out"};
            const auto event = events.receive(static_cast<int>(left));
            const std::string marker = "CTRL-EVENT-BSS-ADDED ";
            const auto added = event.find(marker);
            if (discovered && added != event.npos) {
                const auto begin = added + marker.size();
                const auto end = event.find(' ', begin);
                int id{};
                if (end != event.npos && number(event.substr(begin, end - begin), id) && id >= 0) {
                    WifiNetwork ap;
                    if (parse_access_point(wifi.query("BSS ID-" + std::to_string(id)), ap))
                        discovered(std::move(ap));
                }
            }
            if (event.find("CTRL-EVENT-SCAN-RESULTS") != event.npos) break;
            if (event.empty() || event.find("CTRL-EVENT-SCAN-FAILED") != event.npos) return {{}, "Scan did not complete"};
        }
    }
    WifiScanResult result;
    std::string next = "BSS FIRST";
    for (size_t i = 0; i < 1024; ++i) {
        const auto reply = wifi.query(next);
        if (reply == "FAIL\n" || reply == "\n") break;
        // wpa_supplicant signals the end of its BSS table with an empty datagram.
        if (reply.empty()) break;
        WifiNetwork item;
        if (!parse_access_point(reply, item))
            return {{}, "Cannot read radio inventory"};
        if (discovered) discovered(item);
        item.vendor = registered_vendor(item.bssid);
        next = "BSS NEXT-" + field(reply,"id");
        result.networks.push_back(std::move(item));
    }
    return result;
#endif
}

} // namespace platform::wireless
