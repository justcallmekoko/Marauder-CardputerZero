// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>

namespace screen {
class MapPacksScreen : public BaseScreen {
public:
    MapPacksScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    void build_content(lv_obj_t* content) override;
    void render();
    lv_obj_t *title_{}, *space_{}, *zoom_{}, *message_{};
    struct Row { lv_obj_t *box{}, *name{}, *meta{}; };
    std::array<Row, 4> rows_{};
};
}
