// SPDX-License-Identifier: AGPL-3.0-only
#include "raw_wifi.h"
#include <cassert>
#include <cstdint>
#include <vector>

using platform::wireless::FrameObservation;
using platform::wireless::parse_monitor_frame;
using platform::wireless::parse_frame_summary;
using platform::wireless::frame_matches_target;
using platform::wireless::FrameKind;

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
    beacon.insert(beacon.end(), {0, 4, 'T', 'e', 's', 't', 3, 1, 1,
        48, 20, 1, 0, 0, 15, 172, 4, 1, 0, 0, 15, 172, 4,
        1, 0, 0, 15, 172, 2, 0, 0});
    FrameObservation observed;
    assert(parse_monitor_frame(beacon.data(), beacon.size(), observed));
    assert(observed.beacon && observed.bssid == "10:20:30:40:50:60");
    assert(observed.ssid == "Test" && observed.security == "WPA2 Personal");
    assert(observed.security_info.akm == "PSK" && observed.security_info.pairwise == "CCMP-128");
    assert(observed.frequency_mhz == 2437 && observed.has_rssi && observed.rssi == -48);
    assert(observed.advertised_frequency_mhz == 2412);
    auto ht_beacon = beacon;
    ht_beacon[13 + 36 + 6] = 61; // HT Operation primary channel, without DS Parameters.
    ht_beacon[13 + 36 + 8] = 36;
    assert(parse_monitor_frame(ht_beacon.data(), ht_beacon.size(), observed));
    assert(observed.advertised_frequency_mhz == 5180);
    platform::wireless::FrameSummary summary;
    assert(parse_frame_summary(beacon.data(), beacon.size(), summary));
    assert(summary.type == "Beacon" && summary.sender == observed.bssid);
    assert(summary.kind == FrameKind::Beacon);
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
    assert(summary.kind == FrameKind::Data);
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
    assert(summary.kind == FrameKind::Control);
    auto sae = beacon;
    sae[2] = 13;
    sae[13] = 0xB0; // Authentication subtype, SAE algorithm 3.
    sae[13 + 24] = 3;
    sae[13 + 25] = 0;
    assert(parse_frame_summary(sae.data(), sae.size(), summary));
    assert(summary.type == "SAE auth" && summary.kind == FrameKind::Sae);
    auto eapol = uplink;
    eapol[13] = 0x08;
    eapol[14] = 0x01;
    eapol.resize(13 + 24);
    eapol.insert(eapol.end(), {0xAA, 0xAA, 0x03, 0, 0, 0, 0x88, 0x8E});
    eapol.resize(13 + 24 + 8 + 99 + 22, 0);
    const auto key = 13 + 24 + 8;
    eapol[key + 1] = 3; // EAPOL-Key.
    eapol[key + 3] = 117; // Descriptor body plus 22 bytes of key data.
    eapol[key + 4] = 2; // RSN key descriptor.
    eapol[key + 6] = 0x08; // Pairwise key.
    eapol[key + 98] = 22; // Key data length.
    eapol[key + 99] = 221; eapol[key + 100] = 20;
    eapol[key + 102] = 0x0F; eapol[key + 103] = 0xAC; eapol[key + 104] = 4;
    assert(parse_frame_summary(eapol.data(), eapol.size(), summary));
    assert(summary.type == "PMKID" && summary.kind == FrameKind::Eapol);
    eapol[key + 104] = 0;
    assert(parse_frame_summary(eapol.data(), eapol.size(), summary));
    assert(summary.type == "EAPOL" && summary.kind == FrameKind::Eapol);
    eapol[14] |= 0x40; // Protected data must not be inspected as clear EAPOL.
    assert(parse_frame_summary(eapol.data(), eapol.size(), summary));
    assert(summary.kind == FrameKind::Data);
    auto key_frame = [&](unsigned info, bool from_ap, unsigned replay) {
        auto frame = eapol;
        frame.resize(13 + 24 + 8 + 99);
        frame[13] = 0x08;
        frame[14] = from_ap ? 0x02 : 0x01;
        for (int i = 0; i < 6; ++i) {
            frame[13 + 4 + i] = from_ap ? client[i] : ap[i];
            frame[13 + 10 + i] = from_ap ? ap[i] : client[i];
            frame[13 + 16 + i] = from_ap ? ap[i] : client[i];
        }
        frame[key + 3] = 95;
        frame[key + 5] = static_cast<uint8_t>(info >> 8);
        frame[key + 6] = static_cast<uint8_t>(info);
        frame[key + 16] = static_cast<uint8_t>(replay);
        frame[key + 98] = 0;
        for (int i = 0; i < 32; ++i) frame[key + 17 + i] = static_cast<uint8_t>(i + 1);
        return frame;
    };
    platform::wireless::HandshakeState handshake;
    for (const auto message : {1u, 2u, 3u, 4u}) {
        const auto frame = key_frame(message == 1 ? 0x0088 : message == 2 ? 0x0108 :
                                     message == 3 ? 0x03C8 : 0x0308,
                                     message == 1 || message == 3, message <= 2 ? 5 : 6);
        assert(parse_monitor_frame(frame.data(), frame.size(), observed));
        assert(observed.client == "22:33:44:55:66:77" && observed.eapol.message == message);
        assert(parse_frame_summary(frame.data(), frame.size(), summary));
        assert(summary.type == "EAPOL M" + std::to_string(message));
        handshake.observe(observed.eapol, observed.client_transmitted);
        assert(handshake.stage == message);
        assert(handshake.complete == (message == 4));
    }
    platform::wireless::HandshakeState mismatched;
    for (const auto message : {1u, 2u, 3u}) {
        const auto frame = key_frame(message == 1 ? 0x0088 : message == 2 ? 0x0108 : 0x03C8,
                                     message == 1 || message == 3, message <= 2 ? 5 : 6);
        assert(parse_monitor_frame(frame.data(), frame.size(), observed));
        mismatched.observe(observed.eapol, observed.client_transmitted);
    }
    const auto wrong_fourth = key_frame(0x0308, false, 7);
    assert(parse_monitor_frame(wrong_fourth.data(), wrong_fourth.size(), observed));
    mismatched.observe(observed.eapol, observed.client_transmitted);
    assert(!mismatched.complete && mismatched.stage == 3);
    auto pmkid_frame = key_frame(0x0088, true, 9);
    pmkid_frame.resize(13 + 24 + 8 + 99 + 22, 0);
    pmkid_frame[key + 3] = 117;
    pmkid_frame[key + 98] = 22;
    pmkid_frame[key + 99] = 221;
    pmkid_frame[key + 100] = 20;
    pmkid_frame[key + 102] = 0x0F;
    pmkid_frame[key + 103] = 0xAC;
    pmkid_frame[key + 104] = 4;
    assert(parse_monitor_frame(pmkid_frame.data(), pmkid_frame.size(), observed));
    assert(observed.eapol.message == 1 && observed.eapol.pmkid);
    platform::wireless::HandshakeState pmkid_state;
    pmkid_state.observe(observed.eapol, observed.client_transmitted);
    assert(pmkid_state.pmkid_seen && !pmkid_state.complete);
    assert(!parse_monitor_frame(beacon.data(), 7, observed));
    beacon[2] = 0xff;
    assert(!parse_monitor_frame(beacon.data(), beacon.size(), observed));
}
