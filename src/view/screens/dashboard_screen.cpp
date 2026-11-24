/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "dashboard_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace screen {

DashboardScreen::DashboardScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void DashboardScreen::build_content(lv_obj_t* content) {
    auto* panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_hor(panel, 10, 0);
    lv_obj_set_style_pad_ver(panel, 5, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    auto* status = lv_label_create(panel);
    lv_label_set_text(status, "SYSTEM DIAGNOSTICS");
    auto* status_font = assets().load_standard_font(13);
    lv_obj_set_style_text_font(status, status_font ? status_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(status, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* summary = lv_label_create(panel);
    lv_obj_set_width(summary, LV_PCT(100));
    lv_label_set_long_mode(summary, LV_LABEL_LONG_WRAP);
    lv_label_bind_text(summary, view_model().dashboard_summary_subject(), nullptr);
    auto* body_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(summary, body_font ? body_font : &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_top(summary, 4, 0);
    reactive::bind_theme(summary, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
