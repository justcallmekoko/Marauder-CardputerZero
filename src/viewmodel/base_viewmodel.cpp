/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "display_format.h"
#include "system_diagnostics.h"
#include "radio_power.h"
#include "channel_survey.h"
#include "wardrive_plan.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <chrono>
#include <fstream>
#include <cstdio>

#include <algorithm>
#include <sstream>
#include <system_error>
#include <iterator>
#include <memory>
#include <set>
#include <tuple>
#include <cstring>
#if defined(__linux__) && !USE_DESKTOP
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#endif

namespace viewmodel {
namespace {
constexpr const char* brightness_path = "/sys/class/backlight/backlight/brightness";
int backlight_value(const char* path) {
    int n = -1;
    std::ifstream(path) >> n;
    return n;
}

std::pair<std::string,std::string> active_wifi_profile() {
#if defined(__linux__) && !USE_DESKTOP
    FILE* pipe = popen("LC_ALL=C /usr/bin/nmcli -w 1 -t -e no -f TYPE,UUID,NAME connection show --active 2>/dev/null", "r");
    if (!pipe) return {};
    std::pair<std::string,std::string> profile;
    char buffer[1024];
    unsigned count = 0;
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        const std::string prefix = "802-11-wireless:";
        if (line.compare(0, prefix.size(), prefix) != 0) continue;
        const auto sep = line.find(':', prefix.size());
        if (sep == std::string::npos) continue;
        auto uuid = line.substr(prefix.size(), sep - prefix.size());
        if (uuid.size() != 36 || uuid.find_first_not_of("abcdefABCDEF0123456789-") != std::string::npos) continue;
        auto name = line.substr(sep + 1);
        name.erase(name.find_last_not_of("\r\n") + 1);
        profile = {uuid, name};
        ++count;
    }
    if (pclose(pipe) == 0 && count == 1) return profile;
#endif
    return {};
}

int page_to_int(model::AppPage page) { return static_cast<int>(page); }
std::string mac_text(const uint8_t mac[6]) {
    char text[18];
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return text;
}
void merge_remote_record(RemoteIdRecord& target, const RemoteIdRecord& source) {
    if (source.hasUasId && remoteIdShouldReplaceBasicId(target.idType, target.hasUasId, source.idType)) {
        std::memcpy(target.uasId, source.uasId, sizeof(target.uasId));
        target.hasUasId = true;
        target.idType = source.idType;
        target.uaType = source.uaType;
    }
    if (source.hasOperatorId) {
        std::memcpy(target.operatorId, source.operatorId, sizeof(target.operatorId));
        target.hasOperatorId = true;
    }
    if (source.hasDescription) {
        std::memcpy(target.description, source.description, sizeof(target.description));
        target.hasDescription = true;
    }
    if (source.hasLocation) {
        remoteIdUpdateLocation(target, source.latitudeE7, source.longitudeE7);
        target.altitudeGeoM = source.altitudeGeoM;
        target.altitudePressureM = source.altitudePressureM;
        target.heightM = source.heightM;
        target.horizontalSpeedMps = source.horizontalSpeedMps;
        target.verticalSpeedMps = source.verticalSpeedMps;
        target.directionDeg = source.directionDeg;
        target.operationalStatus = source.operationalStatus;
        target.altitudeGeoValid = source.altitudeGeoValid;
        target.horizontalSpeedValid = source.horizontalSpeedValid;
        target.directionValid = source.directionValid;
    }
    if (source.hasOperatorLocation) {
        target.hasOperatorLocation = true;
        target.operatorLatitudeE7 = source.operatorLatitudeE7;
        target.operatorLongitudeE7 = source.operatorLongitudeE7;
        target.operatorAltitudeGeoM = source.operatorAltitudeGeoM;
        target.operatorAltitudeValid = source.operatorAltitudeValid;
    }
    target.hasAuthentication |= source.hasAuthentication;
}
}

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      battery_display_subject_(static_cast<int>(model_.battery_display())),
      marauder_menu_generation_subject_(0),
      dashboard_summary_subject_("Collecting system information..."),
      diagnostics_detail_subject_("Collecting system information..."),
      diagnostics_generation_subject_(0),
      adapter_detail_subject_("Collecting wireless adapters..."),
      wifi_scan_subject_("SELECT AN ADAPTER FIRST"),
      quit_requested_subject_(false) {
    lv_image_cache_resize(model_.cache_kib() * 1024U, false);
    wifi_scan_timer_ = lv_timer_create(wifi_scan_timer_cb, 100, this);
    original_light_ = backlight_value(brightness_path);
    light_max_ = backlight_value("/sys/class/backlight/backlight/max_brightness");
    gps_id_ = model::saved_gps_id();
    if (!gps_id_.empty())
        gps_status_.set(gps_session_.start(gps_id_) ? "Wait" : "Unavailable");
    radio_session_ = platform::begin_radio_session();
    refresh_radios();
    activity_tick_ = lv_tick_get();
    if (model_.brightness() && original_light_ >= 0 && light_max_ > 0)
        set_light(std::max(1, static_cast<int>(model_.brightness()) * light_max_ / 100));
    refresh_diagnostics();
#if defined(__linux__) && !USE_DESKTOP
    std::vector<std::tuple<std::string, std::string, platform::MonitorSession*>> restore;
    for (const auto& adapter : adapters_) {
        if (!model_.monitor_preferences().count(adapter.stable_id) ||
            adapter.monitor_mode != platform::wireless::CapabilityState::Supported) continue;
        const auto use = platform::wireless::connection_use(adapter.interface_name);
        if (use == platform::wireless::ConnectionUse::Connected ||
            use == platform::wireless::ConnectionUse::Hotspot) continue;
        restore.emplace_back(adapter.stable_id, adapter.interface_name,
                             &monitor_session_for(adapter.stable_id));
    }
    if (!restore.empty()) {
        try {
            monitor_restore_job_ = std::async(std::launch::async, [restore] {
                std::vector<std::string> started;
                for (const auto& [id, name, session] : restore)
                    if (session->start(name) == "Monitor on") started.push_back(id);
                return started;
            });
        } catch (const std::system_error&) {
            adapter_error_ = "Saved monitor restore unavailable";
        }
    } else if (!model_.monitor_preferences().empty())
        adapter_error_ = "Saved monitor adapters missing or busy";
#endif
}

