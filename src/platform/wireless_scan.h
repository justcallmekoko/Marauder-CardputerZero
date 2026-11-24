/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace platform::wireless {

struct WifiNetwork {
    std::string bssid;
    std::string ssid;
    int channel{0};
    int frequency_mhz{0};
    int signal_percent{0};
    std::string security;
};

struct WifiScanResult {
    std::vector<WifiNetwork> networks;
    std::string error;
};

std::vector<std::string> split_nmcli_terse_row(std::string_view row);
WifiScanResult parse_nmcli_wifi_scan(std::string_view output);
WifiScanResult scan_wifi_networks(const std::string& interface_name);

} // namespace platform::wireless
