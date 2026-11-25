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
#include <filesystem>
#include <sys/wait.h>
#endif

namespace platform::wireless {
namespace {

bool safe_interface_name(const std::string& value) {
    return !value.empty() && value.size() <= 32 &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
           });
}

bool parse_int(const std::string& value, int minimum, int maximum, int& parsed, bool allow_mhz = false) {
    if (value.empty()) return false;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || parsed < minimum || parsed > maximum) return false;
    std::string_view suffix(result.ptr, static_cast<std::size_t>(value.data() + value.size() - result.ptr));
    while (!suffix.empty() && std::isspace(static_cast<unsigned char>(suffix.front())) != 0) {
        suffix.remove_prefix(1);
    }
    return suffix.empty() || (allow_mhz && suffix == "MHz");
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

std::vector<std::string> split_nmcli_terse_row(std::string_view row) {
    std::vector<std::string> fields;
    std::string field;
    bool escaped = false;
    for (const char ch : row) {
        if (escaped) {
            field.push_back(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == ':') {
            fields.push_back(std::move(field));
            field.clear();
        } else if (ch != '\r') {
            field.push_back(ch);
        }
    }
    if (escaped) field.push_back('\\');
    fields.push_back(std::move(field));
    return fields;
}

WifiScanResult parse_nmcli_wifi_scan(std::string_view output) {
    WifiScanResult result;
    bool saw_nonempty_row = false;
    std::size_t offset = 0;
    while (offset <= output.size()) {
        const auto end = output.find('\n', offset);
        const auto length = end == std::string_view::npos ? output.size() - offset : end - offset;
        const auto row = output.substr(offset, length);
        if (!row.empty()) {
            saw_nonempty_row = true;
            auto fields = split_nmcli_terse_row(row);
            WifiNetwork network;
            if (fields.size() == 6 &&
                parse_int(fields[2], 0, 1000, network.channel) &&
                parse_int(fields[3], 0, 100000, network.frequency_mhz, true) &&
                parse_int(fields[4], 0, 100, network.signal_percent)) {
                network.bssid = sanitize_text(fields[0]);
                network.ssid = fields[1].empty() ? "<hidden>" : sanitize_text(fields[1]);
                network.security = fields[5].empty() ? "OPEN" : sanitize_text(fields[5]);
                result.networks.push_back(std::move(network));
            }
        }
        if (end == std::string_view::npos) break;
        offset = end + 1;
    }
    std::stable_sort(result.networks.begin(), result.networks.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.signal_percent > rhs.signal_percent;
    });
    if (saw_nonempty_row && result.networks.empty()) result.error = "Unexpected Wi-Fi scan output";
    return result;
}

WifiScanResult scan_wifi_networks(const std::string& interface_name) {
    if (!safe_interface_name(interface_name)) return {{}, "Invalid adapter name"};
#if defined(_WIN32)
    return {{}, "Wi-Fi scanning is available on Linux devices"};
#else
    std::error_code filesystem_error;
    const bool has_usr_nmcli = std::filesystem::is_regular_file("/usr/bin/nmcli", filesystem_error);
    filesystem_error.clear();
    const bool has_bin_nmcli = std::filesystem::is_regular_file("/bin/nmcli", filesystem_error);
    const char* binary = has_usr_nmcli ? "/usr/bin/nmcli" : has_bin_nmcli ? "/bin/nmcli" : nullptr;
    if (!binary) return {{}, "NetworkManager tools unavailable"};

    const std::string command = std::string(binary) +
        " --wait 12 --terse --escape yes --fields BSSID,SSID,CHAN,FREQ,SIGNAL,SECURITY"
        " device wifi list ifname " + interface_name + " --rescan yes 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {{}, "Unable to start Wi-Fi scan"};

    std::string output;
    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output.append(buffer.data());
        if (output.size() > 1024 * 1024) break;
    }
    const int status = pclose(pipe);
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return {{}, "Wi-Fi scan failed"};
    }
    auto result = parse_nmcli_wifi_scan(output);
    for (auto& network : result.networks) network.vendor = registered_vendor(network.bssid);
    return result;
#endif
}

} // namespace platform::wireless