BaseViewModel::~BaseViewModel() {
    if (map_job_.valid()) { map_progress_.cancel = true; map_job_.wait(); }
    if (wardrive_upload_job_.valid()) wardrive_upload_job_.wait();
    if (credential_check_job_.valid()) credential_check_job_.wait();
    bluetooth_session_.stop();
    gps_log_.stop();
    pcap_capture_.stop();
    if (changed_light_) set_light(original_light_);
    if (wifi_scan_timer_) {
        lv_timer_delete(wifi_scan_timer_);
        wifi_scan_timer_ = nullptr;
    }
    stop_wifi_capture();
    if (radio_job_.valid()) radio_job_.wait();
    if (monitor_job_.valid()) monitor_job_.wait();
    if (monitor_restore_job_.valid()) monitor_restore_job_.wait();
    for (auto& [id, session] : monitor_sessions_) session->stop();
    gps_session_.stop();
    platform::end_radio_session(radio_session_);
}
lv_subject_t* BaseViewModel::title_subject() { return title_subject_.native(); }
lv_subject_t* BaseViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }
lv_subject_t* BaseViewModel::current_page_subject() { return current_page_subject_.native(); }
lv_subject_t* BaseViewModel::marauder_menu_generation_subject() { return marauder_menu_generation_subject_.native(); }
lv_subject_t* BaseViewModel::dashboard_summary_subject() { return dashboard_summary_subject_.native(); }
lv_subject_t* BaseViewModel::diagnostics_detail_subject() { return diagnostics_detail_subject_.native(); }
lv_subject_t* BaseViewModel::diagnostics_generation_subject() { return diagnostics_generation_subject_.native(); }
lv_subject_t* BaseViewModel::adapter_detail_subject() { return adapter_detail_subject_.native(); }
lv_subject_t* BaseViewModel::wifi_scan_subject() { return wifi_scan_subject_.native(); }
lv_subject_t* BaseViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }
platform::MonitorSession& BaseViewModel::monitor_session_for(const std::string& stable_id) {
    auto& session = monitor_sessions_[stable_id];
    if (!session) session = std::make_unique<platform::MonitorSession>();
    return *session;
}
bool BaseViewModel::is_dark_mode() const { return model_.dark_mode(); }
model::AppPage BaseViewModel::current_page() const { return model_.current_page(); }
void BaseViewModel::show_dashboard_page() {
    bluetooth_session_.stop();
    gps_log_.stop();
    wardrive_session_ = false;
    stop_wifi_capture();
    model_.set_current_page(model::AppPage::Dashboard);
    publish_navigation();
}
void BaseViewModel::show_diagnostics_page() {
    stop_wifi_capture();
    refresh_diagnostics();
    model_.set_current_page(model::AppPage::Diagnostics);
    publish_navigation();
}
void BaseViewModel::show_adapters_page() {
    bluetooth_session_.stop();
    gps_log_.stop();
    wardrive_session_ = false;
    stop_wifi_capture();
    refresh_adapters();
    model_.set_current_page(model::AppPage::Adapters);
    publish_navigation();
}
void BaseViewModel::show_wifi_scan_page() {
    raw_session_ = false;
    remote_session_ = false;
    pineapple_session_ = false;
    model_.set_current_page(model::AppPage::WifiScan);
    publish_navigation();
    if (!wifi_scan_running_) start_wifi_scan();
}
void BaseViewModel::show_raw_capture_page() {
    raw_session_ = true;
    remote_session_ = false;
    pineapple_session_ = false;
    raw_auto_hop_ = false;
    wifi_view_ = 7;
    wifi_scan_details_expanded_ = true;
    model_.set_current_page(model::AppPage::WifiScan);
    publish_navigation();
    if (!wifi_scan_running_) start_wifi_scan();
}
void BaseViewModel::show_bluetooth_page(uint32_t filter) {
    stop_wifi_capture();
    gps_log_.stop();
    wardrive_session_ = false;
    bluetooth_session_.stop();
    bluetooth_feed_.clear();
    const auto radio = std::find_if(radio_list_.devices.begin(), radio_list_.devices.end(),
        [](const auto& item) { return item.kind == "Bluetooth" && item.state == "On"; });
    const std::string adapter_id = radio == radio_list_.devices.end() ? "" : radio->id;
    if (adapter_id != bluetooth_adapter_id_) {
        bluetooth_devices_.clear();
        bluetooth_index_.clear();
        bluetooth_total_ = 0;
        bluetooth_adapter_id_ = adapter_id;
    }
    bluetooth_fox_.reset();
    bluetooth_feed_offset_ = 0;
    bluetooth_filter_ = filter;
    bluetooth_builder_ = filter != 0;
    bluetooth_view_ = 0;
    bluetooth_cursor_ = bluetooth_action_cursor_ = 0;
    bluetooth_filter_cursor_ = 0;
    bluetooth_start_stop();
    model_.set_current_page(model::AppPage::BluetoothCapture);
    publish_navigation();
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::bluetooth_start_stop() {
    if (bluetooth_session_.active()) {
        bluetooth_session_.stop();
        bluetooth_status_ = "Paused";
    } else {
        const auto radio = std::find_if(radio_list_.devices.begin(), radio_list_.devices.end(),
            [](const auto& item) { return item.kind == "Bluetooth" && item.state == "On"; });
        if (radio == radio_list_.devices.end()) bluetooth_status_ = "Enable Bluetooth in Device > Radio setup";
        else bluetooth_status_ = bluetooth_session_.start(radio->name, true) ? "Listening" : "Bluetooth scan unavailable";
    }
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::bluetooth_toggle_builder() {
    bluetooth_builder_ = !bluetooth_builder_;
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::bluetooth_show_filters() {
    bluetooth_view_ = bluetooth_view_ == 1 ? 0 : 1;
    bluetooth_filter_cursor_ = 0;
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::bluetooth_show_devices() {
    bluetooth_view_ = 2;
    bluetooth_cursor_ = 0;
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
std::vector<std::size_t> BaseViewModel::bluetooth_visible_indices() const {
    std::vector<std::size_t> visible;
    for (std::size_t i = 0; i < bluetooth_devices_.size(); ++i)
        if (!bluetooth_filter_ || (bluetooth_devices_[i].classification.matches & bluetooth_filter_))
            visible.push_back(i);
    return visible;
}
const BaseViewModel::BluetoothRecord* BaseViewModel::bluetooth_selected() const {
    const auto visible = bluetooth_visible_indices();
    return bluetooth_cursor_ < visible.size() ? &bluetooth_devices_[visible[bluetooth_cursor_]] : nullptr;
}
void BaseViewModel::bluetooth_move(int direction) {
    if (bluetooth_view_ >= 4) return;
    if (bluetooth_view_ == 0) {
        if (direction < 0 && bluetooth_feed_offset_ + 1 < bluetooth_feed_.size()) ++bluetooth_feed_offset_;
        if (direction > 0 && bluetooth_feed_offset_) --bluetooth_feed_offset_;
        bluetooth_generation_.set(bluetooth_generation_.value() + 1);
        return;
    }
    std::size_t* cursor = bluetooth_view_ == 1 ? &bluetooth_filter_cursor_ :
        bluetooth_view_ == 3 ? &bluetooth_action_cursor_ : &bluetooth_cursor_;
    const auto count = bluetooth_view_ == 1 ? static_cast<std::size_t>(platform::ble::Kind::Count) + 1 :
        bluetooth_view_ == 3 ? 2u : bluetooth_visible_indices().size();
    if (count) *cursor = (*cursor + count + (direction < 0 ? count - 1 : 1)) % count;
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::bluetooth_select() {
    if (bluetooth_view_ == 0) { bluetooth_start_stop(); return; }
    if (bluetooth_view_ == 1) {
        bluetooth_filter_ = bluetooth_filter_cursor_ ?
            bluetooth_filter_ ^ platform::ble::bit(static_cast<platform::ble::Kind>(bluetooth_filter_cursor_ - 1)) : 0;
        bluetooth_feed_.clear();
        bluetooth_feed_offset_ = 0;
        bluetooth_cursor_ = 0;
    } else if (bluetooth_view_ == 2 && bluetooth_selected()) {
        bluetooth_action_cursor_ = 0;
        bluetooth_view_ = 3;
    } else if (bluetooth_view_ == 3 && bluetooth_selected()) {
        bluetooth_view_ = bluetooth_action_cursor_ ? 5 : 4;
        if (bluetooth_view_ == 5) bluetooth_fox_.begin(bluetooth_selected()->observation.address, 0);
    }
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::bluetooth_back() {
    bluetooth_view_ = bluetooth_view_ >= 4 ? 3 : bluetooth_view_ == 3 ? 2 : 0;
    if (bluetooth_view_ != 5) bluetooth_fox_.reset();
    bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::update_bluetooth_capture() {
    if (model_.current_page() != model::AppPage::BluetoothCapture || !bluetooth_session_.active()) return;
    std::string state;
    bool changed = false;
    for (auto& sighting : bluetooth_session_.poll(state)) {
        ++bluetooth_total_;
        const auto classified = platform::ble::classify(sighting);
        if (!bluetooth_filter_ || (classified.matches & bluetooth_filter_)) {
            if (bluetooth_feed_.size() == 64) bluetooth_feed_.pop_front();
            bluetooth_feed_.push_back({sighting, classified, 1, lv_tick_get()});
            changed = true;
        }
        if (bluetooth_builder_) {
            const auto key = sighting.address_type + ":" + sighting.address;
            const auto known = bluetooth_index_.find(key);
            if (known != bluetooth_index_.end()) {
                auto& record = bluetooth_devices_[known->second];
                const auto prior = record.classification;
                record.observation = sighting;
                record.classification = classified;
                record.classification.matches |= prior.matches;
                if (!classified.matches && prior.matches) {
                    record.classification.type = prior.type;
                    record.classification.evidence = prior.evidence;
                    record.classification.detail = prior.detail;
                }
                if (record.classification.vendor == "Unknown") record.classification.vendor = prior.vendor;
                ++record.sightings;
                record.last_seen = lv_tick_get();
            } else if (bluetooth_devices_.size() < 512) {
                bluetooth_index_[key] = bluetooth_devices_.size();
                bluetooth_devices_.push_back({sighting, classified, 1, lv_tick_get()});
            } else bluetooth_status_ = "Device list full (512)";
            changed = true;
        }
        if (bluetooth_view_ == 5)
            bluetooth_fox_.observe(sighting.address, 0, sighting.rssi, true, lv_tick_get());
    }
    if (!state.empty() && state != "Scanning") { bluetooth_status_ = state; changed = true; }
    if (changed) bluetooth_generation_.set(bluetooth_generation_.value() + 1);
}
void BaseViewModel::show_remote_id_page() {
    remote_session_ = true;
    raw_session_ = false;
    pineapple_session_ = false;
    remote_view_ = 0;
    remote_message_.clear();
    model_.set_current_page(model::AppPage::RemoteId);
    publish_navigation();
    if (!wifi_scan_running_) start_wifi_scan();
}
void BaseViewModel::show_map_packs_page() {
    map_pack_cursor_ = model_.map_pack();
    map_preview_zoom_ = model_.map_zoom();
    map_message_ = model_.censor_enabled() ? "Censor On: Remote ID hides tiles" : "Choose a map pack";
    model_.set_current_page(model::AppPage::MapPacks);
    publish_navigation();
    map_generation_.set(map_generation_.value() + 1);
}
void BaseViewModel::move_map_pack(int direction) {
    if (map_job_.valid()) return;
    map_pack_cursor_ = direction < 0 ? (map_pack_cursor_ + platform::maps::pack_count() - 1) % platform::maps::pack_count() :
        (map_pack_cursor_ + 1) % platform::maps::pack_count();
    if (map_pack_cursor_ == 2 && map_preview_zoom_ < 10) map_preview_zoom_ = 14;
    if (map_pack_cursor_ < 2 && map_preview_zoom_ > 6) map_preview_zoom_ = 6;
    if (map_pack_cursor_ >= 3 && map_preview_zoom_ > 10) map_preview_zoom_ = 6;
    map_generation_.set(map_generation_.value() + 1);
}
void BaseViewModel::select_map_pack() {
    if (map_job_.valid()) return;
    map_progress_.cancel = false;
    map_progress_.done = 0;
    map_deleting_ = false;
    if (map_pack_cursor_ == 2) {
        int32_t lat{}, lon{};
        if (!remote_position(lat, lon)) map_message_ = "GPS fix required for local map";
        else {
            map_download_pack_ = 2;
            map_download_zoom_ = map_preview_zoom_;
            map_progress_.total = map_local_estimate() / platform::maps::kEstimatedTileBytes;
            map_message_ = "Downloading nearby tiles...";
            try { map_job_ = std::async(std::launch::async, platform::maps::install_local, lat, lon, map_download_zoom_, &map_progress_); }
            catch (const std::system_error&) { map_message_ = "Download worker unavailable"; }
        }
    } else if (platform::maps::installed_zoom(map_pack_cursor_) >= map_preview_zoom_) {
        map_message_ = model_.set_map_zoom(map_preview_zoom_) && model_.set_map_pack(map_pack_cursor_) ?
            (model_.censor_enabled() ? "Selected; Censor hides map tiles" : "Map selected for Remote ID") : "Could not save map";
    } else {
        map_message_ = "Downloading map tiles...";
        map_download_pack_ = map_pack_cursor_;
        map_download_zoom_ = map_preview_zoom_;
        map_progress_.total = map_download_pack_ >= 3 ? map_area_estimate() / platform::maps::kEstimatedTileBytes :
            platform::maps::pack(map_download_pack_).archive_bytes;
        try {
            if (map_pack_cursor_ >= 3) map_job_ = std::async(std::launch::async, platform::maps::install_area, map_pack_cursor_, map_download_zoom_, &map_progress_);
            else map_job_ = std::async(std::launch::async, platform::maps::install, map_pack_cursor_, &map_progress_);
        }
        catch (const std::system_error&) { map_message_ = "Download worker unavailable"; }
    }
    map_generation_.set(map_generation_.value() + 1);
}
void BaseViewModel::step_map_zoom(int direction) {
    if (map_job_.valid()) return;
    const auto zoom = static_cast<int>(map_preview_zoom_) + direction;
    const int min_zoom = map_pack_cursor_ == 2 ? 6 : 1;
    const int max_zoom = map_pack_cursor_ == 2 ? 16 : map_pack_cursor_ >= 3 ? 10 : 6;
    if (zoom < min_zoom || zoom > max_zoom) {
        map_message_ = "Zoom limit z" + std::to_string(min_zoom) + "-" + std::to_string(max_zoom);
        map_generation_.set(map_generation_.value() + 1);
        return;
    }
    map_preview_zoom_ = static_cast<unsigned>(zoom);
    map_message_ = "Zoom saved when pack is selected";
    map_generation_.set(map_generation_.value() + 1);
}
void BaseViewModel::cancel_map_download() {
    if (!map_job_.valid() || map_deleting_) return;
    map_progress_.cancel = true;
    map_message_ = "Pausing download...";
    map_generation_.set(map_generation_.value() + 1);
}
void BaseViewModel::delete_map_pack(unsigned index) {
    if (map_job_.valid() || index != map_pack_cursor_ || index >= platform::maps::pack_count()) return;
    map_deleting_ = true;
    map_download_pack_ = index;
    map_message_ = "Deleting map files...";
    try { map_job_ = std::async(std::launch::async, platform::maps::erase, index); }
    catch (const std::system_error&) { map_deleting_ = false; map_message_ = "Delete worker unavailable"; }
    map_generation_.set(map_generation_.value() + 1);
}
void BaseViewModel::show_pineapple_page() {
    raw_session_ = false;
    remote_session_ = false;
    pineapple_session_ = true;
    wifi_view_ = 11;
    wifi_scan_details_expanded_ = true;
    model_.set_current_page(model::AppPage::WifiScan);
    publish_navigation();
    if (!wifi_scan_running_) start_wifi_scan();
}
void BaseViewModel::show_gps_tool(GpsTool tool) {
    bluetooth_session_.stop();
    bluetooth_seen_.clear();
    wardrive_channels_.clear();
    wardrive_sightings_.clear();
    wardrive_route_.clear();
    wardrive_map_points_.clear();
    wardrive_plotted_.clear();
    wardrive_view_ = 0;
    wardrive_channel_offset_ = 0;
    gps_log_.reset();
    stop_wifi_capture();
    gps_tool_ = tool;
    wardrive_session_ = tool == GpsTool::Wardrive;
    gps_tool_message_ = gps_id_.empty() ? "Enable a GPS receiver in Device > Radio setup" :
                        tool == GpsTool::Info ? "Live receiver information" : "Enter to start recording";
    gps_file_name_.clear();
    bluetooth_message_.clear();
    gps_sample_tick_ = 0;
    if (wardrive_session_) {
        raw_session_ = false;
        remote_session_ = false;
        pineapple_session_ = false;
        start_wifi_scan(true);
        if (!wifi_scan_error_.empty()) gps_tool_message_ = wifi_scan_error_;
    }
    model_.set_current_page(model::AppPage::GpsTool);
    publish_navigation();
    if (wardrive_session_) toggle_gps_recording();
    gps_tool_generation_.set(gps_tool_generation_.value() + 1);
}
std::string BaseViewModel::gps_receiver_name() const {
    for (const auto& radio : radio_list_.devices)
        if (radio.kind == "GPS" && radio.id == gps_id_)
            return radio.name + (radio.product.empty() ? "" : " / " + radio.product);
    return gps_id_.empty() ? "No receiver" : "Selected receiver";
}
void BaseViewModel::toggle_gps_recording() {
    if (gps_tool_ == GpsTool::Info) return;
    const auto bluetooth_radio = std::find_if(radio_list_.devices.begin(), radio_list_.devices.end(),
        [](const auto& item) { return item.kind == "Bluetooth" && item.state == "On"; });
    const bool bluetooth_available = bluetooth_radio != radio_list_.devices.end();
    if (gps_log_.active()) {
        bluetooth_session_.stop();
        bluetooth_message_.clear();
        const auto count = gps_log_.count();
        gps_log_.stop();
        if (wardrive_session_) stop_wifi_capture();
        gps_tool_message_ = "Saved " + std::to_string(count) + " records";
    } else if (gps_id_.empty()) {
        gps_tool_message_ = "Enable a GPS receiver in Device > Radio setup";
    } else {
        if (wardrive_session_ && !wifi_scan_running_) start_wifi_scan();
        if (wardrive_session_ && !wifi_scan_running_ && !bluetooth_available) {
            gps_tool_message_ = wifi_scan_error_.empty() ? "Monitor capture unavailable" : wifi_scan_error_;
            gps_tool_generation_.set(gps_tool_generation_.value() + 1);
            return;
        }
        std::string error;
        if (gps_log_.start(gps_tool_ == GpsTool::Track ? platform::gps::LogKind::Track :
                           platform::gps::LogKind::Wardrive, model_.reserve_mib(), error)) {
            if (gps_tool_ == GpsTool::Wardrive) {
                bluetooth_seen_.clear();
                wardrive_channels_.clear();
                wardrive_channel_offset_ = 0;
                wardrive_sightings_.clear();
                wardrive_route_.clear();
                wardrive_map_points_.clear();
                wardrive_plotted_.clear();
            }
            gps_file_name_ = gps_log_.path().filename().string();
            gps_tool_message_ = "Recording; waiting for GPS fix";
            gps_sample_tick_ = 0;
            if (gps_tool_ == GpsTool::Wardrive) {
                bluetooth_message_ = "Enable Bluetooth in Device";
                if (bluetooth_available)
                    bluetooth_message_ = bluetooth_session_.start(bluetooth_radio->name) ?
                        "" : "Bluetooth scan unavailable";
            }
        } else gps_tool_message_ = error;
    }
    gps_tool_generation_.set(gps_tool_generation_.value() + 1);
}
void BaseViewModel::update_gps_tool() {
    if (model_.current_page() != model::AppPage::GpsTool) return;
    if (wardrive_session_ && gps_log_.active()) {
        std::string bluetooth_state;
        const auto before = bluetooth_seen_.size();
        for (const auto& item : bluetooth_session_.poll(bluetooth_state)) {
            int32_t lat{}, lon{};
            if (!gps_session_.position(lat, lon)) continue;
            if (!gps_log_.bluetooth_device(item.address, item.name, item.rssi,
                                            item.address_type, lat, lon)) {
                bluetooth_session_.stop();
                gps_tool_message_ = "GPS file write failed";
                break;
            }
            if (bluetooth_seen_.insert(item.address_type + ":" + item.address).second)
                add_wardrive_sighting(true, item.name, item.address);
            add_wardrive_map_point(true, item.address_type + ":" + item.address, lat, lon);
        }
        if (!bluetooth_state.empty() && bluetooth_state != "Scanning")
            bluetooth_message_ = bluetooth_state;
        if (bluetooth_seen_.size() != before)
            gps_tool_generation_.set(gps_tool_generation_.value() + 1);
    }
    const auto now = lv_tick_get();
    if (gps_sample_tick_ && lv_tick_elaps(gps_sample_tick_) < 1000) return;
    gps_sample_tick_ = now;
    int32_t lat{}, lon{};
    const bool fix = gps_session_.position(lat, lon);
    if (gps_log_.active() && fix) {
        if (wardrive_session_) {
            if (wardrive_route_.size() == 2048) wardrive_route_.pop_front();
            wardrive_route_.push_back({lat, lon, false});
        }
        bool written = true;
        if (gps_tool_ == GpsTool::Track) written = gps_log_.position(lat, lon);
        if (!written) gps_tool_message_ = "GPS file write failed";
        else if (gps_tool_ == GpsTool::Track)
            gps_tool_message_ = "Recording precise GPS coordinates";
        else if (wifi_scan_running_ && bluetooth_session_.active())
            gps_tool_message_ = "Recording WiFi + Bluetooth";
        else if (wifi_scan_running_)
            gps_tool_message_ = "WiFi only: " + bluetooth_message_;
        else if (bluetooth_session_.active())
            gps_tool_message_ = "Recording Bluetooth; WiFi unavailable";
        else gps_tool_message_ = "Collection adapters unavailable";
    } else if (gps_log_.active()) gps_tool_message_ = "Recording; waiting for GPS fix";
    gps_tool_generation_.set(gps_tool_generation_.value() + 1);
}
void BaseViewModel::add_wardrive_sighting(bool bluetooth, const std::string& name, const std::string& address) {
    if (wardrive_sightings_.size() == 16) wardrive_sightings_.pop_front();
    wardrive_sightings_.push_back({bluetooth, name, address});
}
void BaseViewModel::add_wardrive_map_point(bool bluetooth, const std::string& address, int32_t lat, int32_t lon) {
    if (!wardrive_plotted_.insert((bluetooth ? "bt:" : "wifi:") + address).second) return;
    if (wardrive_map_points_.size() == 1024) wardrive_map_points_.pop_front();
    wardrive_map_points_.push_back({lat, lon, bluetooth});
    gps_tool_generation_.set(gps_tool_generation_.value() + 1);
}
void BaseViewModel::toggle_wardrive_view() {
    if (gps_tool_ != GpsTool::Wardrive) return;
    wardrive_view_ = (wardrive_view_ + 1) % 3;
    gps_tool_generation_.set(gps_tool_generation_.value() + 1);
}
void BaseViewModel::scroll_wardrive_channels(int direction) {
    if (gps_tool_ != GpsTool::Wardrive || wardrive_view_ != 2) return;
    const auto count = wardrive_channels_.distribution().size();
    const auto maximum = count > 5 ? count - 5 : 0;
    if (direction > 0 && wardrive_channel_offset_ < maximum) ++wardrive_channel_offset_;
    else if (direction < 0 && wardrive_channel_offset_) --wardrive_channel_offset_;
    gps_tool_generation_.set(gps_tool_generation_.value() + 1);
}
void BaseViewModel::remote_move(int direction) {
    if (remote_view_ == 2) {
        if (direction < 0) remote_detail_tab_ = (remote_detail_tab_ + 2) % 3;
        else if (direction > 0) remote_detail_tab_ = (remote_detail_tab_ + 1) % 3;
    } else if (remote_view_ == 1) {
        if (direction < 0) remote_operation_cursor_ = 0;
        else if (direction > 0) remote_operation_cursor_ = 1;
    } else if (remote_view_ == 0) {
        if (direction < 0 && remote_cursor_) --remote_cursor_;
        else if (direction > 0 && remote_cursor_ + 1 < remote_store_.size()) ++remote_cursor_;
    }
    publish_remote();
}
void BaseViewModel::remote_open_details() {
    if (remote_cursor_ >= remote_store_.size()) return;
    if (remote_view_ == 0) { remote_view_ = 1; remote_operation_cursor_ = 0; }
    else if (remote_view_ == 1) {
        if (remote_operation_cursor_ == 0) { remote_view_ = 2; remote_detail_tab_ = 0; }
        else begin_remote_fox_hunt();
    }
    publish_remote();
}
void BaseViewModel::remote_back() {
    if (remote_view_ == 3) { fox_hunt_.reset(); fox_target_ = FoxTarget::None; wifi_hop_tick_ = 0; }
    remote_view_ = remote_view_ == 1 ? 0 : 1;
    remote_message_.clear();
    publish_remote();
}
void BaseViewModel::begin_remote_fox_hunt() {
    const auto* aircraft = remote_store_.at(remote_cursor_);
    if (!aircraft) return;
    if (!wifi_scan_running_) { remote_message_ = "Monitor capture unavailable"; return; }
    const auto found = std::find_if(wifi_frequencies_.begin(), wifi_frequencies_.end(), [&](unsigned mhz) {
        return platform::wireless::channel_number(static_cast<int>(mhz)) == aircraft->lastWifiChannel;
    });
    if (found == wifi_frequencies_.end()) { remote_message_ = "Aircraft channel unavailable"; return; }
    if (pcap_pinned_frequency() && pcap_pinned_frequency() != *found) {
        remote_message_ = "Stop target PCAP to change channel"; return;
    }
    fox_hunt_.begin(mac_text(aircraft->mac), *found);
    fox_target_ = FoxTarget::Aircraft;
    fox_label_ = aircraft->hasUasId ? aircraft->uasId : "Remote ID aircraft";
    fox_vendor_ = "Wi-Fi Remote ID";
    remote_view_ = 3;
    remote_message_.clear();
    wifi_hop_tick_ = 0;
    wifi_tune_failed_frequency_ = 0;
    fox_ui_tick_ = lv_tick_get();
    advance_wifi_channel();
}
void BaseViewModel::show_pcap_files_page() {
    stop_wifi_capture();
    refresh_pcap_files();
    pcap_details_ = false;
    model_.set_current_page(model::AppPage::PcapFiles);
    publish_navigation();
}

void BaseViewModel::refresh_pcap_files() {
    pcap_files_ = platform::pcap::list_files();
    if (pcap_cursor_ >= pcap_files_.size()) pcap_cursor_ = pcap_files_.empty() ? 0 : pcap_files_.size() - 1;
    pcap_files_generation_.set(pcap_files_generation_.value() + 1);
}

void BaseViewModel::move_pcap_file(int direction) {
    if (pcap_files_.empty() || pcap_details_) return;
    if (direction < 0 && pcap_cursor_) --pcap_cursor_;
    if (direction > 0 && pcap_cursor_ + 1 < pcap_files_.size()) ++pcap_cursor_;
    pcap_files_generation_.set(pcap_files_generation_.value() + 1);
}

void BaseViewModel::toggle_pcap_details() {
    if (pcap_files_.empty()) return;
    pcap_details_ = !pcap_details_;
    pcap_files_generation_.set(pcap_files_generation_.value() + 1);
}

std::string BaseViewModel::selected_pcap_name() const {
    return pcap_cursor_ < pcap_files_.size() ? pcap_files_[pcap_cursor_].name : "";
}

std::string BaseViewModel::delete_selected_pcap() {
    const auto name = selected_pcap_name();
    if (name.empty()) return "No capture selected";
    const auto path = platform::pcap::directory() / name;
    if (pcap_capture_.active() && pcap_capture_.path() == path.string()) return "Stop recording first";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || std::filesystem::is_symlink(path, ec) ||
        !std::filesystem::remove(path, ec) || ec) return "Could not delete capture";
    pcap_details_ = false;
    pcap_message_ = "Capture deleted";
    refresh_pcap_files();
    return pcap_message_;
}

void BaseViewModel::show_wardrive_files_page() {
    stop_wifi_capture();
    refresh_wardrive_files();
    wardrive_details_ = false;
    model_.set_current_page(model::AppPage::WardriveFiles);
    publish_navigation();
}

void BaseViewModel::refresh_wardrive_files() {
    wardrive_files_ = platform::wardrive::list_files();
    if (wardrive_cursor_ >= wardrive_files_.size())
        wardrive_cursor_ = wardrive_files_.empty() ? 0 : wardrive_files_.size() - 1;
    wardrive_files_generation_.set(wardrive_files_generation_.value() + 1);
}

void BaseViewModel::move_wardrive_file(int direction) {
    if (wardrive_details_ || wardrive_files_.empty()) return;
    if (direction < 0 && wardrive_cursor_) --wardrive_cursor_;
    if (direction > 0 && wardrive_cursor_ + 1 < wardrive_files_.size()) ++wardrive_cursor_;
    wardrive_files_generation_.set(wardrive_files_generation_.value() + 1);
}

void BaseViewModel::toggle_wardrive_details() {
    if (wardrive_files_.empty()) return;
    wardrive_details_ = !wardrive_details_;
    wardrive_files_generation_.set(wardrive_files_generation_.value() + 1);
}

std::string BaseViewModel::selected_wardrive_name() const {
    return wardrive_cursor_ < wardrive_files_.size() ? wardrive_files_[wardrive_cursor_].name : "";
}

std::string BaseViewModel::delete_selected_wardrive() {
    const auto name = selected_wardrive_name();
    if (name.empty()) return "No log selected";
    if (wardrive_upload_job_.valid()) return "Upload in progress";
    if (gps_log_.active() && gps_log_.path().filename() == name) return "Stop wardrive first";
    std::string error;
    if (!platform::wardrive::erase(name, error)) return error;
    wardrive_details_ = false;
    wardrive_message_ = "Wardrive log deleted";
    refresh_wardrive_files();
    return wardrive_message_;
}

std::string BaseViewModel::upload_selected_wardrive(const std::string& service) {
    const auto name = selected_wardrive_name();
    if (name.empty()) return "No log selected";
    if (!wardrive_files_[wardrive_cursor_].eligible) return "Log has no devices";
    if (service == "both") {
        std::vector<std::pair<std::string, std::string>> tasks;
        const auto& file = wardrive_files_[wardrive_cursor_];
        if (file.wigle != "Uploaded" && file.wigle != "Queued") tasks.emplace_back(name, "wigle");
        if (file.wdg != "Uploaded" && file.wdg != "Queued") tasks.emplace_back(name, "wdg");
        return start_wardrive_upload(std::move(tasks));
    }
    return start_wardrive_upload({{name, service}});
}

std::size_t BaseViewModel::pending_wardrive_count() const {
    std::size_t count{};
    for (const auto& file : wardrive_files_) {
        if (!file.eligible) continue;
        count += file.wigle != "Uploaded" && file.wigle != "Queued";
        count += file.wdg != "Uploaded" && file.wdg != "Queued";
    }
    return count;
}

std::string BaseViewModel::upload_pending_wardrive() {
    if (wardrive_upload_job_.valid()) return "Upload in progress";
    refresh_wardrive_files();
    std::vector<std::pair<std::string, std::string>> tasks;
    for (const auto& file : wardrive_files_) {
        if (!file.eligible) continue;
        if (file.wigle != "Uploaded" && file.wigle != "Queued") tasks.emplace_back(file.name, "wigle");
        if (file.wdg != "Uploaded" && file.wdg != "Queued") tasks.emplace_back(file.name, "wdg");
    }
    return start_wardrive_upload(std::move(tasks));
}

unsigned BaseViewModel::wardrive_upload_percent() const {
    const auto total = wardrive_progress_.total.load();
    return total ? static_cast<unsigned>(std::min<std::uint64_t>(100, wardrive_progress_.sent.load() * 100 / total)) : 0;
}

std::string BaseViewModel::wardrive_upload_label() const {
    const auto index = wardrive_upload_index_.load();
    if (index >= wardrive_upload_tasks_.size()) return "Finishing uploads";
    const auto& [name, service] = wardrive_upload_tasks_[index];
    return std::to_string(index + 1) + "/" + std::to_string(wardrive_upload_tasks_.size()) + "  " +
        (service == "wigle" ? "WiGLE" : "WDGWars") + "  " + screen_capture_name(name);
}

std::string BaseViewModel::wardrive_upload_phase_label() const {
    const auto phase = wardrive_progress_.phase.load();
    if (phase == 0) return "Preparing log";
    if (phase == 1) return "Sending file";
    const auto started = wardrive_progress_.phase_started_ms.load();
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto seconds = started > 0 && now > started ? (now - started) / 1000 : 0;
    const auto index = wardrive_upload_index_.load();
    const bool wdg = index < wardrive_upload_tasks_.size() && wardrive_upload_tasks_[index].second == "wdg";
    return std::string(phase == 3 ? "WDG job processing " :
                       wdg ? "WDG server processing " : "WiGLE server response ") +
        std::to_string(seconds) + "s";
}

std::string BaseViewModel::start_wardrive_upload(std::vector<std::pair<std::string, std::string>> tasks) {
    if (wardrive_upload_job_.valid()) return "Upload in progress";
    if (tasks.empty()) return "No pending uploads";
    for (const auto& [name, service] : tasks) {
        if (gps_log_.active() && gps_log_.path().filename() == name) return "Stop wardrive first";
        if (service == "wigle" && (!model::credential_saved(model::Credential::WigleName) ||
                                    !model::credential_saved(model::Credential::WigleToken)))
            return "Set WiGLE credentials first";
        if (service == "wdg" && !model::credential_saved(model::Credential::WdgKey))
            return "Set WDGWars API key first";
    }
    const auto active = active_wifi_profile();
    if (active.first.empty()) return "Connect to WiFi first";
    if (!model_.trusted_profile().empty() && active.first != model_.trusted_profile())
        return "Connect to trusted WiFi";
    wardrive_upload_tasks_ = std::move(tasks);
    wardrive_upload_index_ = 0;
    wardrive_progress_.sent = 0;
    wardrive_progress_.total = 0;
    wardrive_progress_.phase = 0;
    wardrive_progress_.phase_started_ms = 0;
    try {
        wardrive_upload_job_ = std::async(std::launch::async, [this] {
            unsigned uploaded{}, queued{}, failed{};
            std::string last;
            for (std::size_t i = 0; i < wardrive_upload_tasks_.size(); ++i) {
                wardrive_upload_index_ = i;
                wardrive_progress_.sent = 0;
                wardrive_progress_.total = 0;
                wardrive_progress_.phase = 0;
                wardrive_progress_.phase_started_ms = 0;
                const auto& [name, service] = wardrive_upload_tasks_[i];
                const auto result = platform::wardrive::upload(name, service, &wardrive_progress_);
                last = result.message;
                if (!result.ok) ++failed;
                else if (last.find(" accepted ") != std::string::npos) ++queued;
                else ++uploaded;
            }
            wardrive_upload_index_ = wardrive_upload_tasks_.size();
            if (wardrive_upload_tasks_.size() == 1) return last;
            return std::to_string(uploaded) + " uploaded, " + std::to_string(queued) +
                " queued, " + std::to_string(failed) + " failed";
        });
    } catch (const std::system_error&) { return "Could not start upload"; }
    wardrive_message_ = "Uploading " + std::to_string(wardrive_upload_tasks_.size()) + " service item(s)...";
    wardrive_files_generation_.set(wardrive_files_generation_.value() + 1);
    return wardrive_message_;
}

void BaseViewModel::check_credentials(const std::string& service) {
    if (credential_check_job_.valid()) { resource_error_ = "Check in progress"; return; }
    if (service == "wigle" && (!model::credential_saved(model::Credential::WigleName) ||
                                !model::credential_saved(model::Credential::WigleToken))) {
        resource_error_ = "Set WiGLE credentials first"; return;
    }
    if (service == "wdg" && !model::credential_saved(model::Credential::WdgKey)) {
        resource_error_ = "Set WDGWars API key first"; return;
    }
    credential_check_service_ = service;
    auto& status = service == "wigle" ? wigle_check_ : wdg_check_;
    status = "Checking";
    try {
        credential_check_job_ = std::async(std::launch::async, platform::wardrive::check_credentials, service);
    } catch (const std::system_error&) { status = "Unavailable"; resource_error_ = "Could not start check"; }
}

std::string BaseViewModel::toggle_pcap_capture() {
    if (pcap_capture_.active()) {
        const auto path = pcap_capture_.stop();
        pcap_status_.set("P");
        pcap_message_ = "Saved " + std::to_string(pcap_capture_.frames()) + " frames";
        refresh_pcap_files();
        if (model_.current_page() == model::AppPage::WifiScan) publish_wifi_scan();
        return pcap_message_ + " to " + path;
    }
    return start_pcap_capture({});
}

std::string BaseViewModel::start_pcap_capture(platform::pcap::Filter filter) {
    const auto warning = collection_warning(model::RadioRequirement::Monitor);
    if (!warning.empty()) return warning;
    if (monitor_job_.valid()) return "Radio mode is changing";
    const int socket = monitor_session_for(model_.selected_adapter_id()).capture_socket();
    if (socket < 0) return "Monitor capture unavailable";
    const auto error = pcap_capture_.start(socket, model_.reserve_mib(), std::move(filter));
    if (!error.empty()) return error;
    const auto scope = pcap_capture_.filter().scope;
    pcap_status_.set(scope == platform::pcap::Scope::AccessPoint ? "REC AP" :
                     scope == platform::pcap::Scope::Station ? "REC STA" : "REC ALL");
    pcap_message_ = "Recording to " + pcap_capture_.path();
    refresh_pcap_files();
    if (model_.current_page() == model::AppPage::WifiScan) publish_wifi_scan();
    return pcap_message_;
}

std::string BaseViewModel::toggle_selected_pcap_capture() {
    if (!pcap_action_selected()) return "Select a device first";
    const bool pineapple = wifi_view_ == 12;
    if (pineapple && pineapple_cursor_ >= pineapple_items_.size()) return "Select a candidate first";
    if (!pineapple && wifi_scan_detail_index_ >= wifi_networks_.size()) return "Select an AP or station first";
    const auto* ap = pineapple ? nullptr : &wifi_networks_[wifi_scan_detail_index_];
    const bool station = wifi_view_ == 5;
    if (station && wifi_client_index_ >= ap->clients.size()) return "Select a station first";
    const auto scope = station ? platform::pcap::Scope::Station : platform::pcap::Scope::AccessPoint;
    const auto& mac = pineapple ? pineapple_items_[pineapple_cursor_].mac :
        station ? ap->clients[wifi_client_index_].mac : ap->bssid;
    if (pcap_capture_.active()) {
        if (pcap_recording_target(scope, mac)) {
            const auto result = toggle_pcap_capture();
            publish_wifi_scan();
            return result;
        }
        return "Stop the current PCAP first";
    }
    const int mhz = pineapple ? pineapple_items_[pineapple_cursor_].frequency_mhz : ap->frequency_mhz;
    if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), static_cast<unsigned>(mhz)) == wifi_frequencies_.end())
        return "AP channel not available";
    if (wifi_hop_job_.valid()) { wifi_hop_job_.wait(); wifi_hop_job_.get(); }
    const auto tuned = monitor_session_for(model_.selected_adapter_id()).tune(static_cast<unsigned>(mhz));
    if (tuned != "OK") return tuned;
    wifi_tuned_frequency_ = static_cast<unsigned>(mhz);
    wifi_tune_failed_frequency_ = 0;
    wifi_hop_tick_ = lv_tick_get();
    const auto result = start_pcap_capture({scope, mac, mhz});
    if (pcap_capture_.active()) wifi_scan_error_.clear();
    publish_wifi_scan();
    return result;
}
void BaseViewModel::toggle_page() { stop_wifi_capture(); model_.toggle_page(); publish_navigation(); }
const char* BaseViewModel::marauder_menu_title() const { return marauder_menu_.title(); }
std::size_t BaseViewModel::marauder_menu_item_count() const { return marauder_menu_.item_count(); }
const model::MarauderMenuItem& BaseViewModel::marauder_menu_item(std::size_t index) const {
    return marauder_menu_.item(index);
}
std::size_t BaseViewModel::marauder_menu_selected_index() const { return marauder_menu_.selected_index(); }
bool BaseViewModel::marauder_menu_at_root() const { return !marauder_menu_.can_go_back(); }
bool BaseViewModel::marauder_menu_can_go_back() const { return marauder_menu_.can_go_back(); }
void BaseViewModel::move_menu_focus(int dx, int dy) {
    pending_uuid_.clear(); pending_network_.clear(); resource_error_.clear();
    const auto previous = marauder_menu_.selected_index();
    marauder_menu_.move(dx, dy);
    if (previous != marauder_menu_.selected_index())
        marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}
namespace {
bool credential_action(model::MarauderMenuAction action, model::Credential& field) {
    using A = model::MarauderMenuAction;
    switch (action) {
        case A::WigleName: field = model::Credential::WigleName; return true;
        case A::WigleToken: field = model::Credential::WigleToken; return true;
        case A::WdgKey: field = model::Credential::WdgKey; return true;
        default: return false;
    }
}
}

void BaseViewModel::credential_key(uint32_t key) {
    if (!credential_open_) return;
    if (key == LV_KEY_ESC) credential_open_ = false;
    else if (key == LV_KEY_ENTER) {
        if (credential_input_.empty() && !clear_credential_) return;
        if (model::store_credential(credential_field_, credential_input_)) {
            credential_open_ = false;
            if (credential_field_ == model::Credential::WdgKey) wdg_check_ = "Not checked";
            else wigle_check_ = "Not checked";
        }
        else credential_prompt_ = "Save failed. Enter to retry.";
    } else if (key == LV_KEY_DEL && credential_input_.empty()) {
        clear_credential_ = true;
        credential_prompt_ = "Enter to remove saved value";
    } else if (key == LV_KEY_BACKSPACE) {
        if (!credential_input_.empty()) credential_input_.pop_back();
    } else if (key >= 33 && key <= 126 && credential_input_.size() < 512) {
        credential_input_.push_back(static_cast<char>(key));
        clear_credential_ = false;
        credential_prompt_ = "Enter to save";
    } else return;
    if (!credential_open_) {
        std::fill(credential_input_.begin(), credential_input_.end(), '\0');
        credential_input_.clear();
    }
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

std::string BaseViewModel::collection_warning(model::RadioRequirement requirement) const {
    using Requirement = model::RadioRequirement;
    if (requirement == Requirement::None) return {};
    if (model_.selected_adapter_id().empty()) return "Select a WiFi adapter first";
    const auto selected = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (selected == adapters_.end()) return "Selected adapter unavailable";
    const std::string path = "/sys/class/net/" + selected->interface_name + "/";
    std::string index;
    if (!(std::ifstream(path + "ifindex") >> index) || index != selected->ifindex)
        return "Selected adapter unavailable";
    if (requirement == Requirement::Selected) return {};
    if (monitor_job_.valid()) return "Radio mode is changing";
    int type{};
    if (!(std::ifstream(path + "type") >> type)) return "Cannot verify adapter mode";
    const bool monitor = type == 801 || type == 802 || type == 803;
    if (requirement == Requirement::Managed && monitor) return "Turn monitor mode off for AP scans";
    if (requirement == Requirement::Monitor && !monitor)
        return selected->monitor_mode == platform::wireless::CapabilityState::Unsupported ?
            "Select a monitor-capable adapter" : "Enable monitor mode first";
    return {};
}

void BaseViewModel::marauder_menu_activate() {
    using Action = model::MarauderMenuAction;
    const auto action = marauder_menu_.activate();
    resource_error_.clear();
    if (action == Action::Brightness) {
        if (light_max_ <= 0 || original_light_ < 0) resource_error_ = "Backlight unavailable";
        else if (!model_.cycle_brightness()) resource_error_ = "Could not save preference";
        else if (!set_light(model_.brightness() ? std::max(1, light_max_ * static_cast<int>(model_.brightness()) / 100) : original_light_))
            resource_error_ = "Backlight write failed";
    }
    if (action == Action::DimDelay && !model_.cycle_dim()) resource_error_ = "Could not save preference";
    if (action == Action::Reserve && !model_.cycle_reserve()) resource_error_ = "Could not save preference";
    if (action == Action::UploadPolicy) choose_upload_policy();
    if (action == Action::CheckWigle) check_credentials("wigle");
    if (action == Action::CheckWdg) check_credentials("wdg");
    if (credential_action(action, credential_field_)) {
        credential_open_ = true;
        clear_credential_ = false;
        credential_prompt_ = model::credential_saved(credential_field_) ?
            "Type replacement or Del to remove" : "Type a new value";
    }
    if (action == Action::CycleStorage || action == Action::CycleCache) {
        const bool storage = action == Action::CycleStorage;
        const bool saved = storage ? model_.cycle_storage() : model_.cycle_cache();
        if (saved && !storage) lv_image_cache_resize(model_.cache_kib() * 1024U, true);
        resource_message_ = !saved ? "Could not save preference" : !storage ? "Image cache limit applied" :
            model_.screenshots_in_documents() ? "~/Documents/Marauder" : "~/Pictures/Screenshots";
    }
    if (action == model::MarauderMenuAction::CycleBatteryDisplay) {
        model_.cycle_battery_display();
        battery_display_subject_.set(static_cast<int>(model_.battery_display()));
    }
    if (action == Action::CycleCensor) {
        if (!model_.toggle_censor()) resource_error_ = "Could not save censor setting";
        else {
            publish_adapter_detail();
            inventory_generation_.set(inventory_generation_.value() + 1);
            pcap_files_generation_.set(pcap_files_generation_.value() + 1);
        }
    }
    if (action == Action::CycleTrafficSource && !model_.cycle_traffic_source())
        resource_error_ = "Could not save graph source";
    if (action == Action::CycleWardriveDwell && !model_.cycle_wardrive_dwell())
        resource_error_ = "Could not save channel dwell";
    if (action == Action::CycleWardrivePriority && !model_.cycle_wardrive_priority())
        resource_error_ = "Could not save channel priority";
    if (action == Action::CycleWardriveAdapters && !model_.cycle_wardrive_adapters())
        resource_error_ = "Could not save adapter policy";
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
    switch (action) {
        case model::MarauderMenuAction::OpenWifiScan:
            show_wifi_scan_page();
            break;
        case model::MarauderMenuAction::OpenRawCapture:
            show_raw_capture_page();
            break;
        case Action::OpenBluetoothRaw: show_bluetooth_page(0); break;
        case Action::OpenBluetoothFindMy: show_bluetooth_page(platform::ble::bit(platform::ble::Kind::FindMy)); break;
        case Action::OpenBluetoothIBeacon: show_bluetooth_page(platform::ble::bit(platform::ble::Kind::IBeacon)); break;
        case Action::OpenBluetoothFlipper: show_bluetooth_page(platform::ble::bit(platform::ble::Kind::Flipper)); break;
        case Action::OpenBluetoothFlock: show_bluetooth_page(platform::ble::bit(platform::ble::Kind::Flock)); break;
        case Action::OpenBluetoothMeta: show_bluetooth_page(platform::ble::bit(platform::ble::Kind::Meta)); break;
        case Action::OpenBluetoothSerial: show_bluetooth_page(platform::ble::bit(platform::ble::Kind::SerialModule)); break;
        case model::MarauderMenuAction::OpenRemoteId:
            show_remote_id_page();
            break;
        case Action::OpenGpsInfo: show_gps_tool(GpsTool::Info); break;
        case Action::OpenWardrive: show_gps_tool(GpsTool::Wardrive); break;
        case Action::OpenGpsTrack: show_gps_tool(GpsTool::Track); break;
        case model::MarauderMenuAction::OpenPineapple:
            show_pineapple_page();
            break;
        case model::MarauderMenuAction::OpenAdapters:
            show_adapters_page();
            break;
        case model::MarauderMenuAction::OpenDiagnostics:
            show_diagnostics_page();
            break;
        case Action::OpenTraffic:
            model_.set_current_page(model::AppPage::Traffic);
            publish_navigation();
            break;
        case Action::OpenPcapFiles:
            show_pcap_files_page();
            break;
        case Action::OpenWardriveFiles:
            show_wardrive_files_page();
            break;
        case Action::OpenMapPacks:
            show_map_packs_page();
            break;
        case Action::OpenRadioSetup:
            model_.set_current_page(model::AppPage::RadioSetup);
            publish_navigation();
            refresh_radios();
            break;
        case Action::WigleName:
        case Action::WigleToken:
        case Action::WdgKey:
        case Action::CheckWigle:
        case Action::CheckWdg:
        case Action::CycleStorage:
        case Action::Brightness:
        case Action::DimDelay:
        case Action::Reserve:
        case Action::UploadPolicy:
        case Action::CycleCache:
        case Action::CycleTrafficSource:
        case Action::CycleWardriveDwell:
        case Action::CycleWardrivePriority:
        case Action::CycleWardriveAdapters:
        case model::MarauderMenuAction::CycleBatteryDisplay:
        case model::MarauderMenuAction::None:
            break;
    }
}
const char* BaseViewModel::settings_message() const {
    if (!resource_error_.empty()) return resource_error_.c_str();
    if (!pending_uuid_.empty()) return "Enter to trust WiFi";
    if (marauder_menu_.current_menu() == model::MarauderMenuId::FilesAndMemory) return resource_message_.c_str();
    return "";
}

std::string BaseViewModel::setting_value(model::MarauderMenuAction action) const {
    using Action = model::MarauderMenuAction;
    if (action == Action::Brightness) return model_.brightness() ? std::to_string(model_.brightness()) + "%" : "System";
    if (action == Action::DimDelay) return model_.dim_seconds() ? std::to_string(model_.dim_seconds()) + " sec" : "Never";
    if (action == Action::Reserve) return model_.reserve_mib() ? std::to_string(model_.reserve_mib()) + " MiB" : "Off";
    if (action == Action::CycleTrafficSource) return model_.traffic_monitor_source() ? "Monitor" : "Adapter";
    if (action == Action::CycleWardriveDwell) return std::to_string(model_.wardrive_dwell_ms()) + " ms";
    if (action == Action::CycleWardriveAdapters)
        return model_.wardrive_all_idle_adapters() ? "All idle" : "Selected";
    if (action == Action::CycleWardrivePriority) {
        constexpr const char* profiles[] = {"Even", "1/6/11", "1/5/9/13", "5G low", "5G high"};
        return profiles[model_.wardrive_priority()];
    }
    if (action == Action::UploadPolicy) return !pending_uuid_.empty() ? screen_name(pending_network_) :
        model_.trusted_profile().empty() ? "Manual" : "Trusted WiFi";
    if (action == Action::CheckWigle) return wigle_check_;
    if (action == Action::CheckWdg) return wdg_check_;
    if (action == Action::CycleCensor) return model_.censor_enabled() ? "On" : "Off";
    model::Credential field;
    if (credential_action(action, field)) return model::credential_saved(field) ? "Saved" : "Not set";
    if (action == model::MarauderMenuAction::CycleBatteryDisplay) return model::battery_display_name(model_.battery_display());
    if (action == model::MarauderMenuAction::CycleStorage)
        return model_.screenshots_in_documents() ? "Documents" : "Pictures";
    if (action == model::MarauderMenuAction::CycleCache)
        return model_.cache_kib() ? std::to_string(model_.cache_kib()) + " KiB" : "Off";
    return {};
}

void BaseViewModel::load_radios(std::string key, bool enabled) {
    if (radio_busy()) return;
    radio_list_.message = key.empty() ? "Refreshing" : "Applying";
    try {
        radio_job_ = std::async(std::launch::async, platform::read_radios, std::move(key), enabled, radio_session_);
    } catch (const std::system_error&) { radio_list_.message = "Worker unavailable"; }
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

void BaseViewModel::refresh_radios() { load_radios({}, false); }

void BaseViewModel::move_radio(int direction) {
    if (radios().empty()) return;
    if (direction < 0 && radio_cursor_ > 0) --radio_cursor_;
    if (direction > 0 && radio_cursor_ + 1 < radios().size()) ++radio_cursor_;
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

void BaseViewModel::toggle_radio() {
    if (radios().empty()) return;
    const auto& radio = radios()[radio_cursor_];
    if (radio_busy()) return;
    if (radio.kind == "GPS") {
        const auto next = gps_id_ == radio.id ? std::string{} : radio.id;
        if (!model::save_gps_id(next)) {
            radio_list_.message = "Could not save GPS";
        } else if (next.empty()) {
            gps_session_.stop();
            gps_id_.clear();
            gps_status_.set("Off");
            radio_list_.message = "GPS released";
        } else if (gps_session_.start(radio.id)) {
            gps_id_ = radio.id;
            gps_status_.set("Wait");
            radio_list_.message = "GPS selected";
        } else {
            gps_id_ = next;
            gps_status_.set("Unavailable");
            radio_list_.message = "GPS reader unavailable";
        }
        sync_gps_rows();
        marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
        return;
    }
    if (radio.state == "On" || radio.state == "Off") load_radios(radio.id, radio.state == "Off");
}

void BaseViewModel::update_gps_status() {
    const auto state = gps_session_.poll();
    if (!state.empty()) {
        gps_status_.set(state.c_str());
        if (model_.current_page() == model::AppPage::RemoteId) publish_remote();
    }
}

void BaseViewModel::sync_gps_rows() {
    auto& rows = radio_list_.devices;
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](const auto& d) {
        return d.kind == "GPS" && d.name == "Saved GPS";
    }), rows.end());
    bool found = false;
    for (auto& d : rows) {
        if (d.kind != "GPS") continue;
        d.state = d.id == gps_id_ ? "On" : "Off";
        found |= d.id == gps_id_;
    }
    if (!gps_id_.empty() && !found)
        rows.push_back({gps_id_, "GPS", "Saved GPS", "Disconnected", "Waiting for receiver",
                        "Enter to disable", "", "On", ""});
    if (radio_cursor_ >= rows.size()) radio_cursor_ = rows.empty() ? 0 : rows.size() - 1;
}

void BaseViewModel::finish_radio_setting() {
    if (!radio_busy() || radio_job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const auto selected = radios().empty() ? std::string{} : radios()[radio_cursor_].id;
    try { radio_list_ = radio_job_.get(); }
    catch (const std::exception&) { radio_list_ = {"Radio request failed", {}}; }
    sync_gps_rows();
    radio_cursor_ = 0;
    for (std::size_t i = 0; i < radios().size(); ++i) {
        if (radios()[i].id == selected) { radio_cursor_ = i; break; }
    }
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

bool BaseViewModel::marauder_menu_back() {
    pending_uuid_.clear(); pending_network_.clear(); resource_error_.clear();
    if (!marauder_menu_.back()) return false;
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
    return true;
}

void BaseViewModel::refresh_diagnostics() {
    diagnostics_snapshot_ = platform::diagnostics::collect();
    system_info_.refresh_counts(diagnostics_snapshot_.network_interfaces.size(),
                                diagnostics_snapshot_.usb_devices.size(),
                                diagnostics_snapshot_.peripheral_nodes.size(),
                                diagnostics_snapshot_.tools.size());
    const auto summary = platform::diagnostics::dashboard_summary(diagnostics_snapshot_);
    dashboard_summary_subject_.set(summary.c_str());
    publish_diagnostics();
    refresh_adapters();
}

void BaseViewModel::previous_system_info_page() { system_info_.previous_page(); publish_diagnostics(); }
void BaseViewModel::next_system_info_page() { system_info_.next_page(); publish_diagnostics(); }
void BaseViewModel::toggle_system_info_details() { system_info_.toggle_details(); publish_diagnostics(); }

void BaseViewModel::publish_diagnostics() {
    const auto text = format::system_info_page(diagnostics_snapshot_, system_info_);
    diagnostics_detail_subject_.set(text.c_str());
    diagnostics_generation_subject_.set(diagnostics_generation_subject_.value() + 1);
}

void BaseViewModel::toggle_monitor() {
    if (monitor_busy() || highlighted_adapter_ >= adapters_.size()) return;
    const auto& adapter = adapters_[highlighted_adapter_];
    if (adapter.stable_id != model_.selected_adapter_id()) adapter_error_ = "Select adapter first";
    else if (wifi_scan_running_) adapter_error_ = "Leave Access Points first";
    else if (pcap_capture_.active() && !monitor_owner_.empty()) adapter_error_ = "Stop PCAP capture first";
    else if (monitor_owner_.empty() && adapter.monitor_mode != platform::wireless::CapabilityState::Supported)
        adapter_error_ = "Monitor support unavailable";
    else {
        const bool start = !active_monitor_ids_.count(adapter.stable_id);
        const auto name = adapter.interface_name;
        const auto id = adapter.stable_id;
        auto* session = &monitor_session_for(id);
        monitor_owner_ = adapter.stable_id;
        try {
            monitor_job_ = std::async(std::launch::async, [session, name, start] {
                return start ? session->start(name) : session->stop();
            });
            monitor_pending_id_ = id;
            monitor_pending_name_ = name;
            monitor_target_on_ = start;
            monitor_started_tick_ = lv_tick_get();
            monitor_status_tick_ = 0;
            adapter_error_ = "Switching radio mode...";
        } catch (const std::system_error&) {
            if (start) monitor_owner_.clear();
            monitor_pending_name_.clear();
            adapter_error_ = "Cannot switch mode now";
        }
    }
    publish_adapter_detail();
}

void BaseViewModel::refresh_adapters() {
    // A capability probe runs `iw phy info`; keep it off the UI timer while a
    // mode switch is underway. The adapter list is already loaded at this point.
    if (monitor_busy() && !adapters_.empty()) {
        refresh_pending_monitor_status();
        return;
    }
    auto catalog = platform::wireless::enumerate();
    adapters_ = std::move(catalog.adapters);
    adapter_error_ = std::move(catalog.error);
    if (adapters_.empty()) {
        highlighted_adapter_ = 0;
    } else if (highlighted_adapter_ >= adapters_.size()) {
        highlighted_adapter_ = adapters_.size() - 1;
    }
    const auto saved = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (saved != adapters_.end()) highlighted_adapter_ = saved - adapters_.begin();
    adapter_details_expanded_ = false;
    publish_adapter_detail();
}

void BaseViewModel::refresh_pending_monitor_status() {
    if (monitor_pending_name_.empty()) return;
    const auto found = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.interface_name == monitor_pending_name_;
    });
    if (found == adapters_.end()) return;
    const auto previous = found->monitor_status;
    platform::wireless::refresh_link_state(*found);
    const bool observed_target = found->monitor_status == (monitor_target_on_ ? "On" : "Off");
    const auto seconds = lv_tick_elaps(monitor_started_tick_) / 1000;
    const std::string progress = observed_target ?
        (monitor_target_on_ ? "Monitor on - confirming" : "Monitor off - confirming") :
        "Switching radio mode " + std::to_string(seconds) + "s";
    if (previous != found->monitor_status || adapter_error_ != progress) {
        adapter_error_ = progress;
        publish_adapter_detail();
    }
}

void BaseViewModel::previous_adapter() {
    if (adapters_.empty()) return;
    highlighted_adapter_ = highlighted_adapter_ == 0 ? adapters_.size() - 1 : highlighted_adapter_ - 1;
    adapter_details_expanded_ = false;
    publish_adapter_detail();
}

void BaseViewModel::next_adapter() {
    if (adapters_.empty()) return;
    highlighted_adapter_ = (highlighted_adapter_ + 1) % adapters_.size();
    adapter_details_expanded_ = false;
    publish_adapter_detail();
}

void BaseViewModel::select_highlighted_adapter() {
    if (adapters_.empty()) return;
    const auto& adapter = adapters_[highlighted_adapter_];
    const bool changed = model_.selected_adapter_id() != adapter.stable_id;
    if (!model_.set_selected_adapter_id(adapter.stable_id)) {
        adapter_error_ = "Could not save WiFi selection";
        publish_adapter_detail();
        return;
    }
    adapter_error_.clear();
    monitor_owner_ = active_monitor_ids_.count(adapter.stable_id) ? adapter.stable_id : "";
    if (changed) {
        wifi_networks_.clear();
        station_refs_.clear();
        wifi_scan_interface_.clear();
        wifi_scan_error_.clear();
        wifi_scan_detail_index_ = 0;
        wifi_scan_details_expanded_ = false;
        wifi_view_ = 0;
        wifi_info_tab_ = 0;
    }
    publish_adapter_detail();
    publish_wifi_scan();
}

void BaseViewModel::toggle_adapter_details() {
    adapter_details_expanded_ = !adapter_details_expanded_;
    publish_adapter_detail();
}

void BaseViewModel::start_wardrive_capture(bool clear_results) {
    stop_wifi_capture();
    if (clear_results) {
        wifi_networks_.clear();
        station_refs_.clear();
    }
    wifi_scan_error_.clear();
    wifi_list_builder_ = true;
    wifi_capture_stop_ = false;
    wifi_capture_failed_ = false;
    wifi_scan_running_ = true;
    const auto adapters = adapters_;
    const auto selected = model_.selected_adapter_id();
    std::unordered_map<std::string, platform::MonitorSession*> active_sessions;
    for (const auto& id : model_.monitor_preferences())
        active_sessions.emplace(id, &monitor_session_for(id));
    for (const auto& id : active_monitor_ids_)
        active_sessions.emplace(id, &monitor_session_for(id));
    const auto all_idle = model_.wardrive_all_idle_adapters();
    const auto dwell = model_.wardrive_dwell_ms();
    const auto priority = model_.wardrive_priority();
    try {
        wifi_scan_thread_ = std::thread([this, adapters, selected, active_sessions, all_idle, dwell, priority] {
            run_wardrive_capture(adapters, selected, active_sessions, all_idle, dwell, priority);
        });
    } catch (const std::system_error&) {
        wifi_scan_running_ = false;
        wifi_scan_error_ = "Unable to start wardrive capture";
    }
    publish_wifi_scan();
}

void BaseViewModel::run_wardrive_capture(std::vector<platform::wireless::AdapterInfo> adapters,
                                          std::string selected_id,
                                          std::unordered_map<std::string, platform::MonitorSession*> active_sessions,
                                          bool all_idle, unsigned dwell_ms, unsigned priority) {
#if defined(__linux__) && !USE_DESKTOP
    using Clock = std::chrono::steady_clock;
    struct Radio {
        std::string id;
        std::unique_ptr<platform::MonitorSession> lease;
        platform::MonitorSession* session{};
        int capture{-1};
        std::vector<unsigned> supported;
        std::vector<unsigned> channels;
        std::size_t next_channel{};
        Clock::time_point next_tune{};
    };
    std::stable_sort(adapters.begin(), adapters.end(), [&](const auto& a, const auto& b) {
        return a.stable_id == selected_id && b.stable_id != selected_id;
    });
    std::vector<Radio> radios;
    std::set<std::string> phys;
    for (const auto& adapter : adapters) {
        if (wifi_capture_stop_) break;
        if (!all_idle && adapter.stable_id != selected_id) continue;
        if (adapter.monitor_mode != platform::wireless::CapabilityState::Supported ||
            adapter.frequencies.empty()) continue;
        const auto use = platform::wireless::connection_use(adapter.interface_name);
        if (use == platform::wireless::ConnectionUse::Connected ||
            use == platform::wireless::ConnectionUse::Hotspot) continue;
        const auto phy = adapter.phy_name.empty() ? adapter.interface_name : adapter.phy_name;
        if (phys.count(phy)) continue;
        Radio radio;
        radio.id = adapter.stable_id;
        if (const auto active = active_sessions.find(adapter.stable_id); active != active_sessions.end())
            radio.session = active->second;
        else {
            radio.lease = std::make_unique<platform::MonitorSession>();
            if (radio.lease->start(adapter.interface_name) != "Monitor on") continue;
            radio.session = radio.lease.get();
        }
        radio.capture = radio.session->capture_socket();
        if (radio.capture < 0 && !radio.lease) {
            radio.lease = std::make_unique<platform::MonitorSession>();
            if (radio.lease->start(adapter.interface_name) == "Monitor on") {
                radio.session = radio.lease.get();
                radio.capture = radio.session->capture_socket();
            }
        }
        if (radio.capture < 0) continue;
        phys.insert(phy);
        radio.supported = adapter.frequencies;
        radios.push_back(std::move(radio));
    }
    if (radios.empty()) {
        if (!wifi_capture_stop_) wifi_capture_failed_ = true;
        return;
    }
    const auto rebuild_plan = [&] {
        std::vector<platform::wireless::WardriveRadioPlan> requested;
        for (const auto& radio : radios)
            if (radio.capture >= 0) requested.push_back({radio.id, radio.supported, {}});
        const auto plans = platform::wireless::wardrive_channel_blocks(std::move(requested), priority);
        std::size_t index = 0;
        for (auto& radio : radios) {
            radio.channels = radio.capture >= 0 ? plans[index++].channels : std::vector<unsigned>{};
            radio.next_channel = 0;
            radio.next_tune = {};
        }
        wardrive_active_radios_ = static_cast<unsigned>(plans.size());
    };
    rebuild_plan();
    uint8_t bytes[65535];
    while (!wifi_capture_stop_) {
        const auto now = Clock::now();
        std::vector<pollfd> ready;
        ready.reserve(radios.size());
        for (auto& radio : radios) {
            if (radio.capture < 0) continue;
            if (!radio.channels.empty() && now >= radio.next_tune) {
                const auto mhz = radio.channels[radio.next_channel];
                if (radio.session->tune(mhz) == "OK" &&
                    (radio.id == selected_id || radios.size() == 1))
                    wardrive_tuned_frequency_ = mhz;
                radio.next_channel = (radio.next_channel + 1) % radio.channels.size();
                radio.next_tune = Clock::now() + std::chrono::milliseconds(dwell_ms);
            }
            ready.push_back({radio.capture, POLLIN, 0});
        }
        if (ready.empty()) break;
        const int status = poll(ready.data(), ready.size(), 100);
        if (status < 0) { if (errno == EINTR) continue; break; }
        if (status == 0) continue;
        bool lost_radio = false;
        for (auto& item : ready) {
            if (item.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                close(item.fd);
                for (auto& radio : radios) if (radio.capture == item.fd) radio.capture = -1;
                lost_radio = true;
                continue;
            }
            if (!(item.revents & POLLIN)) continue;
            const auto count = read(item.fd, bytes, sizeof(bytes));
            if (count <= 0) {
                close(item.fd);
                for (auto& radio : radios) if (radio.capture == item.fd) radio.capture = -1;
                lost_radio = true;
                continue;
            }
            platform::wireless::FrameObservation frame;
            if (!platform::wireless::parse_monitor_frame(bytes, static_cast<std::size_t>(count), frame)) continue;
            std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
            if (incoming_frames_.size() < 1024) incoming_frames_.push_back(std::move(frame));
        }
        if (lost_radio) rebuild_plan();
    }
    for (auto& radio : radios) if (radio.capture >= 0) close(radio.capture);
    wardrive_active_radios_ = 0;
    if (!wifi_capture_stop_) wifi_capture_failed_ = true;
#else
    (void)adapters; (void)selected_id; (void)active_sessions;
    (void)all_idle; (void)dwell_ms; (void)priority;
    if (!wifi_capture_stop_) wifi_capture_failed_ = true;
#endif
}

void BaseViewModel::start_wifi_scan(bool clear_results) {
    if (wardrive_session_) { start_wardrive_capture(clear_results); return; }
    stop_wifi_capture();
    if (remote_session_) { remote_view_ = 0; remote_detail_tab_ = 0; }
    const auto selected = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (selected == adapters_.end()) {
        wifi_scan_error_ = "Select an adapter on the Adapters page";
        publish_wifi_scan();
        return;
    }
    const bool same_interface = wifi_scan_interface_ == selected->interface_name;
    wifi_scan_interface_ = selected->interface_name;
    wifi_frequencies_ = selected->frequencies;
    std::stable_sort(wifi_frequencies_.begin(), wifi_frequencies_.end(), [](unsigned a, unsigned b) {
        return (a < 3000) != (b < 3000) ? a < 3000 : a < b;
    });
    wifi_frequencies_.erase(std::unique(wifi_frequencies_.begin(), wifi_frequencies_.end()), wifi_frequencies_.end());
    wifi_scan_error_ = collection_warning(model::RadioRequirement::Monitor);
    if (!wifi_scan_error_.empty()) { publish_wifi_scan(); return; }
    const int capture = monitor_session_for(model_.selected_adapter_id()).capture_socket();
    if (capture < 0) {
        wifi_scan_error_ = "Monitor capture unavailable";
        publish_wifi_scan();
        return;
    }
    // A new capture session must not discard APs and clients built on this adapter.
    // D/Restart explicitly starts a fresh inventory; changing adapters does too.
    if (clear_results || !same_interface) {
        wifi_networks_.clear();
        station_refs_.clear();
        pineapple_items_.clear();
    }
    wifi_scan_detail_index_ = 0;
    wifi_client_index_ = 0;
    station_cursor_ = 0;
    pineapple_cursor_ = 0;
    pineapple_action_cursor_ = 0;
    station_from_global_ = false;
    wifi_view_ = raw_session_ ? 7 : pineapple_session_ ? 11 : 0;
    wifi_scan_details_expanded_ = raw_session_ || pineapple_session_;
    wifi_list_builder_ = !raw_session_ && !remote_session_ && !pineapple_session_;
    wifi_info_tab_ = 0;
    wifi_feed_frames_.clear();
    raw_feed_frames_.clear();
    wifi_feed_target_.clear();
    wifi_feed_count_ = 0;
    raw_feed_offset_ = 0;
    raw_frames_seen_ = 0;
    raw_displayed_count_ = 0;
    raw_filter_mask_ = 0;
    raw_filter_cursor_ = 0;
    wifi_capture_stop_ = false;
    wifi_capture_failed_ = false;
    wifi_hop_index_ = 0;
    raw_channel_index_ = 0;
    wifi_tuned_frequency_ = 0;
    wifi_tune_failed_frequency_ = 0;
    wifi_hop_tick_ = 0;
    wifi_scan_running_ = true;
    try {
        wifi_scan_thread_ = std::thread([this, capture, raw_mode = raw_session_,
                                         remote_mode = remote_session_, pineapple_mode = pineapple_session_] {
#if defined(__linux__) && !USE_DESKTOP
            uint8_t bytes[65535];
            while (!wifi_capture_stop_) {
                pollfd ready{capture, POLLIN, 0};
                const int status = poll(&ready, 1, 150);
                if (status < 0) { if (errno == EINTR) continue; break; }
                if (status == 0) continue;
                if (ready.revents & (POLLERR | POLLHUP | POLLNVAL)) break;
                if (!(ready.revents & POLLIN)) continue;
                const auto count = read(capture, bytes, sizeof(bytes));
                if (count <= 0) break;
                if (remote_mode) {
                    platform::remote_id::Packet packet;
                    if (platform::remote_id::decode_monitor_frame(bytes, static_cast<size_t>(count), packet)) {
                        std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
                        if (incoming_remote_.size() < 128) incoming_remote_.push_back(std::move(packet));
                    }
                    continue;
                }
                if (raw_mode) ++raw_frames_seen_;
                platform::wireless::FrameObservation observation;
                platform::wireless::FrameSummary summary;
                const bool has_summary = platform::wireless::parse_frame_summary(bytes, static_cast<size_t>(count), summary);
                platform::remote_id::Packet remote_packet;
                const bool has_remote = raw_mode && has_summary &&
                    summary.kind == platform::wireless::FrameKind::RemoteId &&
                    platform::remote_id::decode_monitor_frame(bytes, static_cast<size_t>(count), remote_packet);
                platform::wireless::PineappleObservation pineapple;
                const bool has_pineapple = (raw_mode || pineapple_mode) && has_summary &&
                    summary.kind == platform::wireless::FrameKind::Pineapple &&
                    platform::wireless::detect_pineapple_beacon(bytes, static_cast<size_t>(count), pineapple);
                const bool has_observation = wifi_list_builder_ &&
                    platform::wireless::parse_monitor_frame(bytes, static_cast<size_t>(count), observation);
                if (raw_mode && !has_summary) summary.type = "Unparsed";
                if (!raw_mode && !has_summary && !has_observation && !has_pineapple) continue;
                std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
                if (has_observation && wifi_list_builder_ && incoming_frames_.size() < 1024)
                    incoming_frames_.push_back(std::move(observation));
                if ((has_summary || raw_mode) && incoming_summaries_.size() < 1024) incoming_summaries_.push_back(std::move(summary));
                if (has_remote && incoming_remote_.size() < 128) incoming_remote_.push_back(std::move(remote_packet));
                if (has_pineapple && incoming_pineapple_.size() < 256)
                    incoming_pineapple_.push_back(std::move(pineapple));
            }
            if (!wifi_capture_stop_) wifi_capture_failed_ = true;
            close(capture);
#endif
        });
        wifi_scan_error_.clear();
    } catch (const std::system_error&) {
#if defined(__linux__) && !USE_DESKTOP
        close(capture);
#endif
        wifi_scan_running_ = false;
        wifi_scan_error_ = "Unable to start frame capture";
    }
    publish_wifi_scan();
}

void BaseViewModel::stop_wifi_capture() {
    wifi_capture_stop_ = true;
    if (wifi_scan_thread_.joinable()) wifi_scan_thread_.join();
    if (wifi_hop_job_.valid()) {
        wifi_hop_job_.wait();
        wifi_hop_job_ = {}; // A later scan must not consume this session's tune result.
    }
    wifi_scan_running_ = false;
    wardrive_active_radios_ = 0;
    wardrive_tuned_frequency_ = 0;
    std::vector<platform::remote_id::Packet> remote;
    std::vector<platform::wireless::PineappleObservation> pineapple;
    {
        std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
        incoming_frames_.clear();
        incoming_summaries_.clear();
        remote.swap(incoming_remote_);
        pineapple.swap(incoming_pineapple_);
    }
    if (!remote.empty()) finish_remote_observations(remote);
    if (!pineapple.empty()) finish_pineapple_observations(pineapple);
    fox_hunt_.reset();
    fox_target_ = FoxTarget::None;
}

void BaseViewModel::advance_wifi_channel() {
    if (wardrive_session_) return;
    if (!wifi_scan_running_ || wifi_frequencies_.empty()) return;
    if (wifi_hop_job_.valid()) {
        if (wifi_hop_job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        const auto reply = wifi_hop_job_.get();
        if (reply == "OK") {
            wifi_tuned_frequency_ = wifi_frequencies_[wifi_hop_index_];
            wifi_tune_failed_frequency_ = 0;
            wifi_scan_error_.clear();
        }
        else { wifi_tune_failed_frequency_ = wifi_frequencies_[wifi_hop_index_]; wifi_scan_error_ = "Channel tune failed"; }
        publish_wifi_scan();
    }
    const auto& recording = pcap_capture_.filter();
    const bool pinned = pcap_capture_.active() && recording.scope != platform::pcap::Scope::All;
    const bool raw_browsing = raw_session_ && (wifi_view_ == 0 || wifi_view_ == 7 ||
                                                 wifi_view_ == 8 || wifi_view_ == 9);
    const bool hopping = wardrive_session_ || (remote_session_ && remote_view_ != 3) ||
                         (!raw_session_ && !pineapple_session_ && wifi_view_ == 0) ||
                         (pineapple_session_ && wifi_view_ == 11) ||
                         (raw_browsing && raw_auto_hop_);
    if (!pinned && wifi_hop_tick_ && lv_tick_elaps(wifi_hop_tick_) < 650 && hopping) return;
    unsigned requested{};
    if (pinned) {
        requested = static_cast<unsigned>(recording.frequency_mhz);
    } else if (hopping) {
        wifi_hop_index_ = wifi_hop_tick_ ? (wifi_hop_index_ + 1) % wifi_frequencies_.size() : 0;
        requested = wifi_frequencies_[wifi_hop_index_];
    } else if (remote_session_ && remote_view_ == 3) {
        requested = fox_hunt_.frequency_mhz();
    } else if (pineapple_session_ && pineapple_cursor_ < pineapple_items_.size()) {
        requested = static_cast<unsigned>(pineapple_items_[pineapple_cursor_].frequency_mhz);
    } else if (raw_browsing) {
        requested = wifi_frequencies_[raw_channel_index_];
    } else if (wifi_scan_detail_index_ < wifi_networks_.size()) {
        requested = wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
    }
    if (!requested) return;
    const auto found = std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), requested);
    if (found == wifi_frequencies_.end()) return;
    wifi_hop_index_ = static_cast<size_t>(found - wifi_frequencies_.begin());
    if (requested == wifi_tuned_frequency_ || requested == wifi_tune_failed_frequency_) return;
    wifi_hop_tick_ = lv_tick_get();
    auto* session = &monitor_session_for(model_.selected_adapter_id());
    wifi_hop_job_ = std::async(std::launch::async, [session, requested] { return session->tune(requested); });
}

void BaseViewModel::previous_wifi_scan_page() {
    if (wifi_view_ == 11) { if (pineapple_cursor_) --pineapple_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 12) { if (pineapple_action_cursor_) --pineapple_action_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 15) { if (wifi_feed_offset_ + 1 < wifi_feed_frames_.size()) ++wifi_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 13 || wifi_view_ == 14) return;
    if (wifi_view_ == 10) return;
    if (wifi_view_ == 9) { if (raw_filter_cursor_) --raw_filter_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 8) { if (station_cursor_) --station_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 7) { if (raw_feed_offset_ + 1 < raw_feed_frames_.size()) ++raw_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 6) { if (wifi_feed_offset_ + 1 < wifi_feed_frames_.size()) ++wifi_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 4 || wifi_view_ == 5) { if (wifi_operation_index_) --wifi_operation_index_; publish_wifi_scan(); return; }
    auto& index = wifi_view_ == 2 || wifi_view_ == 3 ? wifi_client_index_ : wifi_scan_detail_index_;
    if (index > 0) { --index; if (wifi_view_ <= 1) wifi_info_tab_ = 0; }
    publish_wifi_scan();
}

void BaseViewModel::next_wifi_scan_page() {
    if (wifi_view_ == 11) { if (pineapple_cursor_ + 1 < pineapple_items_.size()) ++pineapple_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 12) { if (pineapple_action_cursor_ < 3) ++pineapple_action_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 15) { if (wifi_feed_offset_) --wifi_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 13 || wifi_view_ == 14) return;
    if (wifi_view_ == 10) return;
    if (wifi_view_ == 9) { if (raw_filter_cursor_ < static_cast<size_t>(platform::wireless::FrameKind::Count)) ++raw_filter_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 8) { if (station_cursor_ + 1 < station_refs_.size()) ++station_cursor_; publish_wifi_scan(); return; }
    if (wifi_view_ == 7) { if (raw_feed_offset_) --raw_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 6) { if (wifi_feed_offset_) --wifi_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 4 || wifi_view_ == 5) {
        if (wifi_operation_index_ + 1 < (wifi_view_ == 4 ? 5u : 4u)) ++wifi_operation_index_;
        publish_wifi_scan(); return;
    }
    auto& index = wifi_view_ == 2 || wifi_view_ == 3 ? wifi_client_index_ : wifi_scan_detail_index_;
    const auto limit = (wifi_view_ == 2 || wifi_view_ == 3) && wifi_scan_detail_index_ < wifi_networks_.size() ?
        wifi_networks_[wifi_scan_detail_index_].clients.size() : wifi_networks_.size();
    if (index + 1 < limit) { ++index; if (wifi_view_ <= 1) wifi_info_tab_ = 0; }
    publish_wifi_scan();
}

void BaseViewModel::toggle_wifi_scan_details() {
    if (wifi_view_ == 11) {
        if (pineapple_cursor_ < pineapple_items_.size()) { wifi_view_ = 12; pineapple_action_cursor_ = 0; }
        publish_wifi_scan(); return;
    }
    if (wifi_view_ == 12) {
        if (pineapple_cursor_ >= pineapple_items_.size()) return;
        const auto& item = pineapple_items_[pineapple_cursor_];
        if (pineapple_action_cursor_ == 0) wifi_view_ = 13;
        else if (pineapple_action_cursor_ == 1) {
            const auto mhz = static_cast<unsigned>(item.frequency_mhz);
            if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), mhz) == wifi_frequencies_.end())
                wifi_scan_error_ = "Candidate channel unavailable";
            else if (pcap_pinned_frequency() && pcap_pinned_frequency() != mhz)
                wifi_scan_error_ = "Stop target PCAP to change channel";
            else {
                wifi_scan_error_.clear(); wifi_feed_target_ = item.mac;
                wifi_feed_frames_.clear(); wifi_feed_count_ = 0; wifi_feed_offset_ = 0;
                wifi_view_ = 15;
            }
        } else if (pineapple_action_cursor_ == 2) begin_pineapple_fox_hunt();
        wifi_hop_tick_ = 0; wifi_tune_failed_frequency_ = 0;
        publish_wifi_scan(); return;
    }
    if (wifi_view_ == 13 || wifi_view_ == 14 || wifi_view_ == 15) return;
    if (wifi_view_ == 9) {
        const auto bit = raw_filter_cursor_ ? platform::wireless::frame_kind_bit(
            static_cast<platform::wireless::FrameKind>(raw_filter_cursor_ - 1)) : 0u;
        raw_filter_mask_ = bit ? raw_filter_mask_ ^ bit : 0u;
        raw_feed_frames_.clear(); raw_feed_offset_ = 0; raw_displayed_count_ = 0;
        publish_wifi_scan(); return;
    }
    if (wifi_view_ == 7) { wifi_view_ = 0; wifi_scan_details_expanded_ = true; publish_wifi_scan(); return; }
    if (wifi_view_ == 10) return;
    if (wifi_view_ == 8) {
        if (station_cursor_ >= station_refs_.size()) return;
        wifi_scan_detail_index_ = station_refs_[station_cursor_].first;
        wifi_client_index_ = station_refs_[station_cursor_].second;
        station_from_global_ = true;
        wifi_view_ = 5; wifi_operation_index_ = 0;
        publish_wifi_scan(); return;
    }
    if (wifi_networks_.empty()) return;
    if (wifi_view_ == 0) { wifi_view_ = 4; wifi_operation_index_ = 0; }
    else if (wifi_view_ == 4) {
        if (wifi_operation_index_ == 0) { wifi_view_ = 1; wifi_info_tab_ = 0; }
        else if (wifi_operation_index_ == 1) { wifi_view_ = 2; wifi_client_index_ = 0; }
        else if (wifi_operation_index_ == 2) {
            const auto mhz = wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
            if (pcap_capture_.active() && pcap_capture_.filter().scope != platform::pcap::Scope::All &&
                pcap_capture_.filter().frequency_mhz != mhz) {
                wifi_scan_error_ = "Stop target PCAP to change channel";
                publish_wifi_scan(); return;
            }
            if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), static_cast<unsigned>(mhz)) == wifi_frequencies_.end()) {
                wifi_scan_error_ = "AP channel not available";
                publish_wifi_scan();
                return;
            }
            wifi_scan_error_.clear();
            wifi_feed_target_ = wifi_networks_[wifi_scan_detail_index_].bssid;
            wifi_feed_station_ = false; wifi_feed_frames_.clear(); wifi_feed_count_ = 0; wifi_feed_offset_ = 0;
            wifi_view_ = 6;
        }
        else if (wifi_operation_index_ == 4) begin_wifi_fox_hunt(false);
    } else if (wifi_view_ == 2 && !wifi_networks_[wifi_scan_detail_index_].clients.empty()) {
        station_from_global_ = false;
        wifi_view_ = 5; wifi_operation_index_ = 0;
    } else if (wifi_view_ == 5) {
        if (wifi_operation_index_ == 0) wifi_view_ = 3;
        else if (wifi_operation_index_ == 1) {
            const auto mhz = wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
            if (pcap_capture_.active() && pcap_capture_.filter().scope != platform::pcap::Scope::All &&
                pcap_capture_.filter().frequency_mhz != mhz) {
                wifi_scan_error_ = "Stop target PCAP to change channel";
                publish_wifi_scan(); return;
            }
            if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), static_cast<unsigned>(mhz)) == wifi_frequencies_.end()) {
                wifi_scan_error_ = "AP channel not available";
                publish_wifi_scan();
                return;
            }
            wifi_scan_error_.clear();
            wifi_feed_target_ = wifi_networks_[wifi_scan_detail_index_].clients[wifi_client_index_].mac;
            wifi_feed_station_ = true; wifi_feed_frames_.clear(); wifi_feed_count_ = 0; wifi_feed_offset_ = 0;
            wifi_view_ = 6;
        }
        else if (wifi_operation_index_ == 3) begin_wifi_fox_hunt(true);
    }
    wifi_hop_tick_ = 0;
    wifi_tune_failed_frequency_ = 0;
    wifi_scan_details_expanded_ = wifi_view_ != 0;
    publish_wifi_scan();
}

