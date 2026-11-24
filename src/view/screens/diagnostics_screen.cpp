/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "diagnostics_screen.h"

#include "asset_manager.h"
#include "bindings.h"

namespace screen {

DiagnosticsScreen::DiagnosticsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void DiagnosticsScreen::build_content(lv_obj_t* content) {
    auto* scroll = lv_obj_create(content);
    lv_obj_remove_style_all(scroll);
    lv_obj_set_size(scroll, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_hor(scroll, 8, 0);
    lv_obj_set_style_pad_ver(scroll, 4, 0);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_ACTIVE);

    auto* details = lv_label_create(scroll);
    lv_obj_set_width(details, LV_PCT(100));
    lv_label_set_long_mode(details, LV_LABEL_LONG_WRAP);
    lv_label_bind_text(details, view_model().diagnostics_detail_subject(), nullptr);
    auto* body_font = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(details, body_font ? body_font : &lv_font_montserrat_10, 0);
    reactive::bind_theme(details, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
