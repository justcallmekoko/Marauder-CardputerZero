// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>

namespace platform {
enum class Radio { Wifi, Bluetooth, Gps };
std::string enable_radio(Radio radio);
}