void BaseViewModel::wifi_back() {
    if (wifi_view_ == 11) { show_dashboard_page(); return; }
    if (wifi_view_ == 12) { wifi_view_ = 11; publish_wifi_scan(); return; }
    if (wifi_view_ == 13 || wifi_view_ == 14 || wifi_view_ == 15) {
        if (wifi_view_ == 14) { fox_hunt_.reset(); fox_target_ = FoxTarget::None; }
        wifi_view_ = 12; publish_wifi_scan(); return;
    }
    if (wifi_view_ == 7) { show_dashboard_page(); return; }
    if (wifi_view_ == 10) {
        fox_hunt_.reset(); fox_target_ = FoxTarget::None;
        wifi_view_ = wifi_feed_station_ ? 5 : 4;
        wifi_scan_details_expanded_ = true;
        publish_wifi_scan();
        return;
    }
    if (wifi_view_ == 9 || wifi_view_ == 8 || (raw_session_ && wifi_view_ == 0)) wifi_view_ = 7;
    else
    if (wifi_view_ == 6) wifi_view_ = wifi_feed_station_ ? 5 : 4;
    else if (wifi_view_ == 1 || wifi_view_ == 2) wifi_view_ = 4;
    else if (wifi_view_ == 3) wifi_view_ = 5;
    else if (wifi_view_ == 5) wifi_view_ = station_from_global_ ? 8 : 2;
    else wifi_view_ = 0;
    if (wifi_view_ == 0) wifi_info_tab_ = 0;
    wifi_scan_details_expanded_ = wifi_view_ != 0;
    publish_wifi_scan();
}
bool BaseViewModel::begin_wifi_fox_hunt(bool station) {
    if (wifi_scan_detail_index_ >= wifi_networks_.size()) return false;
    if (!wifi_scan_running_) { wifi_scan_error_ = "Monitor capture unavailable"; return false; }
    const auto& ap = wifi_networks_[wifi_scan_detail_index_];
    if (station && wifi_client_index_ >= ap.clients.size()) return false;
    const auto mhz = static_cast<unsigned>(ap.frequency_mhz);
    if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), mhz) == wifi_frequencies_.end()) {
        wifi_scan_error_ = "AP channel not available";
        return false;
    }
    if (pcap_pinned_frequency() && pcap_pinned_frequency() != mhz) {
        wifi_scan_error_ = "Stop target PCAP to change channel";
        return false;
    }
    const auto& client = station ? ap.clients[wifi_client_index_] : platform::wireless::WifiNetwork::Client{};
    fox_hunt_.begin(station ? client.mac : ap.bssid, mhz);
    fox_target_ = station ? FoxTarget::Station : FoxTarget::AccessPoint;
    fox_label_ = station ? client.mac : ap.ssid;
    fox_vendor_ = station ? client.vendor : ap.vendor;
    wifi_feed_station_ = station;
    wifi_view_ = 10;
    wifi_scan_error_.clear();
    wifi_hop_tick_ = 0;
    wifi_tune_failed_frequency_ = 0;
    fox_ui_tick_ = lv_tick_get();
    advance_wifi_channel();
    return true;
}
bool BaseViewModel::begin_pineapple_fox_hunt() {
    if (pineapple_cursor_ >= pineapple_items_.size()) return false;
    if (!wifi_scan_running_) { wifi_scan_error_ = "Monitor capture unavailable"; return false; }
    const auto& item = pineapple_items_[pineapple_cursor_];
    const auto mhz = static_cast<unsigned>(item.frequency_mhz);
    if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), mhz) == wifi_frequencies_.end()) {
        wifi_scan_error_ = "Candidate channel unavailable"; return false;
    }
    if (pcap_pinned_frequency() && pcap_pinned_frequency() != mhz) {
        wifi_scan_error_ = "Stop target PCAP to change channel"; return false;
    }
    fox_hunt_.begin(item.mac, mhz);
    fox_target_ = FoxTarget::Pineapple;
    fox_label_ = item.ssid;
    fox_vendor_ = item.vendor;
    wifi_view_ = 14;
    wifi_scan_error_.clear(); wifi_hop_tick_ = 0; wifi_tune_failed_frequency_ = 0;
    fox_ui_tick_ = lv_tick_get();
    advance_wifi_channel();
    return true;
}
void BaseViewModel::toggle_list_builder() {
    if (!wifi_scan_running_) return;
    wifi_list_builder_ = !wifi_list_builder_;
    if (!wifi_list_builder_) {
        std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
        incoming_frames_.clear();
    }
    publish_wifi_scan();
}
void BaseViewModel::toggle_raw_auto_hop() {
    if (!raw_session_ || !wifi_scan_running_) return;
    if (pcap_pinned_frequency()) {
        wifi_scan_error_ = "Stop target PCAP to change channel";
        publish_wifi_scan();
        return;
    }
    raw_auto_hop_ = !raw_auto_hop_;
    if (raw_auto_hop_) wifi_hop_tick_ = lv_tick_get();
    else {
        const auto found = std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), wifi_tuned_frequency_);
        if (found != wifi_frequencies_.end()) raw_channel_index_ = static_cast<std::size_t>(found - wifi_frequencies_.begin());
    }
    wifi_scan_error_.clear();
    publish_wifi_scan();
}
void BaseViewModel::step_raw_channel(int direction) {
    if (!raw_session_ || !wifi_scan_running_ || wifi_frequencies_.empty() || !direction) return;
    if (pcap_pinned_frequency()) {
        wifi_scan_error_ = "Stop target PCAP to change channel";
        publish_wifi_scan();
        return;
    }
    raw_auto_hop_ = false;
    const auto count = wifi_frequencies_.size();
    raw_channel_index_ = direction < 0 ? (raw_channel_index_ + count - 1) % count :
                                         (raw_channel_index_ + 1) % count;
    wifi_tune_failed_frequency_ = 0;
    wifi_scan_error_.clear();
    advance_wifi_channel();
    publish_wifi_scan();
}
void BaseViewModel::show_station_list() {
    if (!raw_session_ || wifi_view_ != 7) return;
    wifi_view_ = 8;
    wifi_scan_details_expanded_ = true;
    publish_wifi_scan();
}
void BaseViewModel::show_raw_filters() {
    if (!raw_session_ || !wifi_scan_running_) return;
    if (wifi_view_ == 9) wifi_view_ = 7;
    else { wifi_view_ = 9; raw_filter_cursor_ = 0; }
    wifi_scan_details_expanded_ = true;
    publish_wifi_scan();
}

