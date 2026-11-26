// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include "wifi_security.h"

namespace platform::wireless {
enum class FrameKind : uint8_t {
    Beacon, Probe, Deauth, Eapol, Sae, Data, Association,
    Authentication, Action, Control, Other, Count
};
const char* frame_kind_label(FrameKind kind);
uint32_t frame_kind_bit(FrameKind kind);
struct FrameObservation {
    std::string bssid;
    std::string client;
    std::string ssid;
    std::string security;
    SecurityInfo security_info;
    int frequency_mhz{};
    int advertised_frequency_mhz{};
    int rssi{};
    bool has_rssi{};
    bool client_transmitted{};
    bool beacon{};
};

struct FrameSummary {
    std::string type;
    FrameKind kind{FrameKind::Other};
    std::string sender;
    std::string receiver;
    int frequency_mhz{};
    int rssi{};
    bool has_rssi{};
};

// Extract only the MAC header for the live, address-filtered frame feed.
bool parse_frame_summary(const uint8_t* bytes, std::size_t size, FrameSummary& out);
bool frame_matches_target(const FrameSummary& frame, std::string_view mac, int frequency_mhz);

// Parse only 802.11 header and management tags. Payload bytes are never retained.
bool parse_monitor_frame(const uint8_t* bytes, std::size_t size, FrameObservation& out);
}
