/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

namespace screen {
namespace {
bool guide_key(std::string_view token) {
    static constexpr std::array<std::string_view, 15> names = {
        "F", "X", "Z", "C", "D", "M", "P", "B", "T", "L", "H", "A", "U", "W", "ESC"
    };
    auto is_name = [&](std::string_view name) {
        if (name == "Enter" || name == "ENTER" || name == "Esc" ||
            name == "Backspace" || name == "HELP") return true;
        for (const auto known : names) if (known == name) return true;
        return false;
    };
    if (token.empty()) return false;
    for (std::size_t start = 0; start < token.size();) {
        const auto end = token.find('/', start);
        if (!is_name(token.substr(start, end == std::string_view::npos ? end : end - start))) return false;
        if (end == std::string_view::npos) return true;
        start = end + 1;
    }
    return false;
}
}

BaseScreen::BaseScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : view_model_(view_model), assets_(assets) {}

BaseScreen::~BaseScreen() {
    title_bar_.reset();

    if (root_ && lv_obj_is_valid(root_)) {
        lv_obj_delete(root_);
    }
}

void BaseScreen::init() {
    if (root_) {
        return;
    }

    root_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(root_, view::kScreenWidth, view::kScreenHeight);
    reactive::bind_theme(root_, view_model_.dark_mode_subject(), reactive::ThemeRole::Screen);

    title_bar_ = std::make_unique<view::widgets::TitleBar>(root_, view_model_, assets_);
    title_bar_->build();

    content_ = lv_obj_create(root_);
    lv_obj_remove_style_all(content_);
    lv_obj_set_size(content_, LV_PCT(100), view::kContentHeight);
    lv_obj_align(content_, LV_ALIGN_TOP_MID, 0, view::kTitleBarHeight);
    lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
    reactive::bind_theme(content_, view_model_.dark_mode_subject(), reactive::ThemeRole::Surface);

    key_guide_ = lv_label_create(root_);
    lv_obj_set_size(key_guide_, LV_PCT(100), view::kGuideHeight);
    lv_obj_align(key_guide_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_align(key_guide_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(key_guide_, 3, 0);
    auto* guide_font = assets_.load_standard_font(10);
    lv_obj_set_style_text_font(key_guide_, guide_font ? guide_font : &lv_font_montserrat_10, 0);
    lv_label_set_recolor(key_guide_, true);
    const auto colors = view::palette(view_model_.is_dark_mode());
    lv_obj_set_style_text_color(key_guide_, colors.text, 0);
    lv_obj_set_style_bg_color(key_guide_, colors.bar, 0);
    lv_obj_set_style_bg_opa(key_guide_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(key_guide_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(key_guide_, 1, 0);
    lv_obj_set_style_border_color(key_guide_, colors.border, 0);
    build_content(content_);
}

void BaseScreen::set_key_guide(const char* text) {
    const auto colors = view::palette(view_model_.is_dark_mode());
    char color_hex[8]{};
    std::snprintf(color_hex, sizeof(color_hex), "%06x",
                  static_cast<unsigned>(lv_color_to_int(colors.info) & 0xffffffU));
    const std::string_view plain = text ? text : "";
    std::string styled;
    styled.reserve(plain.size() + 80);
    for (std::size_t start = 0; start < plain.size();) {
        if (std::isspace(static_cast<unsigned char>(plain[start]))) {
            styled += plain[start++];
            continue;
        }
        auto end = start;
        while (end < plain.size() && !std::isspace(static_cast<unsigned char>(plain[end]))) ++end;
        const auto token = plain.substr(start, end - start);
        if (guide_key(token)) {
            styled += '#';
            styled += color_hex;
            styled += ' ';
            styled.append(token.data(), token.size());
            styled += '#';
        } else styled.append(token.data(), token.size());
        start = end;
    }
    lv_obj_set_style_text_color(key_guide_, colors.text, 0);
    lv_label_set_text(key_guide_, styled.c_str());
}

lv_obj_t* BaseScreen::root() const {
    return root_;
}

viewmodel::BaseViewModel& BaseScreen::view_model() {
    return view_model_;
}

app::AssetManager& BaseScreen::assets() {
    return assets_;
}

} // namespace screen
