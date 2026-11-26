// SPDX-License-Identifier: AGPL-3.0-only
#include "fox_hunt.h"
#include <cassert>

int main() {
    model::FoxHunt hunt;
    hunt.begin("12:34:56:78:9A:BC", 2437);
    assert(!hunt.observe("AA:BB:CC:DD:EE:FF", 2437, -50, true, 100));
    assert(!hunt.observe("12:34:56:78:9A:BC", 2412, -50, true, 100));
    assert(hunt.frames() == 0);
    assert(hunt.observe("12:34:56:78:9A:BC", 2437, -61, true, 100));
    assert(hunt.observe("12:34:56:78:9A:BC", 2437, -60, true, 150));
    assert(hunt.observe("12:34:56:78:9A:BC", 0, 0, false, 200));
    assert(hunt.frames() == 3 && hunt.sample_count() == 1 && hunt.rssi() == -60);
    assert(hunt.fresh(3150) && !hunt.fresh(3151));
    for (int i = 0; i < 25; ++i)
        assert(hunt.observe("12:34:56:78:9A:BC", 2437, -80 + i, true, 4000 + i * 250));
    assert(hunt.sample_count() == model::FoxHunt::kHistory);
    assert(hunt.history()[0] == -75 && hunt.history()[19] == -56);
    hunt.reset();
    assert(!hunt.active() && hunt.frames() == 0 && hunt.sample_count() == 0);
}
