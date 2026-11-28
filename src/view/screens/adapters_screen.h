/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include "base_screen.h"

#include <array>

namespace screen {

class AdaptersScreen : public BaseScreen {
public:
    AdaptersScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    lv_obj_t* heading_{};
    lv_obj_t* bands_{};
    lv_obj_t* info_{};
    lv_obj_t* expanded_{};
    std::array<lv_obj_t*, 3> rows_{};
    std::array<lv_obj_t*, 3> labels_{};
    void render();
    void render_bluetooth();
    static void changed(lv_observer_t* observer, lv_subject_t*);
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
