// SPDX-License-Identifier: AGPL-3.0-only
#include "remote_id_wifi.h"
#include "raw_wifi.h"
#include <cassert>
#include <cstring>
#include <vector>

namespace {
void put32(uint8_t* out, int32_t value) {
    const auto raw = static_cast<uint32_t>(value);
    for (int i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(raw >> (i * 8));
}
std::vector<uint8_t> wrapped(const std::vector<uint8_t>& frame) {
    std::vector<uint8_t> bytes{0, 0, 13, 0, 0x28, 0, 0, 0, 0x85, 0x09, 0, 0, 0xD0};
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    return bytes;
}
void basic(uint8_t* message) {
    message[0] = 0x02;
    message[1] = 0x14;
    std::memcpy(message + 2, "TEST-DRONE-42", 13);
}
}

int main() {
    std::vector<uint8_t> beacon(36 + 2 + 5 + 3 + 50, 0);
    beacon[0] = 0x80;
    const uint8_t mac[6]{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    std::memcpy(beacon.data() + 10, mac, 6);
    beacon[36] = 221;
    beacon[37] = 5 + 3 + 50;
    beacon[38] = 0xFA; beacon[39] = 0x0B; beacon[40] = 0xBC;
    beacon[41] = 0x0D; beacon[42] = 1;
    beacon[43] = 0xF2; beacon[44] = 25; beacon[45] = 2;
    basic(beacon.data() + 46);
    auto* location = beacon.data() + 71;
    location[0] = 0x12; location[1] = 0x20;
    location[2] = 90; location[3] = 40;
    put32(location + 5, 407123456);
    put32(location + 9, -740123456);
    location[15] = 0xFC; location[16] = 0x08; // 2300 * .5 - 1000 = 150m.
    auto bytes = wrapped(beacon);
    platform::remote_id::Packet packet;
    assert(platform::remote_id::decode_monitor_frame(bytes.data(), bytes.size(), packet));
    assert(std::strcmp(packet.record.uasId, "TEST-DRONE-42") == 0);
    assert(packet.record.hasLocation && packet.record.latitudeE7 == 407123456);
    assert(packet.record.longitudeE7 == -740123456);
    assert(packet.record.altitudeGeoValid && packet.record.altitudeGeoM == 150);
    assert(packet.record.lastWifiChannel == 6 && packet.has_rssi && packet.rssi == -48);
    platform::wireless::FrameSummary summary;
    assert(platform::wireless::parse_frame_summary(bytes.data(), bytes.size(), summary));
    assert(summary.kind == platform::wireless::FrameKind::RemoteId && summary.type == "Remote ID");
    beacon[39] = 0; // Wrong OUI must not classify.
    bytes = wrapped(beacon);
    assert(!platform::remote_id::decode_monitor_frame(bytes.data(), bytes.size(), packet));
    assert(platform::wireless::parse_frame_summary(bytes.data(), bytes.size(), summary));
    assert(summary.kind == platform::wireless::FrameKind::Beacon);

    std::vector<uint8_t> nan(44 + 25, 0);
    nan[0] = 0xD0;
    std::memcpy(nan.data() + 10, mac, 6);
    const uint8_t destination[6]{0x51, 0x6F, 0x9A, 0x01, 0, 0};
    std::memcpy(nan.data() + 4, destination, 6);
    nan[24] = 4; nan[25] = 9;
    nan[26] = 0x50; nan[27] = 0x6F; nan[28] = 0x9A; nan[29] = 0x13;
    nan[30] = 3; nan[31] = 36;
    const uint8_t service[6]{0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};
    std::memcpy(nan.data() + 33, service, 6);
    nan[39] = 1; nan[41] = 0x10;
    nan[42] = 26; nan[43] = 4;
    basic(nan.data() + 44);
    bytes = wrapped(nan);
    assert(platform::remote_id::decode_monitor_frame(bytes.data(), bytes.size(), packet));
    assert(packet.transport == RemoteIdTransport::WifiNan);
    assert(platform::wireless::parse_frame_summary(bytes.data(), bytes.size(), summary));
    assert(summary.kind == platform::wireless::FrameKind::RemoteId);
    bytes.resize(20);
    assert(!platform::remote_id::decode_monitor_frame(bytes.data(), bytes.size(), packet));

    RemoteIdRecord track;
    assert(remoteIdUpdateLocation(track, 407123456, -740123456));
    assert(remoteIdUpdateLocation(track, 407124456, -740123456));
    assert(track.historyCount == 1);
    const auto center = remoteIdProjectToGrid(407123456, -740123456,
        407123456, -740123456, 104, 104, 100.0f, 6);
    assert(center.visible && center.x == 52 && center.y == 52);
    const auto north = remoteIdProjectToGrid(407124456, -740123456,
        407123456, -740123456, 104, 104, 100.0f, 6);
    assert(north.visible && north.y < center.y);
}