void BaseViewModel::advance_wifi_info_tab() {
    if (wifi_view_ != 1) return;
    wifi_info_tab_ = (wifi_info_tab_ + 1) % 3;
    publish_wifi_scan();
}

bool BaseViewModel::set_light(int value) {
    if (value < 0 || light_max_ <= 0) return false;
    std::ofstream file(brightness_path);
    file << value;
    file.close();
    if (!file) return false;
    changed_light_ = value != original_light_;
    return true;
}

bool BaseViewModel::wake_display() {
    activity_tick_ = lv_tick_get();
    if (before_dim_ < 0) return false;
    if (set_light(before_dim_)) before_dim_ = -1;
    return true;
}

void BaseViewModel::idle_display() {
    if (!model_.dim_seconds() || before_dim_ >= 0 || light_max_ <= 0 ||
        lv_tick_elaps(activity_tick_) < model_.dim_seconds() * 1000) return;
    const int current = backlight_value(brightness_path);
    const int target = std::max(1, current / 5);
    if (current > target && set_light(target)) before_dim_ = current;
    else activity_tick_ = lv_tick_get();
}

void BaseViewModel::choose_upload_policy() {
    if (!model_.trusted_profile().empty()) {
        if (!model_.set_trusted_profile("")) resource_error_ = "Could not save policy";
        return;
    }
    const auto profile = active_wifi_profile();
    if (profile.first.empty()) {
        resource_error_ = "Connect one WiFi network";
        pending_uuid_.clear();
    } else if (profile.first == pending_uuid_) {
        if (!model_.set_trusted_profile(pending_uuid_)) resource_error_ = "Could not save policy";
        pending_uuid_.clear();
    } else {
        pending_uuid_ = profile.first;
        pending_network_ = profile.second;
    }
}

