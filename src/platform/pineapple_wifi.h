// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

enum PineappleClue : uint8_t {
    PineappleLegacyAddress = 1u << 0,
    PineappleShortBeacon = 1u << 1,
    PineapplePagerSsid = 1u << 2,
    PineapplePagerAddress = 1u << 3,
    PineappleMultiSsid = 1u << 4,
};

// Parse any valid beacon into an observation and attach static Pineapple clues.
bool inspect_pineapple_beacon(const uint8_t* bytes, std::size_t size, PineappleObservation& out);

// Marauder-inspired beacon heuristics. A match is a candidate, never proof of hardware identity.
bool detect_pineapple_beacon(const uint8_t* bytes, std::size_t size, PineappleObservation& out);

std::string pineapple_evidence(uint8_t clues);

class PineappleBehaviorTracker {
public:
    bool observe(PineappleObservation& observation, uint32_t now_ms);
    void clear() { entries_.clear(); }

private:
    struct Entry {
        std::string mac;
        std::vector<std::pair<std::string, uint32_t>> ssids;
    };
    std::vector<Entry> entries_;
};
}
