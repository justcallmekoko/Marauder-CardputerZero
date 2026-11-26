// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include "fox_hunt_panel.h"
#include <array>
#include <memory>

namespace screen {
class WifiScanScreen : public BaseScreen {
public:
    WifiScanScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    void build_content(lv_obj_t* content) override;
    void render();
    struct Row { lv_obj_t *box{}, *name{}, *metric{}, *detail{}; };
    std::array<Row, 4> rows_{};
    std::unique_ptr<view::widgets::FoxHuntPanel> fox_panel_;
    lv_obj_t *title_{}, *channel_{}, *count_{}, *builder_{}, *message_{};
    size_t first_{};
    int last_mode_{-1};
};
}