void BaseViewModel::request_quit() { quit_requested_subject_.set(true); }

void BaseViewModel::wifi_scan_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (self) {
        if (self->wardrive_upload_job_.valid() &&
            self->wardrive_upload_job_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            self->wardrive_message_ = self->wardrive_upload_job_.get();
            self->refresh_wardrive_files();
        } else if (self->wardrive_upload_job_.valid() &&
                   self->model_.current_page() == model::AppPage::WardriveFiles &&
                   lv_tick_elaps(self->wardrive_progress_tick_) >= 250) {
            self->wardrive_progress_tick_ = lv_tick_get();
            self->wardrive_files_generation_.set(self->wardrive_files_generation_.value() + 1);
        }
        if (self->credential_check_job_.valid() &&
            self->credential_check_job_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const auto result = self->credential_check_job_.get();
            auto& status = self->credential_check_service_ == "wigle" ? self->wigle_check_ : self->wdg_check_;
            if (status == "Checking")
                status = result.ok ? "Valid" : result.message.find("invalid") != std::string::npos ? "Invalid" : "Unavailable";
            self->resource_error_ = result.message;
            self->marauder_menu_generation_subject_.set(self->marauder_menu_generation_subject_.value() + 1);
        }
        if (self->map_job_.valid()) {
            if (self->map_job_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                self->map_message_ = self->map_job_.get();
                if (self->map_deleting_) {
                    if (self->map_message_ == "Map deleted" && self->model_.map_pack() == self->map_download_pack_) {
                        const unsigned fallback = platform::maps::installed(0) ? 0 : platform::maps::installed(1) ? 1 : 0;
                        if (self->model_.set_map_pack(fallback) && self->model_.set_map_zoom(6))
                            self->map_message_ = "Map deleted; Remote ID reset";
                    }
                    self->map_deleting_ = false;
                } else if ((self->map_message_ == "Map installed" || self->map_message_ == "Local map installed" || self->map_message_ == "Area map installed") &&
                    self->model_.set_map_zoom(self->map_download_zoom_) && self->model_.set_map_pack(self->map_download_pack_))
                    self->map_message_ = self->model_.censor_enabled() ?
                        "Installed; Censor hides map tiles" : "Installed and selected for Remote ID";
                ++self->map_storage_revision_;
                self->map_generation_.set(self->map_generation_.value() + 1);
            } else if (self->model_.current_page() == model::AppPage::MapPacks &&
                       lv_tick_elaps(self->map_progress_tick_) >= 250) {
                self->map_progress_tick_ = lv_tick_get();
                self->map_generation_.set(self->map_generation_.value() + 1);
            }
        }
        self->idle_display();
        if (self->monitor_restore_job_.valid() &&
            self->monitor_restore_job_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            for (const auto& id : self->monitor_restore_job_.get())
                self->active_monitor_ids_.insert(id);
            const auto& selected_id = self->model_.selected_adapter_id();
            self->monitor_owner_ = self->active_monitor_ids_.count(selected_id) ? selected_id : "";
            self->refresh_adapters();
            if (self->active_monitor_ids_.size() < self->model_.monitor_preferences().size()) {
                self->adapter_error_ = "Some saved monitors were unavailable or busy";
                self->publish_adapter_detail();
            }
        }
        if (self->monitor_job_.valid()) {
            if (self->monitor_job_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const auto message = self->monitor_job_.get();
                self->refresh_pending_monitor_status();
                const auto selected = std::find_if(self->adapters_.begin(), self->adapters_.end(), [&](const auto& adapter) {
                    return adapter.interface_name == self->monitor_pending_name_;
                });
                const bool confirmed = selected != self->adapters_.end() &&
                    selected->monitor_status == (message == "Monitor on" ? "On" : "Off");
                if (message == "Monitor on") {
                    self->active_monitor_ids_.insert(self->monitor_pending_id_);
                    if (!self->model_.set_monitor_preference(self->monitor_pending_id_, true))
                        self->adapter_error_ = "Monitor on; preference not saved";
                } else if (message == "Monitor off") {
                    self->active_monitor_ids_.erase(self->monitor_pending_id_);
                    if (!self->model_.set_monitor_preference(self->monitor_pending_id_, false))
                        self->adapter_error_ = "Monitor off; preference not saved";
                }
                self->monitor_owner_ = self->active_monitor_ids_.count(self->model_.selected_adapter_id()) ?
                    self->model_.selected_adapter_id() : "";
                self->monitor_pending_name_.clear();
                self->monitor_pending_id_.clear();
                if (self->adapter_error_.find("preference not saved") == std::string::npos)
                    self->adapter_error_ = message == "Monitor on" && !confirmed ? "Monitor mode unconfirmed" :
                        message == "Monitor off" && !confirmed ? "Check adapter restore" : message;
                self->publish_adapter_detail();
            } else if (!self->monitor_status_tick_ || lv_tick_elaps(self->monitor_status_tick_) >= 500) {
                self->monitor_status_tick_ = lv_tick_get();
                self->refresh_pending_monitor_status();
            }
        }
        self->update_gps_status();
        self->finish_wifi_scan_if_ready();
        const auto pcap_error = self->pcap_capture_.take_error();
        if (!pcap_error.empty()) {
            self->pcap_status_.set("PCAP!");
            self->pcap_message_ = pcap_error;
            self->refresh_pcap_files();
        }
        if (self->wifi_capture_failed_.exchange(false)) {
            self->stop_wifi_capture();
            self->wifi_scan_error_ = "Monitor capture stopped";
            if (self->wifi_view_ == 10) {
                self->wifi_view_ = self->wifi_feed_station_ ? 5 : 4;
                self->wifi_scan_details_expanded_ = true;
            }
            if (self->wifi_view_ == 14) self->wifi_view_ = 12;
            if (self->remote_view_ == 3) {
                self->remote_view_ = 1;
                self->remote_message_ = "Monitor capture stopped";
                self->publish_remote();
            }
            self->publish_wifi_scan();
        }
        self->advance_wifi_channel();
        self->finish_radio_setting();
        self->update_gps_tool();
        self->update_bluetooth_capture();
    }
}

