// SPDX-License-Identifier: AGPL-3.0-only
#include "remote_id_wifi.h"
#include "raw_wifi.h"
#include "channel_survey.h"
#include <cstring>

namespace platform::remote_id {
bool decode_monitor_frame(const uint8_t* bytes, std::size_t size, Packet& out) {
    out = {};
    std::size_t header{};
    if (!platform::wireless::monitor_header(bytes, size, header,
                                             out.frequency_mhz, out.rssi, out.has_rssi) ||
        size - header < 24) return false;
    const auto* frame = bytes + header;
    const auto length = size - header;
    if ((frame[0] & 0x03) != 0 || (frame[0] != 0x80 && frame[0] != 0xD0)) return false;
    RemoteIdDecoder decoder;
    auto result = decoder.decodeWifiBeacon(frame, length, out.record);
    out.transport = RemoteIdTransport::WifiBeacon;
    if (result != RemoteIdDecodeResult::Decoded) {
        out.record = {};
        result = decoder.decodeWifiNan(frame, length, out.record);
        out.transport = RemoteIdTransport::WifiNan;
    }
    if (result != RemoteIdDecodeResult::Decoded) return false;
    std::memcpy(out.record.mac, frame + 10, 6);
    out.record.transportMask = static_cast<uint8_t>(out.transport);
    out.record.rssi = out.has_rssi ? static_cast<int8_t>(out.rssi) : 0;
    out.record.lastWifiChannel = static_cast<uint8_t>(platform::wireless::channel_number(out.frequency_mhz));
    return true;
}
}
