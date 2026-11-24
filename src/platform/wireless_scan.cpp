/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wireless_scan.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>

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

bool parse_int(const std::string& value, int minimum, int maximum, int& parsed) {
    if (value.empty()) return false;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
           parsed >= minimum && parsed <= maximum;
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
                parse_int(fields[3], 0, 100000, network.frequency_mhz) &&
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
    return parse_nmcli_wifi_scan(output);
#endif
}

} // namespace platform::wireless
