// SPDX-License-Identifier: AGPL-3.0-only
#include "wardrive_channels.h"
#include <cassert>

int main() {
    platform::wireless::WardriveChannels channels;
    assert(channels.observe("a", 0, false));
    assert(!channels.observe("a", 2412, true));
    assert(!channels.observe("a", 2437, false));
    assert(channels.observe("b", 5955, true));
    assert(channels.observe("c", 2412, true));
    assert(channels.observe("d", 0, false));
    assert(channels.size() == 4);
    const auto counts = channels.distribution();
    assert(counts.size() == 3);
    assert(counts[0].frequency_mhz == 2412 && counts[0].channel == 1 && counts[0].access_points == 2);
    assert(counts[1].frequency_mhz == 5955 && counts[1].channel == 1 && counts[1].access_points == 1);
    assert(counts[2].frequency_mhz == 0 && counts[2].access_points == 1);
    channels.clear();
    assert(channels.size() == 0 && channels.distribution().empty());
}
