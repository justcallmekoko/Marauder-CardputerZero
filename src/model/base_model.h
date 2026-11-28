/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <set>

namespace model {

enum class BatteryDisplay { Both, Icon, Percent };
const char* battery_display_name(BatteryDisplay mode);

enum class AppPage {
    Dashboard = 0,
    Adapters = 1,
    WifiScan = 2,
    Diagnostics = 3,
    RadioSetup,
    BluetoothAdapters,
    Traffic,
    PcapFiles,
    WardriveFiles,
    RemoteId,
    GpsTool,
    MapPacks,
    BluetoothCapture,
};

std::string saved_gps_id();
bool save_gps_id(const std::string& id);

enum class Credential { WigleName, WigleToken, WdgKey };
bool credential_saved(Credential field);
bool store_credential(Credential field, const std::string& value);

class BaseModel {
public:
    BaseModel();
    BatteryDisplay battery_display() const { return battery_display_; }
    void cycle_battery_display();
    bool censor_enabled() const { return censor_enabled_; }
    bool toggle_censor();
    bool screenshots_in_documents() const { return documents_; }
    unsigned cache_kib() const { return cache_kib_; }
    unsigned brightness() const { return brightness_; }
    unsigned dim_seconds() const { return dim_seconds_; }
    unsigned reserve_mib() const { return reserve_mib_; }
    const std::string& trusted_profile() const { return trusted_profile_; }
    bool cycle_brightness();
    bool cycle_dim();
    bool cycle_reserve();
    bool set_trusted_profile(const std::string& uuid);
    bool cycle_storage();
    bool cycle_cache();
    const char* app_title() const;
    bool dark_mode() const;
    AppPage current_page() const;
    void set_current_page(AppPage page);
    void toggle_page();
    const std::string& selected_adapter_id() const;
    bool set_selected_adapter_id(std::string stable_id);
    const std::string& selected_bluetooth_id() const { return selected_bluetooth_id_; }
    bool set_selected_bluetooth_id(std::string stable_id);
    bool traffic_monitor_source() const { return traffic_monitor_source_; }
    bool cycle_traffic_source();
    unsigned wardrive_dwell_ms() const { return wardrive_dwell_ms_; }
    unsigned wardrive_priority() const { return wardrive_priority_; }
    bool wardrive_all_idle_adapters() const { return wardrive_all_idle_adapters_; }
    bool cycle_wardrive_dwell();
    bool cycle_wardrive_priority();
    bool cycle_wardrive_adapters();
    const std::set<std::string>& monitor_preferences() const { return monitor_preferences_; }
    bool set_monitor_preference(const std::string& stable_id, bool enabled);
    unsigned map_zoom() const { return map_zoom_; }
    unsigned map_pack() const { return map_pack_; }
    bool set_map_zoom(unsigned zoom);
    bool set_map_pack(unsigned pack);

private:
    unsigned brightness_{0}, dim_seconds_{0}, reserve_mib_{64};
    std::string trusted_profile_;
    bool documents_{false};
    unsigned cache_kib_{0};
    BatteryDisplay battery_display_{BatteryDisplay::Both};
    bool censor_enabled_{};
    AppPage current_page_{AppPage::Dashboard};
    std::string selected_adapter_id_;
    std::string selected_bluetooth_id_;
    bool traffic_monitor_source_{true};
    unsigned wardrive_dwell_ms_{650};
    unsigned wardrive_priority_{};
    bool wardrive_all_idle_adapters_{true};
    std::set<std::string> monitor_preferences_;
    unsigned map_zoom_{6};
    unsigned map_pack_{};
};

} // namespace model
