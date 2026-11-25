/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include "base_screen.h"

#include <array>

namespace screen {

class DashboardScreen : public BaseScreen {
public:
    DashboardScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~DashboardScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void render_menu();
    static void menu_changed_cb(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* panel_{nullptr};
    lv_obj_t* menu_title_{nullptr};
    std::array<lv_obj_t*, 6> rows_{};
    std::array<lv_obj_t*, 6> row_labels_{};
    std::array<lv_obj_t*, 6> marks_{};
    lv_obj_t* footer_{nullptr};
    lv_observer_t* menu_observer_{nullptr};
};

} // namespace screen
