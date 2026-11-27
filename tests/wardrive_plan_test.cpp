// SPDX-License-Identifier: AGPL-3.0-only
#include "wardrive_plan.h"
#include <algorithm>
#include <cassert>

int main() {
    using platform::wireless::WardriveRadioPlan;
    std::vector<WardriveRadioPlan> radios = {
        {"a", {2412, 2437, 2462, 5180, 5200, 5220}},
        {"b", {2412, 2437, 2462, 5180, 5200, 5220}},
        {"c", {2412, 2437, 2462, 5180, 5200, 5220}},
    };
    const auto blocks = platform::wireless::wardrive_channel_blocks(radios, 0);
    assert(blocks[0].channels == std::vector<unsigned>({2412, 2437}));
    assert(blocks[1].channels == std::vector<unsigned>({2462, 5180}));
    assert(blocks[2].channels == std::vector<unsigned>({5200, 5220}));
    radios = {{"a", {2412, 2437, 2462}}, {"b", {5180, 5200, 5220}}};
    const auto mixed = platform::wireless::wardrive_channel_blocks(radios, 1);
    assert(mixed[0].channels.size() == 6);
    assert(mixed[1].channels.size() == 3);
    for (const auto& radio : mixed)
        for (auto mhz : radio.channels)
            assert(std::find(radio.supported.begin(), radio.supported.end(), mhz) != radio.supported.end());
}
