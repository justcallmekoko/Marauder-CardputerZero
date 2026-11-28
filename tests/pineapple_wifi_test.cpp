// SPDX-License-Identifier: AGPL-3.0-only
#include "pineapple_wifi.h"
#include "raw_wifi.h"
#include <cassert>
#include <vector>

namespace {
std::vector<uint8_t> beacon(uint8_t a, uint8_t b, uint8_t c, bool protected_ap, bool short_tags,
                            const char* ssid = "Test", uint16_t capability = 0) {
    std::vector<uint8_t> frame(36, 0);
    frame[0] = 0x80;
    frame[10] = a; frame[11] = b; frame[12] = c;
    frame[13] = 0x12; frame[14] = 0x34; frame[15] = 0x56;
    for (int i = 0; i < 6; ++i) frame[16 + i] = frame[10 + i];
    capability = capability ? capability : static_cast<uint16_t>(0x01 | (protected_ap ? 0x10 : 0));
    frame[34] = static_cast<uint8_t>(capability); frame[35] = static_cast<uint8_t>(capability >> 8);
    const auto length = static_cast<uint8_t>(std::char_traits<char>::length(ssid));
    frame.push_back(0); frame.push_back(length);
    frame.insert(frame.end(), ssid, ssid + length);
    frame.insert(frame.end(), {3, 1, 6});
    if (!short_tags) frame.insert(frame.end(), {1, 1, 0x82});
    std::vector<uint8_t> packet{0, 0, 13, 0, 0x28, 0, 0, 0, 0x85, 0x09, 0, 0, 0xD0};
    packet.insert(packet.end(), frame.begin(), frame.end());
    return packet;
}
}
int main() {
    platform::wireless::PineappleObservation candidate;
    auto packet = beacon(0x00, 0x13, 0x37, false, false);
    platform::wireless::FrameObservation ap;
    assert(platform::wireless::parse_monitor_frame(packet.data(), packet.size(), ap));
    assert(platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
    assert(candidate.mac == "00:13:37:12:34:56" && candidate.ssid == "Test");
    assert(candidate.frequency_mhz == 2437 && candidate.has_rssi && candidate.rssi == -48);
    assert(candidate.clues == 1 && candidate.evidence == "Marauder OUI clue");
    platform::wireless::FrameSummary summary;
    assert(platform::wireless::parse_frame_summary(packet.data(), packet.size(), summary));
    assert(summary.kind == platform::wireless::FrameKind::Pineapple);

    packet = beacon(0x02, 0xC0, 0xCA, false, false);
    assert(!platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
    packet = beacon(0x02, 0xC0, 0xCA, true, false);
    assert(platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
    packet = beacon(0x28, 0x34, 0x56, false, false);
    assert(!platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
    packet = beacon(0x28, 0x34, 0x56, false, true);
    assert(platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
    assert(candidate.clues == 2 && candidate.evidence == "Short beacon pattern");

    // Captured Pineapple Pager shape: local 06:13:37 BSSID, ordinary full beacon, pager-open SSID.
    packet = beacon(0x06, 0x13, 0x37, false, false, "pager-open", 0x0421);
    assert(platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
    assert((candidate.clues & platform::wireless::PineapplePagerAddress) != 0);
    assert((candidate.clues & platform::wireless::PineapplePagerSsid) != 0);
    assert(candidate.evidence == "Pager SSID + address clue");

    // Default Pager names remain candidates even when the BSSID is randomized.
    packet = beacon(0x28, 0x34, 0x56, false, false, "pager-wpa", 0x0421);
    assert(platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
    assert(candidate.clues == platform::wireless::PineapplePagerSsid);
    packet = beacon(0x28, 0x34, 0x56, false, false, "my-pager-device", 0x0421);
    assert(!platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));

    // Five distinct SSIDs from one BSSID inside the observation window is PineAP-style behavior.
    platform::wireless::PineappleBehaviorTracker tracker;
    for (unsigned i = 0; i < 5; ++i) {
        const std::string name = "Network-" + std::to_string(i);
        packet = beacon(0x28, 0x34, 0x56, false, false, name.c_str(), 0x0421);
        assert(platform::wireless::inspect_pineapple_beacon(packet.data(), packet.size(), candidate));
        const bool matched = tracker.observe(candidate, 1000 + i * 100);
        assert(matched == (i == 4));
    }
    assert(candidate.evidence == "PineAP-style multi-SSID behavior");
    packet.resize(17);
    assert(!platform::wireless::detect_pineapple_beacon(packet.data(), packet.size(), candidate));
}