void BaseViewModel::finish_wifi_scan_if_ready() {
    std::vector<platform::wireless::FrameObservation> incoming;
    std::vector<platform::wireless::FrameSummary> summaries;
    std::vector<platform::remote_id::Packet> remote;
    std::vector<platform::wireless::PineappleObservation> pineapple;
    {
        std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
        incoming.swap(incoming_frames_);
        summaries.swap(incoming_summaries_);
        remote.swap(incoming_remote_);
        pineapple.swap(incoming_pineapple_);
    }
    const auto now = lv_tick_get();
    if ((fox_target_ == FoxTarget::AccessPoint || fox_target_ == FoxTarget::Station ||
         fox_target_ == FoxTarget::Pineapple) &&
        wifi_tuned_frequency_ == fox_hunt_.frequency_mhz()) {
        for (const auto& frame : summaries)
            fox_hunt_.observe(frame.sender, static_cast<unsigned>(frame.frequency_mhz),
                              frame.rssi, frame.has_rssi, now);
    }
    if (raw_session_ || remote_session_) finish_remote_observations(remote);
    if (!pineapple.empty()) finish_pineapple_observations(pineapple);
    const bool fox_refresh = fox_hunt_.active() && lv_tick_elaps(fox_ui_tick_) >= 500;
    if (fox_refresh) fox_ui_tick_ = now;
    if (remote_session_) { if (fox_refresh) publish_remote(); return; }
    if (incoming.empty() && summaries.empty()) { if (fox_refresh) publish_wifi_scan(); return; }
    if (raw_session_) {
        for (const auto& frame : summaries) {
            if (raw_filter_mask_ && !(raw_filter_mask_ & platform::wireless::frame_kind_bit(frame.kind))) continue;
            ++raw_displayed_count_;
            if (raw_feed_frames_.size() == 128) raw_feed_frames_.pop_front();
            raw_feed_frames_.push_back(frame);
            if (raw_feed_offset_) raw_feed_offset_ = std::min(raw_feed_offset_ + 1, raw_feed_frames_.size() - 1);
        }
    }
    if ((wifi_view_ == 6 && wifi_scan_detail_index_ < wifi_networks_.size() ||
         wifi_view_ == 15 && pineapple_cursor_ < pineapple_items_.size()) && !wifi_feed_target_.empty()) {
        const int channel = wifi_view_ == 15 ? pineapple_items_[pineapple_cursor_].frequency_mhz :
            wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
        for (auto& frame : summaries) {
            if (!platform::wireless::frame_matches_target(frame, wifi_feed_target_, channel)) continue;
            ++wifi_feed_count_;
            if (wifi_feed_frames_.size() == 128) wifi_feed_frames_.pop_front();
            wifi_feed_frames_.push_back(std::move(frame));
            if (wifi_feed_offset_) wifi_feed_offset_ = std::min(wifi_feed_offset_ + 1, wifi_feed_frames_.size() - 1);
        }
    }
    bool wardrive_feed_changed = false;
    for (const auto& frame : incoming) {
        // The bounded recon list must not limit a wardrive's counts or CSV rows.
        if (wardrive_session_ && gps_log_.active() && frame.beacon) {
            int32_t lat{}, lon{};
            if (gps_session_.position(lat, lon)) {
                const auto frequency = frame.advertised_frequency_mhz ?
                    frame.advertised_frequency_mhz : frame.frequency_mhz;
                if (!gps_log_.access_point(frame.bssid,
                                           frame.ssid.empty() ? "[hidden/unknown]" : frame.ssid,
                                           platform::wireless::channel_number(frequency),
                                           frequency,
                                           frame.has_rssi ? frame.rssi : -128, frame.security, lat, lon))
                    gps_tool_message_ = "GPS file write failed";
                else {
                    if (wardrive_channels_.observe(frame.bssid, frequency,
                                                   frame.advertised_frequency_mhz != 0)) {
                        add_wardrive_sighting(false, frame.ssid.empty() ? "[hidden/unknown]" : frame.ssid,
                                              frame.bssid);
                        wardrive_feed_changed = true;
                    } else if (!frame.ssid.empty()) {
                        for (auto& sighting : wardrive_sightings_)
                            if (!sighting.bluetooth && sighting.address == frame.bssid &&
                                sighting.name == "[hidden/unknown]") {
                                sighting.name = frame.ssid;
                                wardrive_feed_changed = true;
                            }
                    }
                    add_wardrive_map_point(false, frame.bssid, lat, lon);
                }
            }
        }
        auto ap = std::find_if(wifi_networks_.begin(), wifi_networks_.end(), [&](const auto& item) { return item.bssid == frame.bssid; });
        if (ap == wifi_networks_.end()) {
            if (wifi_networks_.size() >= 512) continue;
            platform::wireless::WifiNetwork item;
            item.bssid = frame.bssid;
            item.ssid = frame.ssid.empty() ? "[hidden/unknown]" : frame.ssid;
            item.vendor = platform::wireless::vendor_for_mac(frame.bssid);
            wifi_networks_.push_back(std::move(item));
            ap = std::prev(wifi_networks_.end());
        }
        ++ap->frames;
        if (frame.beacon) {
            if (!frame.ssid.empty()) {
                ap->ssid = frame.ssid;
            }
            ap->security = frame.security;
            ap->security_info = frame.security_info;
            if (frame.has_rssi) { ap->rssi = frame.rssi; ap->has_rssi = true; }
        }
        // Radiotap can report the tuned channel for an adjacent-channel AP.
        // Prefer the primary channel advertised in the beacon/response IEs.
        if (frame.advertised_frequency_mhz) {
            ap->frequency_mhz = frame.advertised_frequency_mhz;
            ap->channel_advertised = true;
        } else if (frame.frequency_mhz && !ap->channel_advertised &&
                   (frame.beacon || !ap->frequency_mhz)) {
            ap->frequency_mhz = frame.frequency_mhz;
        }
        ap->channel = platform::wireless::channel_number(ap->frequency_mhz);
        if (frame.client.empty()) continue;
        auto client = std::find_if(ap->clients.begin(), ap->clients.end(), [&](const auto& item) { return item.mac == frame.client; });
        if (client == ap->clients.end()) {
            if (ap->clients.size() >= 64) continue;
            ap->clients.push_back({frame.client, platform::wireless::vendor_for_mac(frame.client)});
            client = std::prev(ap->clients.end());
            station_refs_.emplace_back(static_cast<size_t>(ap - wifi_networks_.begin()),
                                      static_cast<size_t>(client - ap->clients.begin()));
        }
        ++client->frames;
        client->handshake.observe(frame.eapol, frame.client_transmitted);
        if (frame.client_transmitted && frame.has_rssi) { client->rssi = frame.rssi; client->has_rssi = true; }
    }
    publish_wifi_scan();
    if (wardrive_feed_changed)
        gps_tool_generation_.set(gps_tool_generation_.value() + 1);
}
void BaseViewModel::finish_pineapple_observations(std::vector<platform::wireless::PineappleObservation>& incoming) {
    const auto now = lv_tick_get();
    bool changed = false;
    for (auto& seen : incoming) {
        auto item = std::find_if(pineapple_items_.begin(), pineapple_items_.end(),
                                 [&](const auto& candidate) { return candidate.mac == seen.mac; });
        if (item == pineapple_items_.end()) {
            if (pineapple_items_.size() >= 128) continue;
            if (seen.vendor == "Unknown") seen.vendor = platform::wireless::vendor_for_mac(seen.mac);
            seen.beacons = 1; seen.last_seen_ms = now;
            pineapple_items_.push_back(std::move(seen)); changed = true;
            continue;
        }
        ++item->beacons;
        item->last_seen_ms = now;
        item->clues |= seen.clues;
        item->evidence = item->clues == 3 ? "OUI + beacon pattern" :
            item->clues == 1 ? "Marauder OUI clue" : "Short beacon pattern";
        if (!seen.ssid.empty() && seen.ssid != "[hidden/unknown]") item->ssid = std::move(seen.ssid);
        if (seen.frequency_mhz) item->frequency_mhz = seen.frequency_mhz;
        if (seen.has_rssi) { item->rssi = seen.rssi; item->has_rssi = true; }
        if (!seen.security.empty()) item->security = std::move(seen.security);
        changed = true;
    }
    if (changed && pineapple_session_) publish_wifi_scan();
}
void BaseViewModel::finish_remote_observations(std::vector<platform::remote_id::Packet>& incoming) {
    const uint32_t now = lv_tick_get();
    bool changed = false;
    for (const auto& packet : incoming) {
        const auto& decoded = packet.record;
        if (fox_target_ == FoxTarget::Aircraft && wifi_tuned_frequency_ == fox_hunt_.frequency_mhz())
            fox_hunt_.observe(mac_text(decoded.mac), static_cast<unsigned>(packet.frequency_mhz),
                              packet.rssi, packet.has_rssi, now);
        auto* record = remote_store_.findByMac(decoded.mac);
        if (!record && decoded.hasUasId) record = remote_store_.findByUasId(decoded.uasId);
        if (!record) record = &remote_store_.observe(decoded.mac, packet.transport,
                                                    decoded.rssi, now);
        else if (std::memcmp(record->mac, decoded.mac, 6) == 0)
            remote_store_.observe(decoded.mac, packet.transport, decoded.rssi, now);
        else {
            std::memcpy(record->mac, decoded.mac, 6);
            record->lastSeenMs = now;
            ++record->packetCount;
            record->rssi = decoded.rssi;
            record->transportMask |= static_cast<uint8_t>(packet.transport);
        }
        merge_remote_record(*record, decoded);
        record->lastWifiChannel = decoded.lastWifiChannel;
        changed = true;
    }
    if (remote_store_.updateLifecycle(now, 15000) || changed) publish_remote();
}
void BaseViewModel::publish_remote() {
    inventory_generation_.set(inventory_generation_.value() + 1);
}

