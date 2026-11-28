// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace platform::maps {
struct Pixel { double x{}, y{}; };
inline Pixel project(int32_t latitude_e7, int32_t longitude_e7, unsigned zoom) {
    const double world = 256.0 * (1u << zoom);
    const double latitude = std::clamp(static_cast<double>(latitude_e7) / 1e7,
        -85.05112878, 85.05112878) * 3.141592653589793 / 180.0;
    return {world * (static_cast<double>(longitude_e7) / 1e7 + 180.0) / 360.0,
            world * (1.0 - std::asinh(std::tan(latitude)) / 3.141592653589793) / 2.0};
}
inline double wrapped_x(double x, double center_x, unsigned zoom) {
    const double world = 256.0 * (1u << zoom);
    double difference = x - center_x;
    if (difference > world / 2) difference -= world;
    if (difference < -world / 2) difference += world;
    return difference;
}
}
