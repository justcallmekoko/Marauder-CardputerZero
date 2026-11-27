// SPDX-License-Identifier: AGPL-3.0-only
#include "map_packs.h"
#include <cassert>

int main() {
    assert(platform::maps::pack_count() == 3);
    assert(platform::maps::pack(0).archive_bytes > 0);
    assert(platform::maps::pack(1).installed_bytes > 0);
    assert(platform::maps::tile(0, 6, 32, 20).filename() == "20.png");
    assert(platform::maps::tile(0, 7, 32, 20).empty());
    assert(platform::maps::tile(0, 6, 64, 20).empty());
    assert(platform::maps::tile(2, 16, 32768, 20000).filename() == "20000.png");
    assert(platform::maps::local_estimate(400000000, -740000000, 14) > 0);
    assert(platform::maps::local_estimate(400000000, -740000000, 17) == 0);
}
