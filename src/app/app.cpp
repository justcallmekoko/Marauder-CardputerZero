/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"
#include <csignal>

#include "asset_manager.h"
#include "help_popup.h"
#include "logger.h"
#include "linux_input.h"
#include "screenshot_service.h"
#include "screen_manager.h"
#include "shortcut_policy.h"
#include "base_viewmodel.h"
#include "theme.h"
#include "toast.h"

#include <memory>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

namespace app {
namespace {

volatile std::sig_atomic_t stopping = 0;
void stop_signal(int) { stopping = 1; }

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

class ShortcutController {
public:
    ShortcutController(viewmodel::BaseViewModel& view_model, AssetManager& assets)
        : view_model_(view_model), assets_(assets),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)) {
        toast_->build();
        help_popup_->build();
        platform::set_global_key_listener(global_key_listener, this);
    }

    ~ShortcutController() {
        platform::clear_global_key_listener(global_key_listener, this);
        if (confirmation_) lv_obj_delete(confirmation_);
    }

private:
    enum class DialogAction { Exit, SelectAdapter, OpenAdapters };
    DialogAction dialog_action_{DialogAction::Exit};
    void show_confirmation(const std::string& message, const char* accept, DialogAction action) {
        dialog_action_ = action;
        const auto colors = view::palette(view_model_.is_dark_mode());
        confirmation_ = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(confirmation_);
        lv_obj_set_size(confirmation_, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(confirmation_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(confirmation_, LV_OPA_80, 0);
        lv_obj_clear_flag(confirmation_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(confirmation_, LV_OBJ_FLAG_CLICKABLE);

        auto* panel = lv_obj_create(confirmation_);
        lv_obj_remove_style_all(panel);
        lv_obj_set_size(panel, 248, action == DialogAction::Exit ? 88 : 112);
        lv_obj_center(panel);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(panel, 6, 0);
        lv_obj_set_style_bg_color(panel, colors.surface, 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_border_color(panel, colors.border, 0);

        auto* title = lv_label_create(panel);
        lv_label_set_text(title, message.c_str());
        lv_obj_set_width(title, 228);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        auto* font = assets_.load_standard_font(16);
        lv_obj_set_style_text_font(title, font ? font : LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(title, colors.text, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

        const auto add_shortcut = [&](const char* text, lv_align_t align, int x, lv_color_t color) {
            auto* label = lv_label_create(panel);
            lv_label_set_text(label, text);
            auto* key_font = assets_.load_standard_font(12);
            lv_obj_set_style_text_font(label, key_font ? key_font : LV_FONT_DEFAULT, 0);
            lv_obj_set_style_text_color(label, color, 0);
            lv_obj_set_style_bg_color(label, color, 0);
            lv_obj_set_style_bg_opa(label, LV_OPA_10, 0);
            lv_obj_set_style_pad_hor(label, 8, 0);
            lv_obj_set_style_pad_ver(label, 4, 0);
            lv_obj_set_style_radius(label, 3, 0);
            lv_obj_align(label, align, x, -12);
        };
        add_shortcut("Esc  Cancel", LV_ALIGN_BOTTOM_LEFT, 12, colors.text);
        add_shortcut(accept, LV_ALIGN_BOTTOM_RIGHT, -12, colors.warning);
    }

    bool handle_key(uint32_t key, bool long_pressed) {
        if (view_model_.wake_display()) return true;
        if (confirmation_) {
            if (!long_pressed && (key == LV_KEY_ENTER || key == LV_KEY_ESC)) {
                if (key == LV_KEY_ENTER) {
                    if (dialog_action_ == DialogAction::Exit) view_model_.request_quit();
                    else if (dialog_action_ == DialogAction::OpenAdapters) view_model_.show_adapters_page();
                    else {
                        const auto& radios = view_model_.adapters();
                        const auto cursor = view_model_.adapter_cursor();
                        if (cursor < radios.size() && radios[cursor].stable_id == pending_adapter_)
                            view_model_.select_highlighted_adapter();
                    }
                }
                lv_obj_delete(confirmation_);
                confirmation_ = nullptr;
                pending_adapter_.clear();
            }
            return true;
        }
        if (view_model_.editing_credential()) {
            if (!long_pressed) view_model_.credential_key(key);
            return true;
        }
        if (key == platform::kKeyPrintScreen) {
            if (!long_pressed) {
                const auto result = platform::screenshot::capture_active_screen(view_model_.screenshots_in_documents(), view_model_.reserve_mib());
                if (result.success) {
                    LOG_INFO("screenshot saved: {}", result.path);
                    toast_->show_highlighted("Saved to ",
                                             result.path,
                                             "",
                                             view::widgets::ToastTone::Success,
                                             5200,
                                             true);
                }
                else {
                    LOG_WARN("screenshot failed: {}", result.error);
                    toast_->show("Screenshot failed: " + result.error,
                                 view::widgets::ToastTone::Error,
                                 3200);
                }
            }
            return true;
        }

        if (key == platform::kKeyHelp) {
            if (!long_pressed) {
                toast_->hide();
                if (help_popup_->visible()) {
                    help_popup_->hide();
                }
                else {
                    help_popup_->show(view_model_.current_page());
                }
            }
            return true;
        }

        if (help_popup_->visible()) {
            if (key == LV_KEY_ESC && !long_pressed) {
                help_popup_->hide();
            }
            return true;
        }

        if (key == LV_KEY_ESC && !long_pressed) {
            if (view_model_.current_page() == model::AppPage::WifiScan && view_model_.wifi_details_open()) {
                view_model_.toggle_wifi_scan_details();
                return true;
            }
            switch (escape_action(view_model_.current_page(),
                                  view_model_.marauder_menu_at_root())) {
                case EscapeAction::ExitApplication:
                    toast_->hide();
                    show_confirmation("Exit Marauder?", "Enter  Exit", DialogAction::Exit);
                    break;
                case EscapeAction::BackMenu:
                    view_model_.marauder_menu_back();
                    break;
                case EscapeAction::ShowDashboard:
                    view_model_.show_dashboard_page();
                    break;
            }
            return true;
        }

        if (long_pressed) return false;

        const auto action = key == LV_KEY_UP ? NavigationAction::Previous :
                            key == LV_KEY_DOWN ? NavigationAction::Next :
                            (key == LV_KEY_ENTER || key == LV_KEY_RIGHT) ? NavigationAction::Primary :
                            key == LV_KEY_LEFT ? NavigationAction::Back : navigation_action(key);
        if (view_model_.current_page() == model::AppPage::Dashboard) {
            if (key == LV_KEY_ENTER) {
                const auto index = view_model_.marauder_menu_selected_index();
                const auto warning = view_model_.collection_warning(view_model_.marauder_menu_item(index).radio);
                if (warning.empty()) view_model_.marauder_menu_activate();
                else show_confirmation(warning, "Enter Adapters", DialogAction::OpenAdapters);
            }
            else if (action == NavigationAction::Previous) view_model_.move_menu_focus(0, -1);
            else if (action == NavigationAction::Next) view_model_.move_menu_focus(0, 1);
            else if (action == NavigationAction::Back) view_model_.move_menu_focus(-1, 0);
            else if (action == NavigationAction::Primary) view_model_.move_menu_focus(1, 0);
            else return false;
            return true;
        }

        if (view_model_.current_page() == model::AppPage::Diagnostics) {
            if (action == NavigationAction::Previous) { view_model_.previous_system_info_page(); return true; }
            if (action == NavigationAction::Next) { view_model_.next_system_info_page(); return true; }
            if (action == NavigationAction::Primary) { view_model_.refresh_diagnostics(); return true; }
            if (action == NavigationAction::Details) { view_model_.toggle_system_info_details(); return true; }
        }

        if (view_model_.current_page() == model::AppPage::RadioSetup) {
            if (action == NavigationAction::Previous) { view_model_.move_radio(-1); return true; }
            if (action == NavigationAction::Next) { view_model_.move_radio(1); return true; }
            if (key == LV_KEY_ENTER) { view_model_.toggle_radio(); return true; }
            if (action == NavigationAction::Details) { view_model_.refresh_radios(); return true; }
        }

        if (view_model_.current_page() == model::AppPage::Adapters) {
            if (key == 'm' || key == 'M') { view_model_.toggle_monitor(); return true; }
            if (action == NavigationAction::Previous) { view_model_.previous_adapter(); return true; }
            if (action == NavigationAction::Next) { view_model_.next_adapter(); return true; }
            if (action == NavigationAction::Primary) {
                const auto& radios = view_model_.adapters();
                const auto index = view_model_.adapter_cursor();
                if (index >= radios.size()) return true;
                const auto& radio = radios[index];
                using platform::wireless::ConnectionUse;
                const auto use = platform::wireless::connection_use(radio.interface_name);
                if (use == ConnectionUse::Idle) view_model_.select_highlighted_adapter();
                else {
                    pending_adapter_ = radio.stable_id;
                    const auto message = use == ConnectionUse::Connected ? " carries WiFi" :
                        use == ConnectionUse::Hotspot ? " hosts a hotspot" : " link unknown";
                    show_confirmation(radio.interface_name + message + "\nUse this adapter?", "Enter  Use", DialogAction::SelectAdapter);
                }
                return true;
            }
            if (action == NavigationAction::Details) { view_model_.toggle_adapter_details(); return true; }
        }

        if (view_model_.current_page() == model::AppPage::Traffic &&
            (action == NavigationAction::Back || action == NavigationAction::Primary) && key != LV_KEY_ENTER) {
            view_model_.step_traffic_channel(action == NavigationAction::Back ? -1 : 1);
            return true;
        }
        if (view_model_.current_page() == model::AppPage::WifiScan) {
            if (action == NavigationAction::Previous) { view_model_.previous_wifi_scan_page(); return true; }
            if (action == NavigationAction::Next) { view_model_.next_wifi_scan_page(); return true; }
            if (action == NavigationAction::Primary) { view_model_.toggle_wifi_scan_details(); return true; }
            if (action == NavigationAction::Details) {
                const auto warning = view_model_.collection_warning(model::RadioRequirement::Managed);
                if (warning.empty()) view_model_.start_wifi_scan();
                else show_confirmation(warning, "Enter Adapters", DialogAction::OpenAdapters);
                return true;
            }
        }

        if (view_model_.current_page() != model::AppPage::Dashboard && action == NavigationAction::Back) {
            view_model_.show_dashboard_page();
            return true;
        }

        return false;
    }

    static bool global_key_listener(uint32_t key,
                                    const char*,
                                    bool long_pressed,
                                    void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        return controller && controller->handle_key(key, long_pressed);
    }

    viewmodel::BaseViewModel& view_model_;
    AssetManager& assets_;
    std::string pending_adapter_;
    lv_obj_t* confirmation_ = nullptr;
    std::unique_ptr<view::widgets::Toast> toast_;
    std::unique_ptr<view::widgets::HelpPopup> help_popup_;
};

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("template");

    stopping = 0;
    std::signal(SIGTERM, stop_signal);
    std::signal(SIGINT, stop_signal);
    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    auto* standard_font = assets.load_standard_font(14);
    view::apply_lvgl_theme(display,
                           true,
                           standard_font ? standard_font : LV_FONT_DEFAULT);

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    ScreenManager screen_manager(view_model, assets);
    screen_manager.start();
    ShortcutController shortcuts(view_model, assets);

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
    while (running && !stopping
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
    ) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    platform::reset_key_input_state();

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }
    return 0;
}

} // namespace app
