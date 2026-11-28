// SPDX-License-Identifier: AGPL-3.0-only
#include "base_model.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

int main() {
    char path[] = "/tmp/marauder-censor-XXXXXX";
    const auto* root = mkdtemp(path);
    assert(root);
    assert(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    {
        model::BaseModel first;
        assert(!first.censor_enabled());
        assert(first.toggle_censor());
        assert(first.censor_enabled());
        assert(first.map_zoom() == 6);
        assert(first.set_map_zoom(4));
        assert(first.set_map_pack(1));
    }
    {
        model::BaseModel reopened;
        assert(reopened.censor_enabled());
        assert(reopened.map_zoom() == 4);
        assert(reopened.map_pack() == 1);
        assert(!reopened.set_map_zoom(17));
        assert(reopened.toggle_censor());
    }
    assert(!model::BaseModel{}.censor_enabled());
    std::filesystem::remove_all(root);
}
