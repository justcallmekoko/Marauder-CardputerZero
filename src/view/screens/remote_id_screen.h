// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>

namespace screen {
class RemoteIdScreen : public BaseScreen {
public:
    RemoteIdScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    void build_content(lv_obj_t* content) override;
    void render();
    struct Row { lv_obj_t *box{}, *name{}, *meta{}; };
    lv_obj_t *title_{}, *count_{}, *map_{}, *gps_hint_{}, *range_{}, *detail_{}, *empty_{};
    std::array<Row, 4> rows_{};
    std::array<lv_obj_t*, 32> markers_{};
    std::array<lv_obj_t*, 32> paths_{};
    std::array<std::array<lv_point_precise_t, REMOTE_ID_HISTORY_CAPACITY + 1>, 32> path_points_{};
    std::size_t first_{};
};
}
