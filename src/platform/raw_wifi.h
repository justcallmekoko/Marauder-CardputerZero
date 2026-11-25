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

struct FrameSummary {
    std::string type;
    std::string sender;
    std::string receiver;
    int frequency_mhz{};
    int rssi{};
    bool has_rssi{};
};

// Extract only the MAC header for the live, address-filtered frame feed.
bool parse_frame_summary(const uint8_t* bytes, std::size_t size, FrameSummary& out);

// Parse only 802.11 header and management tags. Payload bytes are never retained.
bool parse_monitor_frame(const uint8_t* bytes, std::size_t size, FrameObservation& out);
}
