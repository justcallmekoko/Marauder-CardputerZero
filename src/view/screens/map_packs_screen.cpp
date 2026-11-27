// SPDX-License-Identifier: AGPL-3.0-only
#include "map_packs_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "map_packs.h"
#include <cstdio>

namespace screen {
namespace {
std::string size_text(std::uint64_t bytes) {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%.1f %s", bytes >= 1073741824ULL ? bytes / 1073741824.0 : bytes / 1048576.0,
                  bytes >= 1073741824ULL ? "GiB" : "MiB");
    return buffer;
}
}
MapPacksScreen::MapPacksScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }
void MapPacksScreen::build_content(lv_obj_t* content) {
    const auto colors = view::palette(view_model().is_dark_mode());
    const auto label = [&](lv_obj_t* parent, int x, int y, int width, int size) {
        auto* obj = lv_label_create(parent);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_width(obj, width);
        auto* font = assets().load_standard_font(size);
        lv_obj_set_style_text_font(obj, font ? font : LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(obj, colors.text, 0);
        lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
        return obj;
    };
    title_ = label(content, 8, 1, 125, 13);
    space_ = label(content, 134, 3, 178, 10);
    lv_obj_set_style_text_align(space_, LV_TEXT_ALIGN_RIGHT, 0);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_set_pos(row.box, 6, 18 + static_cast<int>(i) * 26);
        lv_obj_set_size(row.box, 308, 25);
        lv_obj_set_style_radius(row.box, 4, 0);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        row.name = label(row.box, 8, 1, 290, 11);
        row.meta = label(row.box, 8, 13, 290, 9);
    }
    zoom_ = label(content, 8, 98, 304, 11);
    message_ = label(content, 8, 112, 304, 10);
    reactive::observe_obj(content, view_model().map_subject(), [](lv_observer_t* observer, lv_subject_t*) {
        static_cast<MapPacksScreen*>(lv_observer_get_user_data(observer))->render();
    }, this);
}
void MapPacksScreen::render() {
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_label_set_text(title_, "OFFLINE MAPS");
    const auto free = "Free " + size_text(platform::maps::free_bytes());
    lv_label_set_text(space_, free.c_str());
    lv_obj_set_style_text_color(space_, colors.info, 0);
    for (unsigned i = 0; i < rows_.size(); ++i) {
        const auto& pack = platform::maps::pack(i);
        auto& row = rows_[i];
        const bool focus = i == view_model().map_pack_cursor();
        const bool installed = platform::maps::installed(i);
        lv_obj_set_style_bg_color(row.box, focus ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_color(row.box, focus ? colors.info : colors.border, 0);
        const auto name = std::string(pack.name) + (i == view_model().map_pack() && installed ? "   ACTIVE" : "");
        lv_label_set_text(row.name, name.c_str());
        const auto meta = i == 2 ?
            (view_model().map_local_estimate() ? "~" + size_text(view_model().map_local_estimate()) +
                " for 3 km   saved " + size_text(platform::maps::local_size()) : "GPS fix needed   3 km radius") :
            (installed ? std::string("Installed") : "Download " + size_text(pack.archive_bytes)) +
                "   z1-6   disk " + size_text(pack.installed_bytes);
        lv_label_set_text(row.meta, meta.c_str());
        lv_obj_set_style_text_color(row.name, focus ? colors.info : colors.text, 0);
        lv_obj_set_style_text_color(row.meta, colors.text_disabled, 0);
    }
    const auto zoom = "Remote ID zoom   <  z" + std::to_string(view_model().map_zoom()) + "  >";
    lv_label_set_text(zoom_, zoom.c_str());
    lv_obj_set_style_text_color(zoom_, colors.info, 0);
    lv_label_set_text(message_, view_model().map_message().c_str());
    lv_obj_set_style_text_color(message_, colors.text_disabled, 0);
    set_key_guide("F/X Packs  Enter Download/Use  Left/Right Zoom  Esc Back");
}
}
