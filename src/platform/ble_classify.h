// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "radio_power.h"
#include <cstdint>
#include <string>

namespace platform::ble {
enum class Kind : unsigned { FindMy, IBeacon, Flipper, Flock, Meta, SerialModule, Count };
constexpr uint32_t bit(Kind kind) { return 1u << static_cast<unsigned>(kind); }
const char* label(Kind kind);
struct Classification {
    uint32_t matches{};
    std::string type{"BLE device"};
    std::string vendor{"Unknown"};
    std::string evidence;
    std::string detail;
};
Classification classify(const BluetoothObservation& observation);
} // namespace platform::ble