void BaseViewModel::publish_navigation() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

void BaseViewModel::publish_wifi_scan() {
    const auto text = format::wifi_scan_page(wifi_scan_interface_,
                                             wifi_networks_,
                                             wifi_scan_error_,
                                             wifi_scan_running_,
                                             wifi_scan_details_expanded_,
                                             wifi_scan_detail_index_);
    wifi_scan_subject_.set(text.c_str());
    inventory_generation_.set(inventory_generation_.value() + 1);
}

void BaseViewModel::publish_adapter_detail() {
    std::ostringstream out;
    if (adapters_.empty()) {
        out << "NO WIRELESS ADAPTERS\n"
            << (adapter_error_.empty() ? "No wireless sysfs interfaces detected" : adapter_error_);
        adapter_detail_subject_.set(out.str().c_str());
        return;
    }

    const auto& adapter = adapters_[highlighted_adapter_];
    const bool selected = model_.selected_adapter_id() == adapter.stable_id;
    if (!adapter_details_expanded_) {
        out << "ADAPTER " << (highlighted_adapter_ + 1) << '/' << adapters_.size()
            << (selected ? "  [SELECTED]" : "  [C/ENTER SELECT]") << '\n'
            << adapter.interface_name << "  " << adapter.bus_type << "  " << adapter.operstate << '\n';
        const auto product = format::clip_for_display(adapter.manufacturer +
                                              (adapter.manufacturer.empty() || adapter.product_name.empty() ? "" : " ") +
                                              adapter.product_name,
                                              42);
        if (!product.empty()) out << product << '\n';
        if (!adapter.vendor_id.empty()) out << "USB " << adapter.vendor_id << ':' << adapter.product_id << '\n';
        out << "Driver " << (adapter.driver.empty() ? "unknown" : adapter.driver)
            << "  PHY " << (adapter.phy_name.empty() ? "unknown" : adapter.phy_name) << '\n'
            << "MAC " << (adapter.mac_address.empty() ? "unknown" : screen_mac(adapter.mac_address)) << '\n'
            << "F/X ITEM C/ENTER SELECT\n"
            << "Z/ESC BACK D MORE";
    } else {
        out << "ADAPTER " << (highlighted_adapter_ + 1) << '/' << adapters_.size() << "  DETAILS\n"
            << adapter.interface_name << "  ifindex " << (adapter.ifindex.empty() ? "?" : adapter.ifindex) << '\n'
            << "ID " << (censor_enabled() ? "censored" : format::clip_for_display(adapter.stable_id, 30)) << '\n'
            << "MAC " << (adapter.permanent_mac ? "PERMANENT" : "ASSIGNED") << '\n'
            << "Monitor now " << adapter.monitor_status << "   Capability " << platform::wireless::capability_label(adapter.monitor_mode) << '\n'
            << "Injection " << platform::wireless::capability_label(adapter.injection) << '\n'
            << adapter.capability_note;
    }
    if (!adapter_error_.empty()) out << '\n' << adapter_error_;
    adapter_detail_subject_.set(out.str().c_str());
}

} // namespace viewmodel
