// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "RemoteIdDecoder.h"
#include <cstddef>
#include <cstdint>

namespace platform::remote_id {
struct Packet {
    RemoteIdRecord record;
    RemoteIdTransport transport{RemoteIdTransport::None};
    int frequency_mhz{};
    int rssi{};
    bool has_rssi{};
};

// Accepts one radiotap-wrapped Wi-Fi frame. No Bluetooth transport is opened.
bool decode_monitor_frame(const uint8_t* bytes, std::size_t size, Packet& out);
}
