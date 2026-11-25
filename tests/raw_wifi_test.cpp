// SPDX-License-Identifier: AGPL-3.0-only
#include "raw_wifi.h"
#include <cassert>
#include <cstdint>
#include <vector>

using platform::wireless::FrameObservation;
using platform::wireless::parse_monitor_frame;
using platform::wireless::parse_frame_summary;
using platform::wireless::frame_matches_target;

int main() {
    // Radiotap with channel 6 and -48 dBm, followed by a beacon.
    std::vector<uint8_t> beacon{0, 0, 13, 0, 0x28, 0, 0, 0, 0x85, 0x09, 0, 0, 0xd0};
    beacon.resize(13 + 36, 0);
    beacon[13] = 0x80;
    const uint8_t ap[6]{0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    for (int i = 0; i < 6; ++i) {
        beacon[13 + 4 + i] = 0xff;
        beacon[13 + 10 + i] = ap[i];
        beacon[13 + 16 + i] = ap[i];
    }
    beacon[13 + 34] = 0x10;
    beacon.insert(beacon.end(), {0, 4, 'T', 'e', 's', 't',
        48, 20, 1, 0, 0, 15, 172, 4, 1, 0, 0, 15, 172, 4,
        1, 0, 0, 15, 172, 2, 0, 0});
    FrameObservation observed;
    assert(parse_monitor_frame(beacon.data(), beacon.size(), observed));
    assert(observed.beacon && observed.bssid == "10:20:30:40:50:60");
    assert(observed.ssid == "Test" && observed.security == "WPA2 Personal");
    assert(observed.security_info.akm == "PSK" && observed.security_info.pairwise == "CCMP-128");
    assert(observed.frequency_mhz == 2437 && observed.has_rssi && observed.rssi == -48);
    platform::wireless::FrameSummary summary;
    assert(parse_frame_summary(beacon.data(), beacon.size(), summary));
    assert(summary.type == "Beacon" && summary.sender == observed.bssid);
    assert(summary.receiver == "FF:FF:FF:FF:FF:FF");

    std::vector<uint8_t> uplink{0, 0, 13, 0, 0x28, 0, 0, 0, 0x85, 0x09, 0, 0, 0xc4};
    uplink.resize(13 + 24, 0);
    uplink[13] = 0x08; uplink[14] = 0x01; // Data, To DS.
    const uint8_t client[6]{0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    for (int i = 0; i < 6; ++i) { uplink[13 + 4 + i] = ap[i]; uplink[13 + 10 + i] = client[i]; }
    assert(parse_monitor_frame(uplink.data(), uplink.size(), observed));
    assert(observed.client == "22:33:44:55:66:77" && observed.client_transmitted);
    assert(observed.rssi == -60);
    assert(parse_frame_summary(uplink.data(), uplink.size(), summary));
    assert(summary.type == "Data" && summary.sender == observed.client && summary.receiver == observed.bssid);
    assert(frame_matches_target(summary, observed.client, 2437));
    assert(frame_matches_target(summary, observed.bssid, 2437));
    assert(!frame_matches_target(summary, observed.client, 2462));
    assert(!frame_matches_target(summary, "AA:BB:CC:DD:EE:FF", 2437));

    uplink[14] = 0x02; // From DS: AP transmitter, client receiver.
    for (int i = 0; i < 6; ++i) { uplink[13 + 4 + i] = client[i]; uplink[13 + 10 + i] = ap[i]; }
    assert(parse_monitor_frame(uplink.data(), uplink.size(), observed));
    assert(observed.client == "22:33:44:55:66:77" && !observed.client_transmitted);
    assert(parse_frame_summary(uplink.data(), uplink.size(), summary));
    assert(summary.sender == observed.bssid && summary.receiver == observed.client);
    uplink[13] = 0xD4; uplink[14] = 0; uplink.resize(13 + 10);
    assert(parse_frame_summary(uplink.data(), uplink.size(), summary));
    assert(summary.type == "ACK" && summary.sender.empty() && summary.receiver == observed.client);
    assert(!parse_monitor_frame(beacon.data(), 7, observed));
    beacon[2] = 0xff;
    assert(!parse_monitor_frame(beacon.data(), beacon.size(), observed));
}
