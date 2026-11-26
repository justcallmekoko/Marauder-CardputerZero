// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>

namespace screen {
class GpsToolScreen : public BaseScreen {
public:
    GpsToolScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    struct FeedRow { lv_obj_t *box{}, *type{}, *name{}, *address{}; };
    void build_content(lv_obj_t* content) override;
    void render();
    lv_obj_t *heading_{}, *state_{}, *summary_card_{}, *status_{}, *position_{}, *detail_{},
             *wifi_card_{}, *bluetooth_card_{}, *total_card_{}, *wifi_count_{},
             *bluetooth_count_{}, *total_count_{}, *wardrive_status_{}, *message_{}, *file_{};
    std::array<FeedRow, 3> feed_rows_{};
};
}
