/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include "display_format.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string_view>

namespace viewmodel::format {
namespace {
// Montserrat 10 can use almost ten pixels for a wide glyph. Keeping text to
// thirty display columns guarantees one line inside the 304 px content area.
constexpr std::size_t kLineColumns = 30;
constexpr std::size_t kWifiRows = 5;

struct Codepoint {
    std::size_t bytes;
    std::size_t columns;
    bool valid;
};

Codepoint decode(std::string_view value, std::size_t index) {
    const auto lead = static_cast<unsigned char>(value[index]);
    if (lead < 0x80) return {1, 1, true};
    std::size_t length = 0;
    unsigned int codepoint = 0;
    unsigned int minimum = 0;
    if ((lead & 0xe0) == 0xc0) { length = 2; codepoint = lead & 0x1f; minimum = 0x80; }
    else if ((lead & 0xf0) == 0xe0) { length = 3; codepoint = lead & 0x0f; minimum = 0x800; }
    else if ((lead & 0xf8) == 0xf0) { length = 4; codepoint = lead & 0x07; minimum = 0x10000; }
    else return {1, 1, false};
    if (index + length > value.size()) return {1, 1, false};
    for (std::size_t offset = 1; offset < length; ++offset) {
        const auto byte = static_cast<unsigned char>(value[index + offset]);
        if ((byte & 0xc0) != 0x80) return {1, 1, false};
        codepoint = (codepoint << 6) | (byte & 0x3f);
    }
    if (codepoint < minimum || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) return {1, 1, false};
    return {length, 2, true};
}

std::string clean_line(const std::string& value) {
    std::string clean;
    clean.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        const auto decoded = decode(value, index);
        const auto byte = static_cast<unsigned char>(value[index]);
        if (!decoded.valid) {
            clean.push_back('?');
            ++index;
        } else if (byte < 0x20 || byte == 0x7f) {
            clean.push_back(' ');
            index += decoded.bytes;
        } else {
            clean.append(value, index, decoded.bytes);
            index += decoded.bytes;
        }
    }
    return clean;
}

std::string bounded_line(const std::string& value) {
    return clip_for_display(value, kLineColumns);
}

std::string bool_word(bool value, const char* yes, const char* no) { return value ? yes : no; }

std::string summary_item(const platform::diagnostics::Snapshot& snapshot,
                         std::size_t index,
                         bool details) {
    std::size_t wireless = 0;
    for (const auto& item : snapshot.network_interfaces) wireless += item.wireless ? 1 : 0;
    switch (index) {
        case 0: return (details ? "Model: " : "HW ") + snapshot.model;
        case 1: return (details ? "Memory: " : "RAM ") + snapshot.memory;
        case 2: return (details ? "Storage: " : "Disk ") + snapshot.storage;
        case 3: return std::to_string(snapshot.network_interfaces.size()) + " interfaces, " +
                       std::to_string(wireless) + " wireless";
        case 4: return std::to_string(snapshot.usb_devices.size()) + " USB, " +
                       std::to_string(snapshot.peripheral_nodes.size()) + " peripherals";
        default: return std::string("GPSD ") + bool_word(snapshot.gpsd_socket, "ready", "not detected");
    }
}

std::string system_item(const platform::diagnostics::Snapshot& snapshot,
                        std::size_t index,
                        bool details) {
    const std::array<std::pair<const char*, const std::string*>, 6> fields = {{
        {"Model", &snapshot.model}, {"OS", &snapshot.operating_system}, {"Kernel", &snapshot.kernel},
        {"Uptime", &snapshot.uptime}, {"Memory", &snapshot.memory}, {"Storage", &snapshot.storage},
    }};
    const auto& field = fields[index];
    return std::string(details ? field.first : std::string(field.first).substr(0, 3)) + ": " + *field.second;
}

std::string category_item(const platform::diagnostics::Snapshot& snapshot,
                          model::SystemInfoPage page,
                          std::size_t index,
                          bool details) {
    if (page == model::SystemInfoPage::Summary) return summary_item(snapshot, index, details);
    if (page == model::SystemInfoPage::System) return system_item(snapshot, index, details);
    if (page == model::SystemInfoPage::Interfaces) {
        if (snapshot.network_interfaces.empty()) return "No interfaces detected";
        const auto& item = snapshot.network_interfaces[index];
        if (details) return item.name + "  " + item.driver + "  " + item.state + (item.wireless ? " wifi" : "");
        return item.name + "  " + item.state + (item.wireless ? " wifi" : "");
    }
    if (page == model::SystemInfoPage::UsbPeripherals) {
        if (snapshot.usb_devices.empty() && snapshot.peripheral_nodes.empty()) return "No USB or peripherals detected";
        if (index < snapshot.usb_devices.size()) {
            const auto& item = snapshot.usb_devices[index];
            return details ? "USB " + item.vendor_id + ":" + item.product_id + " " + item.name
                           : "USB " + item.name;
        }
        const auto& node = snapshot.peripheral_nodes[index - snapshot.usb_devices.size()];
        return details ? "Device node: /dev/" + node : "DEV /dev/" + node;
    }
    if (index < snapshot.tools.size()) {
        const auto& item = snapshot.tools[index];
        return details ? item.name + ": " + bool_word(item.available, "available", "not installed")
                       : std::string(item.available ? "[OK] " : "[--] ") + item.name;
    }
    return std::string(details ? "GPSD socket: " : "GPSD ") +
           bool_word(snapshot.gpsd_socket, "ready", "not detected");
}

std::size_t category_item_count(const platform::diagnostics::Snapshot& snapshot,
                                model::SystemInfoPage page) {
    switch (page) {
        case model::SystemInfoPage::Summary: return 6;
        case model::SystemInfoPage::System: return 6;
        case model::SystemInfoPage::Interfaces: return std::max<std::size_t>(1, snapshot.network_interfaces.size());
        case model::SystemInfoPage::UsbPeripherals:
            return std::max<std::size_t>(1, snapshot.usb_devices.size() + snapshot.peripheral_nodes.size());
        case model::SystemInfoPage::Tools: return std::max<std::size_t>(1, snapshot.tools.size() + 1);
    }
    return 1;
}
} // namespace

