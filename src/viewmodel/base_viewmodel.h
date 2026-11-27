/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_model.h"
#include "censor_text.h"
#include "fox_hunt.h"
#include "marauder_menu.h"
#include "system_info_model.h"
#include "system_diagnostics.h"
#include "subjects.h"

#include "lvgl.h"

#include "wireless_adapters.h"
#include "wireless_scan.h"
#include "radio_power.h"
#include "raw_wifi.h"
#include "pineapple_wifi.h"
#include "remote_id_wifi.h"
#include "pcap_capture.h"
#include "gps_log.h"
#include "map_packs.h"

#include <atomic>
#include <array>
#include <cstddef>
#include <mutex>
#include <future>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <vector>
#include <deque>

namespace viewmodel {

class BaseViewModel {
public:
    enum class FoxTarget { None, AccessPoint, Station, Aircraft, Pineapple };
    struct WardriveSighting {
        bool bluetooth{};
        std::string name, address;
    };
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
    lv_subject_t* pcap_status_subject() { return pcap_status_.native(); }
    bool pcap_active() const { return pcap_capture_.active(); }
    std::string toggle_pcap_capture();
    std::string toggle_selected_pcap_capture();
    bool pcap_action_selected() const {
        return (wifi_view_ == 4 && wifi_operation_index_ == 3) ||
               (wifi_view_ == 5 && wifi_operation_index_ == 2) ||
               (wifi_view_ == 12 && pineapple_action_cursor_ == 3);
    }
    bool pcap_recording_target(platform::pcap::Scope scope, const std::string& mac) const {
        const auto& filter = pcap_capture_.filter();
        return pcap_capture_.active() && filter.scope == scope && filter.mac == mac;
    }
    void show_pcap_files_page();
    void refresh_pcap_files();
    void move_pcap_file(int direction);
    void toggle_pcap_details();
    const auto& pcap_files() const { return pcap_files_; }
    std::size_t pcap_cursor() const { return pcap_cursor_; }
    bool pcap_details() const { return pcap_details_; }
    const auto& pcap_message() const { return pcap_message_; }
    lv_subject_t* pcap_files_subject() { return pcap_files_generation_.native(); }

