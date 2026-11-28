// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "radio_power.h"
#include "remote_id_wifi.h"

namespace platform::remote_id {
// Decodes a complete LE advertising report carrying ASTM service UUID 0xFFFA.
bool decode_bluetooth_report(const BluetoothObservation& sighting, Packet& out);
}