std::size_t display_width(const std::string& value) {
    const auto clean = clean_line(value);
    std::size_t width = 0;
    for (std::size_t index = 0; index < clean.size();) {
        const auto decoded = decode(clean, index);
        width += decoded.columns;
        index += decoded.bytes;
    }
    return width;
}

std::string clip_for_display(const std::string& value, std::size_t max_columns) {
    const auto clean = clean_line(value);
    if (display_width(clean) <= max_columns) return clean;
    if (max_columns == 0) return {};
    const bool ellipsis = max_columns >= 3;
    const std::size_t budget = ellipsis ? max_columns - 3 : max_columns;
    std::string clipped;
    std::size_t width = 0;
    for (std::size_t index = 0; index < clean.size();) {
        const auto decoded = decode(clean, index);
        if (width + decoded.columns > budget) break;
        clipped.append(clean, index, decoded.bytes);
        width += decoded.columns;
        index += decoded.bytes;
    }
    if (ellipsis) clipped += "...";
    return clipped;
}

std::string menu_row_label(const std::string& label, bool available) {
    return label + (available ? "" : "  [soon]");
}

std::string system_info_page(const platform::diagnostics::Snapshot& snapshot,
                             const model::SystemInfoModel& navigation) {
    std::ostringstream out;
    out << model::system_info_page_title(navigation.page()) << ' '
        << (navigation.page_index() + 1) << '/' << model::SystemInfoModel::kPageCount << "  "
        << (navigation.subpage_index() + 1) << '/' << navigation.subpage_count() << '\n';
    if (navigation.details_expanded()) out << "Details\n";
    const auto count = category_item_count(snapshot, navigation.page());
    const auto begin = navigation.subpage_index() * model::SystemInfoModel::kRowsPerSubpage;
    const auto end = std::min(begin + model::SystemInfoModel::kRowsPerSubpage, count);
    for (std::size_t index = begin; index < end; ++index) {
        out << bounded_line(category_item(snapshot, navigation.page(), index, navigation.details_expanded())) << '\n';
    }
    return out.str();
}

std::string wifi_scan_page(const std::string& interface_name,
                           const std::vector<platform::wireless::WifiNetwork>& networks,
                           const std::string& error,
                           bool running,
                           bool details,
                           std::size_t page_index) {
    std::ostringstream out;
    if (interface_name.empty()) {
        out << "WIFI SCAN\n"
            << bounded_line(error.empty() ? "Select a WiFi adapter first" : error);
        return out.str();
    }
    if (details) {
        const std::size_t pages = std::max<std::size_t>(1, networks.size());
        const std::size_t selected = std::min(page_index, pages - 1);
        out << "WIFI DETAILS " << (selected + 1) << '/' << pages << '\n';
        if (networks.empty()) {
            out << "No network details available\n";
        } else {
            const auto& network = networks[selected];
            out << bounded_line("SSID " + network.ssid) << '\n'
                << bounded_line("BSSID " + network.bssid) << '\n'
                << bounded_line("VENDOR " + (network.vendor.empty() ? std::string("Unknown") : network.vendor)) << '\n'
                << bounded_line("SEC " + network.security) << '\n'
                << "SIGNAL " << network.signal_percent << "%  CHANNEL " << network.channel << '\n'
                << "FREQ " << network.frequency_mhz << " MHz\n";
        }
        out << bounded_line("IF " + interface_name + "  " + (running ? "scanning" : error.empty() ? "ready" : "error"));
        return out.str();
    }

    const std::size_t pages = std::max<std::size_t>(1, (networks.size() + kWifiRows - 1) / kWifiRows);
    const std::size_t selected = std::min(page_index / kWifiRows, pages - 1);
    out << bounded_line("WIFI SCAN " + interface_name + "  " + std::to_string(selected + 1) + "/" + std::to_string(pages)) << '\n';
    if (running) out << "SCANNING - previous results\n";
    else if (!error.empty()) out << bounded_line(error) << '\n';
    else out << networks.size() << " NETWORKS  strongest first\n";
    const auto begin = selected * kWifiRows;
    const auto end = std::min(begin + kWifiRows, networks.size());
    for (std::size_t index = begin; index < end; ++index) {
        const auto& network = networks[index];
        out << bounded_line(std::string(index == page_index ? "> " : "  ") + std::to_string(network.signal_percent) + "% C" + std::to_string(network.channel) + " " +
                            network.ssid + " " + network.security) << '\n';
    }
    if (begin == end && !running && error.empty()) out << "No networks found\n";
    return out.str();
}

} // namespace viewmodel::format