    bool is_dark_mode() const;
    bool censor_enabled() const { return model_.censor_enabled(); }
    std::string screen_name(const std::string& value) const {
        return censor_enabled() ? model::censor::name(value) : value;
    }
    std::string screen_mac(const std::string& value) const {
        return censor_enabled() ? model::censor::mac(value) : value;
    }
    std::string screen_identifier(const std::string& value) const {
        return censor_enabled() ? model::censor::identifier(value) : value;
    }
    std::string screen_coordinate(const std::string& value) const {
        return censor_enabled() ? model::censor::coordinate(value) : value;
    }
    std::string screen_capture_name(const std::string& value) const {
        return censor_enabled() ? model::censor::capture_name(value) : value;
    }
    std::string screen_text(const std::string& value) const {
        return censor_enabled() ? model::censor::macs_in_text(value) : value;
    }
    model::AppPage current_page() const;
    void show_dashboard_page();
    void show_diagnostics_page();
    void show_adapters_page();
    void show_wifi_scan_page();
    void show_raw_capture_page();
    void show_remote_id_page();
    void show_map_packs_page();
    void move_map_pack(int direction);
    void select_map_pack();
    void step_map_zoom(int direction);
    unsigned map_pack_cursor() const { return map_pack_cursor_; }
    unsigned map_pack() const { return model_.map_pack(); }
    unsigned map_zoom() const { return model_.map_zoom(); }
    std::uint64_t map_local_estimate() const {
        int32_t lat{}, lon{};
        return remote_position(lat, lon) ? platform::maps::local_estimate(lat, lon, model_.map_zoom()) : 0;
    }
    bool map_installing() const { return map_job_.valid(); }
    const std::string& map_message() const { return map_message_; }
    lv_subject_t* map_subject() { return map_generation_.native(); }
    void show_pineapple_page();
    enum class GpsTool { Info, Wardrive, Track };
    void show_gps_tool(GpsTool tool);
    void toggle_gps_recording();
    GpsTool gps_tool() const { return gps_tool_; }
    bool gps_recording() const { return gps_log_.active(); }
    std::size_t gps_record_count() const { return gps_log_.count(); }
    std::size_t bluetooth_record_count() const { return gps_log_.bluetooth_count(); }
    std::size_t bluetooth_observed_count() const { return bluetooth_seen_.size(); }
    const std::deque<WardriveSighting>& wardrive_sightings() const { return wardrive_sightings_; }
    bool bluetooth_scanning() const { return bluetooth_session_.active(); }
    const std::string& gps_tool_message() const { return gps_tool_message_; }
    const std::string& gps_file_name() const { return gps_file_name_; }
    const std::string& gps_receiver_id() const { return gps_id_; }
    std::string gps_receiver_name() const;
    lv_subject_t* gps_tool_subject() { return gps_tool_generation_.native(); }
    const auto& pineapple_items() const { return pineapple_items_; }
    std::size_t pineapple_cursor() const { return pineapple_cursor_; }
    std::size_t pineapple_action_cursor() const { return pineapple_action_cursor_; }
    bool pineapple_session() const { return pineapple_session_; }
    std::size_t remote_count() const { return remote_store_.size(); }
    const RemoteIdRecord* remote_record(std::size_t index) const { return remote_store_.at(index); }
    std::size_t remote_cursor() const { return remote_cursor_; }
    bool remote_details() const { return remote_view_ != 0; }
    int remote_view() const { return remote_view_; }
    std::size_t remote_operation_cursor() const { return remote_operation_cursor_; }
    const std::string& remote_message() const { return remote_message_; }
    unsigned remote_detail_tab() const { return remote_detail_tab_; }
    bool remote_position(int32_t& lat_e7, int32_t& lon_e7) const { return gps_session_.position(lat_e7, lon_e7); }
    void remote_move(int direction);
    void remote_open_details();
    void remote_back();
    const model::FoxHunt& fox_hunt() const { return fox_hunt_; }
    FoxTarget fox_target() const { return fox_target_; }
    const std::string& fox_label() const { return fox_label_; }
    const std::string& fox_vendor() const { return fox_vendor_; }
    const auto& wifi_items() const { return wifi_networks_; }
    size_t wifi_cursor() const { return wifi_scan_detail_index_; }
    const auto& wifi_error() const { return wifi_scan_error_; }
    void step_traffic_channel(int direction) { traffic_step_ += direction; }
    model::RadioRequirement traffic_requirement() const {
        return model_.traffic_monitor_source() ? model::RadioRequirement::Monitor : model::RadioRequirement::Selected;
    }
    bool traffic_monitor_source() const { return model_.traffic_monitor_source(); }
    bool traffic_can_tune() const {
        return !pcap_pinned_frequency() && !monitor_job_.valid() && !monitor_owner_.empty() &&
               monitor_owner_ == model_.selected_adapter_id();
    }
    unsigned pcap_pinned_frequency() const {
        const auto& filter = pcap_capture_.filter();
        return pcap_capture_.active() && filter.scope != platform::pcap::Scope::All ?
            static_cast<unsigned>(filter.frequency_mhz) : 0;
    }
    std::string tune_traffic_channel(unsigned mhz) { return monitor_session_.tune(mhz); }
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
    std::string collection_warning(model::RadioRequirement requirement) const;
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
    int wifi_view() const { return wifi_view_; }
    int wifi_info_tab() const { return wifi_info_tab_; }
    size_t wifi_client_cursor() const { return wifi_client_index_; }
    size_t wifi_operation_cursor() const { return wifi_operation_index_; }
    const auto& wifi_feed_frames() const { return wifi_feed_frames_; }
    size_t wifi_feed_offset() const { return wifi_feed_offset_; }
    uint64_t wifi_feed_count() const { return wifi_feed_count_; }
    const auto& raw_feed_frames() const { return raw_feed_frames_; }
    size_t raw_feed_offset() const { return raw_feed_offset_; }
    uint64_t raw_feed_count() const { return raw_frames_seen_; }
    bool list_builder_active() const { return wifi_list_builder_; }
    bool raw_session() const { return raw_session_; }
    bool raw_auto_hop() const { return raw_auto_hop_; }
    const auto& station_refs() const { return station_refs_; }
    size_t station_cursor() const { return station_cursor_; }
    uint32_t raw_filter_mask() const { return raw_filter_mask_; }
    size_t raw_filter_cursor() const { return raw_filter_cursor_; }
    uint64_t raw_displayed_count() const { return raw_displayed_count_; }
    void toggle_list_builder();
    void toggle_raw_auto_hop();
    void step_raw_channel(int direction);
    void show_station_list();
    void show_raw_filters();
    const auto& wifi_feed_target() const { return wifi_feed_target_; }
    bool wifi_feed_station() const { return wifi_feed_station_; }
    unsigned wifi_tuned_frequency() const { return wifi_tuned_frequency_; }
    void wifi_back();
    void advance_wifi_info_tab();
    bool system_details_open() const { return system_info_.details_expanded(); }
    void start_wifi_scan(bool clear_results = false);
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
    void refresh_pending_monitor_status();
    void publish_diagnostics();
    void publish_wifi_scan();
    void publish_remote();
    void finish_remote_observations(std::vector<platform::remote_id::Packet>& incoming);
    void finish_pineapple_observations(std::vector<platform::wireless::PineappleObservation>& incoming);
    void finish_wifi_scan_if_ready();
    bool begin_wifi_fox_hunt(bool station);
    bool begin_pineapple_fox_hunt();
    void begin_remote_fox_hunt();
    void stop_wifi_capture();
    std::string start_pcap_capture(platform::pcap::Filter filter);
    void advance_wifi_channel();
    static void wifi_scan_timer_cb(lv_timer_t* timer);

