// SPDX-License-Identifier: AGPL-3.0-only
#include "map_projection.h"
#include <cassert>
#include <cmath>

int main() {
    const auto origin = platform::maps::project(0, 0, 10);
    assert(origin.x == 131072 && origin.y == 131072);
    const auto east = platform::maps::project(0, 10000000, 10);
    assert(east.x > origin.x && std::abs(east.y - origin.y) < 0.01);
    const auto north = platform::maps::project(10000000, 0, 10);
    assert(north.y < origin.y);
    const auto dateline = platform::maps::project(0, -1790000000, 10);
    const auto opposite = platform::maps::project(0, 1790000000, 10);
    assert(std::abs(platform::maps::wrapped_x(dateline.x, opposite.x, 10)) < 1500);
}
