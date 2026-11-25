// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include "wifi_security.h"

namespace platform::wireless {
struct FrameObservation {
    std::string bssid;
    std::string client;
    std::string ssid;
    std::string security;
    SecurityInfo security_info;
    int frequency_mhz{};
    int rssi{};
    bool has_rssi{};
    bool client_transmitted{};
    bool beacon{};
};

// Parse only 802.11 header and management tags. Payload bytes are never retained.
bool parse_monitor_frame(const uint8_t* bytes, std::size_t size, FrameObservation& out);
}
