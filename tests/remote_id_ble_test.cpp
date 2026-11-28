// SPDX-License-Identifier: AGPL-3.0-only
#include "remote_id_ble.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::string hex(const std::vector<uint8_t>& bytes) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (const auto byte : bytes) {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}
platform::BluetoothObservation sighting(const std::vector<uint8_t>& bytes) {
    platform::BluetoothObservation item;
    item.address = "12:34:56:78:9A:BC";
    item.rssi = -62;
    item.raw_ad = hex(bytes);
    return item;
}
std::vector<uint8_t> basic_id() {
    std::vector<uint8_t> message(25);
    message[0] = 0x02;
    message[1] = 0x14;
    std::memcpy(message.data() + 2, "BLE-DRONE-42", 12);
    return message;
}
std::vector<uint8_t> advertisement(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> result{2, 0x01, 0x06,
        static_cast<uint8_t>(5 + payload.size()), 0x16, 0xFA, 0xFF, 0x0D, 0x17};
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
}

int main() {
    auto bytes = advertisement(basic_id());
    platform::remote_id::Packet packet;
    assert(platform::remote_id::decode_bluetooth_report(sighting(bytes), packet));
    assert(packet.transport == RemoteIdTransport::Bluetooth);
    assert(packet.rssi == -62 && packet.has_rssi && packet.frequency_mhz == 0);
    assert(std::strcmp(packet.record.uasId, "BLE-DRONE-42") == 0);
    assert(packet.record.mac[0] == 0x12 && packet.record.mac[5] == 0xBC);
    assert(packet.record.lastWifiChannel == 0);
    char label[20];
    remoteIdFormatTransports(packet.record.transportMask, label, sizeof(label));
    assert(std::strcmp(label, "BLE") == 0);

    std::vector<uint8_t> pack{0xF2, 25, 2};
    const auto basic = basic_id();
    pack.insert(pack.end(), basic.begin(), basic.end());
    std::vector<uint8_t> location(25);
    location[0] = 0x12;
    location[1] = 0x20;
    location[5] = 0x15; location[6] = 0xCD; location[7] = 0x44; location[8] = 0x18;
    location[9] = 0xC0; location[10] = 0xF1; location[11] = 0xE2; location[12] = 0xD3;
    pack.insert(pack.end(), location.begin(), location.end());
    assert(platform::remote_id::decode_bluetooth_report(sighting(advertisement(pack)), packet));
    assert(packet.record.hasUasId && packet.record.hasLocation);

    bytes[6] = 0xFE; // Other service UUID.
    assert(!platform::remote_id::decode_bluetooth_report(sighting(bytes), packet));
    bytes = advertisement(basic);
    bytes[7] = 0x0C; // Other application code.
    assert(!platform::remote_id::decode_bluetooth_report(sighting(bytes), packet));
    bytes = advertisement(basic);
    bytes.pop_back(); // Truncated advertising field.
    assert(!platform::remote_id::decode_bluetooth_report(sighting(bytes), packet));
    auto bad_address = sighting(advertisement(basic));
    bad_address.address = "invalid";
    assert(!platform::remote_id::decode_bluetooth_report(bad_address, packet));
}
