/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

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

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

class ShortcutController {
public:
    ShortcutController(viewmodel::BaseViewModel& view_model, AssetManager& assets)
        : view_model_(view_model),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)) {
        toast_->build();
        help_popup_->build();
        platform::set_global_key_listener(global_key_listener, this);
        platform::set_key_release_listener(key_release_listener, this);
    }

    ~ShortcutController() {
        platform::clear_key_release_listener(key_release_listener, this);
        platform::clear_global_key_listener(global_key_listener, this);
    }

private:
    bool handle_key(uint32_t key, bool long_pressed) {
        if (key == platform::kKeyPrintScreen) {
            if (!long_pressed) {
                const auto result = platform::screenshot::capture_active_screen();
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
            switch (escape_action(view_model_.current_page(),
                                  view_model_.marauder_menu_at_root())) {
                case EscapeAction::ExitApplication:
                    toast_->hide();
                    view_model_.request_quit();
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

        if (view_model_.current_page() == model::AppPage::Dashboard) {
            if (long_pressed && key == '4') {
                if (exit_key_pressed_ == key && exit_armed_) {
                    toast_->hide();
                    view_model_.request_quit();
                }
                return true;
            }
            if (long_pressed) return false;
            if (key == LV_KEY_UP || key == 'f' || key == 'F' || key == '5') {
                view_model_.marauder_menu_previous();
                return true;
            }
            if (key == LV_KEY_DOWN || key == 'x' || key == 'X' || key == '7') {
                view_model_.marauder_menu_next();
                return true;
            }
            if (key == LV_KEY_ENTER || key == LV_KEY_RIGHT || key == 'c' || key == 'C' || key == '6') {
                view_model_.marauder_menu_activate();
                return true;
            }
            if (key == '8') {
                view_model_.request_context_help();
                return true;
            }
            if (key == '4') {
                exit_key_pressed_ = key;
                exit_armed_ = view_model_.marauder_menu_at_root();
                if (exit_armed_) {
                    toast_->show_persistent_highlighted("Hold ",
                                                        "4",
                                                        " to exit",
                                                        view::widgets::ToastTone::Warning);
                }
                else {
                    view_model_.marauder_menu_back();
                }
                return true;
            }
            if (key == LV_KEY_LEFT || key == 'z' || key == 'Z') {
                if (!view_model_.marauder_menu_at_root()) view_model_.marauder_menu_back();
                return true;
            }
        }

        if (view_model_.current_page() == model::AppPage::Diagnostics && !long_pressed) {
            if (key == LV_KEY_UP || key == 'f' || key == 'F') { view_model_.previous_system_info_page(); return true; }
            if (key == LV_KEY_DOWN || key == 'x' || key == 'X') { view_model_.next_system_info_page(); return true; }
            if (key == LV_KEY_ENTER || key == LV_KEY_RIGHT || key == 'c' || key == 'C') { view_model_.refresh_diagnostics(); return true; }
        }

        if (view_model_.current_page() == model::AppPage::Adapters && !long_pressed) {
            if (key == LV_KEY_UP || key == 'f' || key == 'F') { view_model_.previous_adapter(); return true; }
            if (key == LV_KEY_DOWN || key == 'x' || key == 'X') { view_model_.next_adapter(); return true; }
            if (key == LV_KEY_ENTER || key == LV_KEY_RIGHT || key == 'c' || key == 'C') { view_model_.select_highlighted_adapter(); return true; }
        }

        if (view_model_.current_page() == model::AppPage::WifiScan && !long_pressed) {
            if (key == LV_KEY_UP || key == 'f' || key == 'F') { view_model_.previous_wifi_scan_page(); return true; }
            if (key == LV_KEY_DOWN || key == 'x' || key == 'X') { view_model_.next_wifi_scan_page(); return true; }
            if (key == LV_KEY_ENTER || key == LV_KEY_RIGHT || key == 'c' || key == 'C') { view_model_.start_wifi_scan(); return true; }
        }

        if (view_model_.current_page() != model::AppPage::Dashboard && !long_pressed &&
            (key == LV_KEY_LEFT || key == 'z' || key == 'Z')) {
            view_model_.show_dashboard_page();
            return true;
        }

        return false;
    }

    void handle_release(uint32_t key) {
        if (key != exit_key_pressed_) {
            return;
        }

        if (exit_armed_) {
            toast_->hide();
        }
        exit_key_pressed_ = 0;
        exit_armed_ = false;
    }

    static bool global_key_listener(uint32_t key,
                                    const char*,
                                    bool long_pressed,
                                    void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        return controller && controller->handle_key(key, long_pressed);
    }

    static void key_release_listener(uint32_t key, const char*, void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        if (controller) {
            controller->handle_release(key);
        }
    }

    viewmodel::BaseViewModel& view_model_;
    std::unique_ptr<view::widgets::Toast> toast_;
    std::unique_ptr<view::widgets::HelpPopup> help_popup_;
    uint32_t exit_key_pressed_{0};
    bool exit_armed_{false};
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
    while (running
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
