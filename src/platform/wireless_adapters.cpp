/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wireless_adapters.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace platform::wireless {
namespace {

namespace fs = std::filesystem;

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string read_value(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::string value;
    if (file) {
        std::getline(file, value);
    }
    return trim(value);
}

std::string canonical_path(const fs::path& path) {
    std::error_code ec;
    const auto resolved = fs::canonical(path, ec);
    return ec ? std::string{} : resolved.string();
}

std::string basename_of(const std::string& path) {
    return path.empty() ? std::string{} : fs::path(path).filename().string();
}

fs::path find_usb_parent(fs::path path) {
    while (!path.empty() && path != path.root_path()) {
        if (fs::is_regular_file(path / "idVendor") && fs::is_regular_file(path / "idProduct")) {
            return path;
        }
        path = path.parent_path();
    }
    return {};
}

bool meaningful_serial(const std::string& serial) {
    return !serial.empty() && serial != "0" && serial != "000000000";
}

std::string stable_identity(const AdapterInfo& adapter) {
    if (adapter.bus_type == "USB" && meaningful_serial(adapter.serial)) {
        return "usb:" + adapter.vendor_id + ':' + adapter.product_id + ':' + adapter.serial;
    }
    if (adapter.permanent_mac && !adapter.mac_address.empty()) {
        return "mac:" + adapter.mac_address;
    }
    if (!adapter.device_path.empty()) {
        return "path:" + adapter.device_path;
    }
    return "ifindex:" + adapter.ifindex;
}

} // namespace

std::string AdapterInfo::display_name() const {
    if (manufacturer.empty()) return product_name.empty() ? interface_name : product_name;
    if (product_name.empty() || product_name == manufacturer) return manufacturer;
    return manufacturer + " " + product_name;
}

Catalog enumerate() {
    Catalog catalog;
    const fs::path root("/sys/class/net");
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        catalog.error = "Network interface sysfs is unavailable";
        return catalog;
    }

    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (ec) {
            catalog.error = "Unable to enumerate network interfaces";
            break;
        }
        if (!fs::is_directory(entry.path() / "wireless")) {
            continue;
        }

        AdapterInfo adapter;
        adapter.interface_name = entry.path().filename().string();
        adapter.ifindex = read_value(entry.path() / "ifindex");
        adapter.operstate = read_value(entry.path() / "operstate");
        adapter.mac_address = read_value(entry.path() / "address");
        adapter.permanent_mac = read_value(entry.path() / "addr_assign_type") == "0";
        adapter.device_path = canonical_path(entry.path() / "device");
        adapter.driver = basename_of(canonical_path(entry.path() / "device/driver"));
        adapter.phy_name = basename_of(canonical_path(entry.path() / "phy80211"));
        adapter.bus_type = "ONBOARD";

        const auto usb_parent = find_usb_parent(fs::path(adapter.device_path));
        if (!usb_parent.empty()) {
            adapter.bus_type = "USB";
            adapter.vendor_id = read_value(usb_parent / "idVendor");
            adapter.product_id = read_value(usb_parent / "idProduct");
            adapter.manufacturer = read_value(usb_parent / "manufacturer");
            adapter.product_name = read_value(usb_parent / "product");
            adapter.serial = read_value(usb_parent / "serial");
        }

        adapter.stable_id = stable_identity(adapter);
        const auto monitor_probe = probe_monitor_mode(adapter.phy_name);
        adapter.bands = monitor_probe.bands;
        adapter.monitor_mode = monitor_probe.state;
        adapter.injection = CapabilityState::Unverified;
        adapter.capability_note = monitor_probe.note;
        catalog.adapters.push_back(std::move(adapter));
    }

    std::sort(catalog.adapters.begin(), catalog.adapters.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.bus_type != rhs.bus_type) {
            return lhs.bus_type == "USB";
        }
        return lhs.interface_name < rhs.interface_name;
    });
    return catalog;
}

const char* capability_label(CapabilityState state) {
    switch (state) {
        case CapabilityState::Supported:
            return "SUPPORTED";
        case CapabilityState::Unsupported:
            return "UNSUPPORTED";
        case CapabilityState::Unverified:
            return "UNVERIFIED";
        case CapabilityState::Unknown:
        default:
            return "UNKNOWN";
    }
}

} // namespace platform::wireless
