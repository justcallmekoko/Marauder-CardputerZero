// SPDX-License-Identifier: AGPL-3.0-only
#include "map_packs.h"
#include <cassert>
#include <cstdlib>

int main() {
    assert(platform::maps::pack_count() == 27);
    assert(platform::maps::pack(0).archive_bytes > 0);
    assert(platform::maps::pack(1).installed_bytes > 0);
    assert(platform::maps::tile(0, 6, 32, 20).filename() == "20.png");
    assert(platform::maps::tile(0, 7, 32, 20).empty());
    assert(platform::maps::tile(0, 6, 64, 20).empty());
    assert(platform::maps::tile(2, 16, 32768, 20000).filename() == "20000.png");
    assert(platform::maps::local_estimate(400000000, -740000000, 14) > 0);
    assert(platform::maps::local_estimate(400000000, -740000000, 17) == 0);
    assert(platform::maps::pack(3).name == std::string("North America"));
    assert(platform::maps::area_estimate(16, 7) > platform::maps::area_estimate(16, 6));
    assert(platform::maps::area_estimate(3, 10) > 2000ULL * 40960);
    assert(platform::maps::area_estimate(26, 11) == 0);
    assert(platform::maps::tile(26, 10, 100, 100).filename() == "100.png");
    if (std::getenv("MARAUDER_MAP_SMOKE")) {
        assert(platform::maps::install_area(18, 2) == "Area map installed");
        assert(platform::maps::installed(18));
        assert(platform::maps::installed_zoom(18) == 2);
        assert(platform::maps::area_size(18) > 0);
    }
}
