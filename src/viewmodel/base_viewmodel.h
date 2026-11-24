/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_model.h"
#include "subjects.h"

#include "lvgl.h"

#include "wireless_adapters.h"
#include "wireless_scan.h"

#include <atomic>
#include <cstddef>
#include <mutex>
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
    lv_subject_t* dashboard_summary_subject();
    lv_subject_t* diagnostics_detail_subject();
    lv_subject_t* diagnostics_generation_subject();
    lv_subject_t* adapter_detail_subject();
    lv_subject_t* wifi_scan_subject();
    lv_subject_t* quit_requested_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();
    model::AppPage current_page() const;
    void show_dashboard_page();
    void show_diagnostics_page();
    void show_adapters_page();
    void show_wifi_scan_page();
    void toggle_page();
    void refresh_diagnostics();
    void refresh_adapters();
    void previous_adapter();
    void next_adapter();
    void select_highlighted_adapter();
    void toggle_adapter_details();
    void start_wifi_scan();
    void previous_wifi_scan_page();
    void next_wifi_scan_page();
    void request_quit();

private:
    void publish_navigation();
    void publish_adapter_detail();
    void publish_wifi_scan();
    void finish_wifi_scan_if_ready();
    static void wifi_scan_timer_cb(lv_timer_t* timer);

    model::BaseModel model_;
    reactive::StringSubject<32> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::StringSubject<768> dashboard_summary_subject_;
    reactive::StringSubject<6144> diagnostics_detail_subject_;
    reactive::IntSubject diagnostics_generation_subject_;
    reactive::StringSubject<2048> adapter_detail_subject_;
    reactive::StringSubject<2048> wifi_scan_subject_;
    reactive::BoolSubject quit_requested_subject_;
    std::vector<platform::wireless::AdapterInfo> adapters_;
    std::string adapter_error_;
    std::size_t highlighted_adapter_{0};
    bool adapter_details_expanded_{false};
    std::vector<platform::wireless::WifiNetwork> wifi_networks_;
    std::string wifi_scan_interface_;
    std::string wifi_scan_error_;
    std::size_t wifi_scan_page_{0};
    std::atomic<bool> wifi_scan_running_{false};
    std::atomic<bool> wifi_scan_ready_{false};
    std::mutex wifi_scan_mutex_;
    platform::wireless::WifiScanResult pending_wifi_scan_;
    std::string pending_wifi_scan_interface_;
    std::thread wifi_scan_thread_;
    lv_timer_t* wifi_scan_timer_{nullptr};
};

} // namespace viewmodel
