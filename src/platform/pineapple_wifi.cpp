// SPDX-License-Identifier: AGPL-3.0-only
#include "pineapple_wifi.h"
#include "raw_wifi.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <string_view>

namespace platform::wireless {
namespace {
struct VendorClue { uint32_t oui; const char* vendor; uint8_t security; };
// 1 = open, 2 = protected, 3 = either. Derived from ESP32 Marauder PineScan.
constexpr VendorClue clues[] = {
    {0x00C0CA, "Alfa Inc", 1},
    {0x001337, "Orient Power Home Network Ltd", 3},
    {0x1CBFCE, "Shenzhen Century Xinyang Technology Co Ltd", 1},
    {0x0CEFAF, "IEEE Registration Authority", 1},
    {0x02C0CA, "Hak5 local address", 2}, {0x021337, "Hak5 local address", 2},
    {0x000A00, "MediaTek Inc", 3}, {0x000C43, "MediaTek Inc", 3},
    {0x000CE7, "MediaTek Inc", 3}, {0x0017A5, "MediaTek Inc", 3},
    {0x9CEFD5, "Panda Wireless Inc", 3}, {0x9CE5D5, "Panda Wireless Inc", 3},
    {0xDEADBE, "Unassigned / spoofed", 3},
};
constexpr uint32_t kPagerOui = 0x061337;
constexpr uint32_t kBehaviorWindowMs = 60000;
constexpr std::size_t kBehaviorSsidThreshold = 5;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
bool pager_ssid(std::string_view ssid) {
    // Defaults corroborated by the MIT-licensed PineDetect project and Hak5's official Find Hackers payload.
    const auto name = lower(std::string(ssid));
    return name == "pager" || name == "pager-open" || name == "pager_open" ||
           name == "pager-wpa" || name == "pinepager" || name.rfind("pineapple_", 0) == 0;
}
std::string mac_text(const uint8_t* mac) {
    char text[18];
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return text;
}
}

std::string pineapple_evidence(uint8_t clues) {
    if ((clues & PineappleMultiSsid) && (clues & (PineapplePagerSsid | PineapplePagerAddress)))
        return "Pager clue + multi-SSID behavior";
    if ((clues & PineapplePagerSsid) && (clues & PineapplePagerAddress)) return "Pager SSID + address clue";
    if (clues & PineapplePagerAddress) return "Pineapple Pager address clue";
    if (clues & PineapplePagerSsid) return "Pineapple Pager SSID clue";
    if (clues & PineappleMultiSsid) return "PineAP-style multi-SSID behavior";
    if ((clues & PineappleLegacyAddress) && (clues & PineappleShortBeacon)) return "OUI + beacon pattern";
    if (clues & PineappleLegacyAddress) return "Marauder OUI clue";
    if (clues & PineappleShortBeacon) return "Short beacon pattern";
    return "Unclassified beacon";
}

bool inspect_pineapple_beacon(const uint8_t* bytes, std::size_t size, PineappleObservation& out) {
    out = {};
    std::size_t header{};
    int frequency{}, rssi{};
    bool has_rssi{};
    if (!monitor_header(bytes, size, header, frequency, rssi, has_rssi) ||
        size - header < 38) return false;
    const auto* frame = bytes + header;
    const auto length = size - header;
    if (frame[0] != 0x80 || frame[1] != 0) return false;
    const auto capability = static_cast<unsigned>(frame[34]) | (static_cast<unsigned>(frame[35]) << 8);
    const bool protected_ap = (capability & 0x10) != 0;
    const auto oui = (static_cast<uint32_t>(frame[10]) << 16) |
                     (static_cast<uint32_t>(frame[11]) << 8) | frame[12];
    const char* matched_vendor = nullptr;
    for (const auto& clue : clues) {
        if (clue.oui == oui && (clue.security == 3 || clue.security == (protected_ap ? 2 : 1))) {
            matched_vendor = clue.vendor;
            break;
        }
    }
    // Marauder's second clue is a capability 0x0001 beacon with only SSID + DS tags.
    bool short_beacon = false;
    if (capability == 0x0001 && frame[36] == 0 && 38u + frame[37] <= length) {
        const auto ds = static_cast<std::size_t>(38u + frame[37]);
        short_beacon = ds + 3 <= length && frame[ds] == 3 && frame[ds + 1] == 1 &&
                       (ds + 3 == length || ds + 7 == length); // Some drivers retain a 4-byte FCS.
    }
    FrameObservation beacon;
    if (!parse_monitor_frame(bytes, size, beacon) || !beacon.beacon) return false;
    out.mac = mac_text(frame + 10);
    out.ssid = beacon.ssid.empty() ? "[hidden/unknown]" : beacon.ssid;
    const bool pager_address = oui == kPagerOui;
    const bool pager_name = pager_ssid(out.ssid);
    out.vendor = pager_address ? "Hak5 local address" : matched_vendor ? matched_vendor : "Unknown";
    out.clues = static_cast<uint8_t>((matched_vendor ? PineappleLegacyAddress : 0) |
                                     (short_beacon ? PineappleShortBeacon : 0) |
                                     (pager_name ? PineapplePagerSsid : 0) |
                                     (pager_address ? PineapplePagerAddress : 0));
    out.evidence = pineapple_evidence(out.clues);
    out.security = beacon.security;
    out.frequency_mhz = beacon.advertised_frequency_mhz ? beacon.advertised_frequency_mhz : frequency;
    out.rssi = rssi;
    out.has_rssi = has_rssi;
    return true;
}

bool detect_pineapple_beacon(const uint8_t* bytes, std::size_t size, PineappleObservation& out) {
    return inspect_pineapple_beacon(bytes, size, out) && out.clues != 0;
}

bool PineappleBehaviorTracker::observe(PineappleObservation& observation, uint32_t now_ms) {
    auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const auto& candidate) {
        return candidate.mac == observation.mac;
    });
    if (entry == entries_.end()) {
        if (entries_.size() >= 128) entries_.erase(entries_.begin());
        entries_.push_back({observation.mac, {}});
        entry = std::prev(entries_.end());
    }
    auto& names = entry->ssids;
    names.erase(std::remove_if(names.begin(), names.end(), [&](const auto& seen) {
        return now_ms - seen.second > kBehaviorWindowMs;
    }), names.end());
    if (!observation.ssid.empty() && observation.ssid != "[hidden/unknown]") {
        const auto name = lower(observation.ssid);
        auto found = std::find_if(names.begin(), names.end(), [&](const auto& seen) { return seen.first == name; });
        if (found == names.end()) {
            if (names.size() >= 16) names.erase(names.begin());
            names.emplace_back(name, now_ms);
        } else found->second = now_ms;
    }
    if (names.size() >= kBehaviorSsidThreshold) observation.clues |= PineappleMultiSsid;
    observation.evidence = pineapple_evidence(observation.clues);
    return observation.clues != 0;
}
}