    void update_gps_status();
    void sync_gps_rows();
    reactive::StringSubject<16> gps_status_{"Off"};
    platform::MonitorSession monitor_session_;
    std::future<std::string> monitor_job_;
    std::string monitor_owner_;
    std::string monitor_pending_name_;
    bool monitor_target_on_{};
    uint32_t monitor_started_tick_{};
    uint32_t monitor_status_tick_{};
    platform::GpsSession gps_session_;
    platform::BluetoothSession bluetooth_session_;
    std::unordered_set<std::string> bluetooth_seen_;
    std::deque<WardriveSighting> wardrive_sightings_;
    std::string gps_id_, radio_session_;
    platform::gps::Log gps_log_;
    GpsTool gps_tool_{GpsTool::Info};
    bool wardrive_session_{};
    std::string gps_tool_message_, gps_file_name_, bluetooth_message_;
    uint32_t gps_sample_tick_{};
    reactive::IntSubject gps_tool_generation_{0};
    void update_gps_tool();
    void add_wardrive_sighting(bool bluetooth, const std::string& name, const std::string& address);
    void finish_radio_setting();
    void load_radios(std::string key, bool enabled);
    std::future<platform::RadioInventory> radio_job_;
    platform::RadioInventory radio_list_;
    std::size_t radio_cursor_{0};
    std::string resource_message_, pending_uuid_, pending_network_;
    std::future<std::string> map_job_;
    reactive::IntSubject map_generation_{0};
    std::string map_message_;
    unsigned map_pack_cursor_{};
    unsigned map_download_pack_{};
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
    reactive::StringSubject<12> pcap_status_{"P"};
    reactive::IntSubject pcap_files_generation_{0};
    platform::pcap::Capture pcap_capture_;
    std::vector<platform::pcap::FileInfo> pcap_files_;
    std::size_t pcap_cursor_{};
    bool pcap_details_{};
    std::string pcap_message_;
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
    int wifi_view_{0};
    int wifi_info_tab_{0};
    std::size_t wifi_client_index_{};
    std::size_t wifi_operation_index_{};
    std::string wifi_feed_target_;
    bool wifi_feed_station_{};
    std::deque<platform::wireless::FrameSummary> wifi_feed_frames_;
    std::deque<platform::wireless::FrameSummary> raw_feed_frames_;
    std::size_t wifi_feed_offset_{};
    std::size_t raw_feed_offset_{};
    uint64_t wifi_feed_count_{};
    std::atomic<uint64_t> raw_frames_seen_{0};
    std::atomic<bool> wifi_list_builder_{true};
    bool raw_session_{};
    bool raw_auto_hop_{};
    std::size_t raw_channel_index_{};
    bool remote_session_{};
    bool pineapple_session_{};
    std::vector<platform::wireless::PineappleObservation> pineapple_items_;
    std::size_t pineapple_cursor_{};
    std::size_t pineapple_action_cursor_{};
    uint32_t pineapple_ui_tick_{};
    std::array<RemoteIdRecord, 32> remote_storage_{};
    RemoteIdStore remote_store_{remote_storage_.data(), remote_storage_.size()};
    std::size_t remote_cursor_{};
    int remote_view_{}; // 0 map, 1 operations, 2 information, 3 fox hunt
    std::size_t remote_operation_cursor_{};
    std::string remote_message_;
    unsigned remote_detail_tab_{};
    model::FoxHunt fox_hunt_;
    FoxTarget fox_target_{FoxTarget::None};
    std::string fox_label_, fox_vendor_;
    uint32_t fox_ui_tick_{};
    bool station_from_global_{};
    std::vector<std::pair<size_t, size_t>> station_refs_;
    size_t station_cursor_{};
    uint32_t raw_filter_mask_{};
    size_t raw_filter_cursor_{};
    uint64_t raw_displayed_count_{};
    std::atomic<bool> wifi_capture_stop_{false};
    std::atomic<bool> wifi_capture_failed_{false};
    std::atomic<bool> wifi_scan_running_{false};
    std::mutex wifi_scan_mutex_;
    std::vector<platform::wireless::FrameObservation> incoming_frames_;
    std::vector<platform::wireless::FrameSummary> incoming_summaries_;
    std::vector<platform::remote_id::Packet> incoming_remote_;
    std::vector<platform::wireless::PineappleObservation> incoming_pineapple_;
    std::thread wifi_scan_thread_;
    std::vector<unsigned> wifi_frequencies_;
    std::future<std::string> wifi_hop_job_;
    std::size_t wifi_hop_index_{};
    unsigned wifi_tuned_frequency_{};
    unsigned wifi_tune_failed_frequency_{};
    uint32_t wifi_hop_tick_{};
    lv_timer_t* wifi_scan_timer_{nullptr};
};

} // namespace viewmodel
