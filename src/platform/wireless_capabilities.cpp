/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wireless_capabilities.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <filesystem>
#include <sys/wait.h>

namespace platform::wireless {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string frequency_bands(const std::string& info) {
    struct Band { int low, high; const char* name; bool seen; };
    Band bands[] = {{750, 999, "Sub-1", false}, {2400, 2500, "2.4", false},
                    {4900, 5924, "5", false}, {5925, 7125, "6", false},
                    {57000, 71000, "60", false}};
    std::istringstream lines(info);
    for (std::string line; std::getline(lines, line);) {
        std::istringstream fields(line);
        std::string star, unit;
        int frequency;
        if (!(fields >> star >> frequency >> unit) || star != "*" || unit != "MHz") continue;
        for (auto& band : bands)
            if (frequency >= band.low && frequency <= band.high) band.seen = true;
    }
    std::string result;
    for (const auto& band : bands) {
        if (!band.seen) continue;
        if (!result.empty()) result += " / ";
        result += band.name;
    }
    return result.empty() ? result : result + " GHz";
}

bool safe_phy_name(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
    });
}

std::string iw_binary() {
    for (const auto* path : {"/usr/sbin/iw", "/usr/bin/iw", "/sbin/iw", "/bin/iw"}) {
        if (std::filesystem::is_regular_file(path)) return path;
    }
    return {};
}

} // namespace

CapabilityState monitor_mode_from_iw_phy_info(std::string_view output) {
    bool in_modes = false;
    bool saw_mode = false;
    std::size_t offset = 0;
    while (offset <= output.size()) {
        const auto end = output.find('\n', offset);
        const auto length = end == std::string_view::npos ? output.size() - offset : end - offset;
        auto line = trim(std::string(output.substr(offset, length)));
        if (!in_modes) {
            in_modes = line == "Supported interface modes:";
        } else if (!line.empty() && line.front() == '*') {
            saw_mode = true;
            if (trim(line.substr(1)) == "monitor") return CapabilityState::Supported;
        } else if (!line.empty()) {
            return saw_mode ? CapabilityState::Unsupported : CapabilityState::Unknown;
        }
        if (end == std::string_view::npos) break;
        offset = end + 1;
    }
    return in_modes && saw_mode ? CapabilityState::Unsupported : CapabilityState::Unknown;
}

MonitorModeProbe probe_monitor_mode(const std::string& phy_name) {
    if (!safe_phy_name(phy_name)) {
        return {CapabilityState::Unknown, "PHY query unavailable; radio unchanged"};
    }
    const auto binary = iw_binary();
    if (binary.empty()) {
        return {CapabilityState::Unknown, "iw unavailable; radio unchanged"};
    }

    const auto command = binary + " phy " + phy_name + " info 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {CapabilityState::Unknown, "PHY query failed; radio unchanged"};

    std::string output;
    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output.append(buffer.data());
        if (output.size() > 256 * 1024) break;
    }
    const int status = pclose(pipe);
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return {CapabilityState::Unknown, "PHY query failed; radio unchanged"};
    }

    const auto state = monitor_mode_from_iw_phy_info(output);
    if (state == CapabilityState::Supported) {
        return {state, "Kernel advertises monitor mode", frequency_bands(output)};
    }
    if (state == CapabilityState::Unsupported) {
        return {state, "Kernel lacks monitor mode", frequency_bands(output)};
    }
    return {state, "PHY modes unknown; radio unchanged", frequency_bands(output)};
}

} // namespace platform::wireless
