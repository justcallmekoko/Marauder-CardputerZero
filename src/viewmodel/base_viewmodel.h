/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_model.h"
#include "marauder_menu.h"
#include "system_info_model.h"
#include "system_diagnostics.h"
#include "subjects.h"

#include "lvgl.h"

#include "wireless_adapters.h"
#include "wireless_scan.h"
#include "radio_power.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <future>
#include <chrono>
#include <thread>
#include <vector>

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* marauder_menu_generation_subject();
    lv_subject_t* dashboard_summary_subject();
    lv_subject_t* diagnostics_detail_subject();
    lv_subject_t* diagnostics_generation_subject();
    lv_subject_t* adapter_detail_subject();
    lv_subject_t* wifi_scan_subject();
    lv_subject_t* quit_requested_subject();

    bool is_dark_mode() const;
    model::AppPage current_page() const;
    void show_dashboard_page();
    void show_diagnostics_page();
    void show_adapters_page();
    void show_wifi_scan_page();
    const auto& wifi_items() const { return wifi_networks_; }
    size_t wifi_cursor() const { return wifi_scan_detail_index_; }
    const auto& wifi_error() const { return wifi_scan_error_; }
    void step_traffic_channel(int direction) { traffic_step_ = direction; }
    int take_traffic_step() { int step = traffic_step_; traffic_step_ = 0; return step; }
    bool scanning() const { return wifi_scan_running_; }
    lv_subject_t* inventory_subject() { return inventory_generation_.native(); }
    void toggle_page();
    const char* marauder_menu_title() const;
    std::size_t marauder_menu_item_count() const;
    const model::MarauderMenuItem& marauder_menu_item(std::size_t index) const;
    std::size_t marauder_menu_selected_index() const;
    bool marauder_menu_at_root() const;
    bool marauder_menu_can_go_back() const;
    void move_menu_focus(int dx, int dy);
    std::size_t menu_columns() const { return marauder_menu_.columns(); }
    model::BatteryDisplay battery_display() const { return model_.battery_display(); }
    lv_subject_t* battery_display_subject() { return battery_display_subject_.native(); }
    bool editing_credential() const { return credential_open_; }
    const auto& credential_prompt() const { return credential_prompt_; }
    std::size_t credential_length() const { return credential_input_.size(); }
    void credential_key(uint32_t key);
    void marauder_menu_activate();
    bool marauder_menu_back();
    void refresh_diagnostics();
    void previous_system_info_page();
    void next_system_info_page();
    void toggle_system_info_details();
    const auto& adapter_error() const { return adapter_error_; }
    const auto& adapters() const { return adapters_; }
    std::size_t adapter_cursor() const { return highlighted_adapter_; }
    bool adapter_details_open() const { return adapter_details_expanded_; }
    const std::string& selected_radio_id() const { return model_.selected_adapter_id(); }
    bool screenshots_in_documents() const { return model_.screenshots_in_documents(); }
    std::string setting_value(model::MarauderMenuAction action) const;
    const char* settings_message() const;
    lv_subject_t* gps_status_subject() { return gps_status_.native(); }
    void refresh_radios();
    void move_radio(int direction);
    void toggle_radio();
    const auto& radios() const { return radio_list_.devices; }
    const auto& radio_message() const { return radio_list_.message; }
    std::size_t radio_cursor() const { return radio_cursor_; }
    bool radio_busy() const { return radio_job_.valid(); }
    void refresh_adapters();
    void toggle_monitor();
    bool monitor_busy() const { return monitor_job_.valid(); }
    void previous_adapter();
    void next_adapter();
    void select_highlighted_adapter();
    void toggle_adapter_details();
    bool wifi_details_open() const { return wifi_scan_details_expanded_; }
    bool system_details_open() const { return system_info_.details_expanded(); }
    void start_wifi_scan();
    void previous_wifi_scan_page();
    void next_wifi_scan_page();
    void toggle_wifi_scan_details();
    unsigned reserve_mib() const { return model_.reserve_mib(); }
    bool wake_display();
    void request_quit();

private:
    bool credential_open_{false};
    bool clear_credential_{false};
    model::Credential credential_field_{};
    std::string credential_input_, credential_prompt_;
    void publish_navigation();
    void publish_adapter_detail();
    void publish_diagnostics();
    void publish_wifi_scan();
    void finish_wifi_scan_if_ready();
    static void wifi_scan_timer_cb(lv_timer_t* timer);

    void update_gps_status();
    void sync_gps_rows();
    reactive::StringSubject<16> gps_status_{"Off"};
    platform::MonitorSession monitor_session_;
    std::future<std::string> monitor_job_;
    std::string monitor_owner_;
    platform::GpsSession gps_session_;
    std::string gps_id_, radio_session_;
    void finish_radio_setting();
    void load_radios(std::string key, bool enabled);
    std::future<platform::RadioInventory> radio_job_;
    platform::RadioInventory radio_list_;
    std::size_t radio_cursor_{0};
    std::string resource_message_, pending_uuid_, pending_network_;
    int original_light_{-1}, light_max_{0}, before_dim_{-1};
    bool changed_light_{false};
    uint32_t activity_tick_{};
    bool set_light(int value);
    void idle_display();
    void choose_upload_policy();
    std::string resource_error_;
    model::BaseModel model_;
    model::MarauderMenuModel marauder_menu_;
    reactive::StringSubject<32> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::IntSubject battery_display_subject_;
    reactive::IntSubject marauder_menu_generation_subject_;
    reactive::StringSubject<768> dashboard_summary_subject_;
    reactive::StringSubject<6144> diagnostics_detail_subject_;
    reactive::IntSubject diagnostics_generation_subject_;
    reactive::StringSubject<2048> adapter_detail_subject_;
    reactive::StringSubject<2048> wifi_scan_subject_;
    reactive::BoolSubject quit_requested_subject_;
    model::SystemInfoModel system_info_;
    platform::diagnostics::Snapshot diagnostics_snapshot_;
    std::vector<platform::wireless::AdapterInfo> adapters_;
    std::string adapter_error_;
    std::size_t highlighted_adapter_{0};
    bool adapter_details_expanded_{false};
    std::vector<platform::wireless::WifiNetwork> wifi_networks_;
    reactive::IntSubject inventory_generation_{0};
    int traffic_step_{};
    std::string wifi_scan_interface_;
    std::string wifi_scan_error_;
    std::size_t wifi_scan_detail_index_{0};
    bool wifi_scan_details_expanded_{false};
    std::atomic<bool> wifi_scan_running_{false};
    std::atomic<bool> wifi_scan_ready_{false};
    std::mutex wifi_scan_mutex_;
    platform::wireless::WifiScanResult pending_wifi_scan_;
    std::string pending_wifi_scan_interface_;
    std::vector<platform::wireless::WifiNetwork> incoming_aps_;
    std::thread wifi_scan_thread_;
    uint32_t last_wifi_scan_tick_{};
    lv_timer_t* wifi_scan_timer_{nullptr};
};

} // namespace viewmodel
