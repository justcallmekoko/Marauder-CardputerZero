// SPDX-License-Identifier: AGPL-3.0-only
#include "pineapple_wifi.h"
#include "raw_wifi.h"
#include <cstdio>

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
std::string mac_text(const uint8_t* mac) {
    char text[18];
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return text;
}
}

bool detect_pineapple_beacon(const uint8_t* bytes, std::size_t size, PineappleObservation& out) {
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
    if (!matched_vendor && !short_beacon) return false;
    FrameObservation beacon;
    if (!parse_monitor_frame(bytes, size, beacon) || !beacon.beacon) return false;
    out.mac = mac_text(frame + 10);
    out.ssid = beacon.ssid.empty() ? "[hidden/unknown]" : beacon.ssid;
    out.vendor = matched_vendor ? matched_vendor : "Unknown";
    out.clues = static_cast<uint8_t>((matched_vendor ? 1 : 0) | (short_beacon ? 2 : 0));
    out.evidence = out.clues == 3 ? "OUI + beacon pattern" :
                   matched_vendor ? "Marauder OUI clue" : "Short beacon pattern";
    out.security = beacon.security;
    out.frequency_mhz = beacon.advertised_frequency_mhz ? beacon.advertised_frequency_mhz : frequency;
    out.rssi = rssi;
    out.has_rssi = has_rssi;
    return true;
}
}
