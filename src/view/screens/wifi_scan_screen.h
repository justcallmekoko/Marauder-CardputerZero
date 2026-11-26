// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>

namespace screen {
class WifiScanScreen : public BaseScreen {
public:
    WifiScanScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    void build_content(lv_obj_t* content) override;
    void render();
    struct Row { lv_obj_t *box{}, *name{}, *metric{}, *detail{}; };
    std::array<Row, 4> rows_{};
    lv_obj_t *title_{}, *count_{}, *builder_{}, *message_{};
    size_t first_{};
    int last_mode_{-1};
};
}
