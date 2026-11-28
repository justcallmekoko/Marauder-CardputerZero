// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>

namespace screen {
class PcapFilesScreen : public BaseScreen {
public:
    PcapFilesScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    void build_content(lv_obj_t* content) override;
    void render();
    struct Row { lv_obj_t *box{}, *name{}, *meta{}; };
    std::array<Row, 4> rows_{};
    lv_obj_t *title_{}, *count_{}, *message_{};
    lv_obj_t *upload_label_{}, *upload_bar_{}, *batch_label_{}, *batch_bar_{};
    std::size_t first_{};
};
} // namespace screen
