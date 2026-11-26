// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace platform::wireless {
struct PineappleObservation {
    std::string mac;
    std::string ssid;
    std::string vendor;
    std::string evidence;
    std::string security;
    int frequency_mhz{};
    int rssi{};
    bool has_rssi{};
    uint8_t clues{};
    uint32_t beacons{};
    uint32_t last_seen_ms{};
};

// Marauder-inspired beacon heuristics. A match is a candidate, never proof of hardware identity.
bool detect_pineapple_beacon(const uint8_t* bytes, std::size_t size, PineappleObservation& out);
}
