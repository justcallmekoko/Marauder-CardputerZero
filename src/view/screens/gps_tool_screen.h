// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>
#include <string>

namespace screen {
class GpsToolScreen : public BaseScreen {
public:
    GpsToolScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    struct FeedRow { lv_obj_t *box{}, *type{}, *name{}, *address{}; };
    void build_content(lv_obj_t* content) override;
    void render();
    void render_wardrive_map();
    void draw_wardrive_map(lv_event_t* event);
    lv_obj_t *heading_{}, *state_{}, *summary_card_{}, *status_{}, *position_{}, *detail_{},
             *wifi_card_{}, *bluetooth_card_{}, *total_card_{}, *wifi_count_{},
             *bluetooth_count_{}, *total_count_{}, *wardrive_status_{}, *message_{}, *file_{};
    lv_obj_t *map_{}, *map_plot_{}, *map_hint_{}, *map_credit_{}, *map_meta_{},
             *map_wifi_{}, *map_bluetooth_{}, *map_total_{};
    std::array<lv_obj_t*, 6> map_tiles_{};
    std::array<std::string, 6> map_tile_paths_{};
    double map_center_x_{}, map_center_y_{};
    unsigned map_zoom_{};
    bool map_has_position_{};
    std::array<FeedRow, 3> feed_rows_{};
};
}
