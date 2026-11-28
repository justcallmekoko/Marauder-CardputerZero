// SPDX-License-Identifier: AGPL-3.0-only
#include "remote_id_ble.h"
#include <array>
#include <cctype>

namespace platform::remote_id {
namespace {
int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
bool address_bytes(const std::string& text, uint8_t (&mac)[6]) {
    if (text.size() != 17) return false;
    for (std::size_t i = 0; i < 6; ++i) {
        if (i && text[i * 3 - 1] != ':') return false;
        const int high = nibble(text[i * 3]), low = nibble(text[i * 3 + 1]);
        if (high < 0 || low < 0) return false;
        mac[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}
}

bool decode_bluetooth_report(const BluetoothObservation& sighting, Packet& out) {
    out = {};
    const auto& hex = sighting.raw_ad;
    if (hex.empty() || hex.size() > 510 || hex.size() % 2) return false;
    std::array<uint8_t, 255> bytes{};
    const auto length = hex.size() / 2;
    for (std::size_t i = 0; i < length; ++i) {
        const int high = nibble(hex[i * 2]), low = nibble(hex[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        bytes[i] = static_cast<uint8_t>((high << 4) | low);
    }
    for (std::size_t offset = 0; offset < length;) {
        const auto field_length = bytes[offset++];
        if (field_length == 0) break;
        if (field_length > length - offset) return false;
        const auto* field = bytes.data() + offset;
        if (field_length >= 5 && field[0] == 0x16 && field[1] == 0xFA &&
            field[2] == 0xFF && field[3] == 0x0D) {
            RemoteIdRecord record{};
            RemoteIdDecoder decoder;
            // UUID, application code, and counter precede the ODID message.
            if (decoder.decodePayload(field + 5, field_length - 5, record) != RemoteIdDecodeResult::Decoded ||
                !address_bytes(sighting.address, record.mac)) return false;
            record.transportMask = static_cast<uint8_t>(RemoteIdTransport::Bluetooth);
            record.rssi = static_cast<int8_t>(sighting.rssi);
            out.record = record;
            out.transport = RemoteIdTransport::Bluetooth;
            out.rssi = sighting.rssi;
            out.has_rssi = true;
            return true;
        }
        offset += field_length;
    }
    return false;
}
}
