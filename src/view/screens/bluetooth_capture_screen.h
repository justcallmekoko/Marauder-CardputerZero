// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include "fox_hunt_panel.h"
#include <array>
#include <memory>

namespace screen {
class BluetoothCaptureScreen : public BaseScreen {
public:
    BluetoothCaptureScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets);
private:
    void build_content(lv_obj_t* content) override;
    void render();
    struct Row { lv_obj_t *box{}, *name{}, *meta{}, *data{}; };
    std::array<Row, 4> rows_{};
    lv_obj_t *title_{}, *counts_{}, *status_{}, *detail_{};
    std::unique_ptr<view::widgets::FoxHuntPanel> fox_panel_;
};
} // namespace screen
