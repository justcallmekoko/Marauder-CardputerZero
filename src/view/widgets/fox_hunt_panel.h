// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "fox_hunt.h"
#include "lvgl.h"
#include <array>
#include <string>

namespace app { class AssetManager; }
namespace view::widgets {
class FoxHuntPanel {
public:
    FoxHuntPanel(lv_obj_t* parent, app::AssetManager& assets);
    void show(bool visible);
    void render(const model::FoxHunt& hunt, const std::string& label,
                const std::string& vendor, lv_color_t accent, bool dark_mode, bool tuned, bool censor);
private:
    lv_obj_t *root_{}, *name_{}, *address_{}, *vendor_{}, *signal_{}, *status_{}, *meta_{}, *chart_{};
    std::array<lv_obj_t*, model::FoxHunt::kHistory> bars_{};
};
}
