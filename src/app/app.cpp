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
        : view_model_(view_model),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)) {
        toast_->build();
        help_popup_->build();
        platform::set_global_key_listener(global_key_listener, this);
    }

    ~ShortcutController() {
        platform::clear_global_key_listener(global_key_listener, this);
    }

private:
    bool handle_key(uint32_t key, bool long_pressed) {
        if (view_model_.wake_display()) return true;
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

        if (long_pressed) return false;

        const auto action = key == LV_KEY_UP ? NavigationAction::Previous :
                            key == LV_KEY_DOWN ? NavigationAction::Next :
                            (key == LV_KEY_ENTER || key == LV_KEY_RIGHT) ? NavigationAction::Primary :
                            key == LV_KEY_LEFT ? NavigationAction::Back : navigation_action(key);
        if (view_model_.current_page() == model::AppPage::Dashboard) {
            if (key == LV_KEY_ENTER) view_model_.marauder_menu_activate();
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
            if (action == NavigationAction::Previous) { view_model_.previous_adapter(); return true; }
            if (action == NavigationAction::Next) { view_model_.next_adapter(); return true; }
            if (action == NavigationAction::Primary) { view_model_.select_highlighted_adapter(); return true; }
            if (action == NavigationAction::Details) { view_model_.toggle_adapter_details(); return true; }
        }

        if (view_model_.current_page() == model::AppPage::WifiScan) {
            if (action == NavigationAction::Previous) { view_model_.previous_wifi_scan_page(); return true; }
            if (action == NavigationAction::Next) { view_model_.next_wifi_scan_page(); return true; }
            if (action == NavigationAction::Primary) { view_model_.start_wifi_scan(); return true; }
            if (action == NavigationAction::Details) { view_model_.toggle_wifi_scan_details(); return true; }
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
