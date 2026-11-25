/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace platform::wireless {

enum class ConnectionUse { Idle, Connected, Hotspot, Unknown };
ConnectionUse connection_use(const std::string& interface_name);

struct WifiNetwork {
    std::string bssid;
    std::string ssid;
    int channel{0};
    int frequency_mhz{0};
    int signal_percent{0};
    std::string security;
    std::string vendor{};
    int rssi{0};
    std::string rx_packets{}, tx_packets{};
};

struct WifiScanResult {
    std::vector<WifiNetwork> networks;
    std::string error;
};

bool parse_access_point(std::string_view reply, WifiNetwork& ap);
bool parse_hotspot_station(std::string_view reply, WifiNetwork& station);
WifiScanResult scan_wifi_networks(const std::string& interface_name, bool stations = false);

} // namespace platform::wireless
