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
    lv_obj_t *title_{}, *index_{}, *space_{}, *zoom_{}, *message_{};
    lv_obj_t *progress_title_{}, *progress_detail_{}, *progress_bar_{};
    unsigned storage_revision_{};
    std::array<std::uint64_t, 27> sizes_{};
    struct Row { lv_obj_t *box{}, *name{}, *meta{}; };
    std::array<Row, 4> rows_{};
};
}
