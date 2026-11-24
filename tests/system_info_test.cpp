/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include "display_format.h"
#include "system_info_model.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const auto end = text.find('\n', begin);
        result.push_back(text.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

bool bounded(const std::string& text, std::size_t max_lines) {
    const auto rendered = lines(text);
    if (!expect(rendered.size() <= max_lines, "rendered page has too many lines")) return false;
    for (const auto& line : rendered) {
        if (!expect(viewmodel::format::display_width(line) <= 30, "rendered line exceeds safe width")) return false;
    }
    return true;
}

bool valid_utf8(const std::string& value) {
    for (std::size_t index = 0; index < value.size();) {
        const auto lead = static_cast<unsigned char>(value[index]);
        std::size_t length = lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2 :
                             (lead & 0xf0) == 0xe0 ? 3 : (lead & 0xf8) == 0xf0 ? 4 : 0;
        if (length == 0 || index + length > value.size()) return false;
        for (std::size_t offset = 1; offset < length; ++offset) {
            if ((static_cast<unsigned char>(value[index + offset]) & 0xc0) != 0x80) return false;
        }
        index += length;
    }
    return true;
}

platform::diagnostics::Snapshot worst_case_snapshot() {
    platform::diagnostics::Snapshot snapshot;
    snapshot.model = "Model-" + std::string(80, 'M');
    snapshot.operating_system = "OS-" + std::string(80, 'O');
    snapshot.kernel = "Kernel-" + std::string(80, 'K');
    snapshot.uptime = "99d 23h 59m";
    snapshot.memory = "1.0 GiB free / 2.0 GiB";
    snapshot.storage = "3.0 GiB free / 4.0 GiB";
    for (int index = 0; index < 16; ++index) {
        const auto suffix = index < 10 ? "0" + std::to_string(index) : std::to_string(index);
        snapshot.network_interfaces.push_back({"if" + suffix, "driver" + suffix, "up", index % 2 == 0});
        snapshot.usb_devices.push_back({"1234", "ab" + suffix, "usb" + suffix});
        snapshot.peripheral_nodes.push_back("peripheral" + suffix);
        snapshot.tools.push_back({"tool" + suffix, index % 2 == 0});
    }
    snapshot.gpsd_socket = true;
    return snapshot;
}

bool system_pages_are_bounded_and_exhaustive() {
    const auto snapshot = worst_case_snapshot();
    model::SystemInfoModel navigation;
    navigation.refresh_counts(snapshot.network_interfaces.size(), snapshot.usb_devices.size(),
                              snapshot.peripheral_nodes.size(), snapshot.tools.size());

    std::set<std::string> seen;
    std::vector<std::pair<std::size_t, std::size_t>> route;
    for (;;) {
        route.emplace_back(navigation.page_index(), navigation.subpage_index());
        const auto compact = viewmodel::format::system_info_page(snapshot, navigation);
        if (!bounded(compact, 6)) return false;
        seen.insert(compact);

        navigation.toggle_details();
        const auto expanded = viewmodel::format::system_info_page(snapshot, navigation);
        if (!bounded(expanded, 6)) return false;
        if (!expect(compact != expanded, "System Info More view must change visible detail")) return false;
        navigation.toggle_details();

        const auto before = std::make_pair(navigation.page_index(), navigation.subpage_index());
        navigation.next_page();
        const auto after = std::make_pair(navigation.page_index(), navigation.subpage_index());
        if (after == before) break;
    }

    if (!expect(route.size() == 21, "unexpected exhaustive System Info subpage count")) return false;
    if (!expect(seen.size() == route.size(), "System Info route repeated a rendered page")) return false;
    for (int index = 0; index < 16; ++index) {
        const auto suffix = index < 10 ? "0" + std::to_string(index) : std::to_string(index);
        bool found_interface = false, found_usb = false, found_peripheral = false, found_tool = false;
        for (const auto& page : seen) {
            found_interface |= page.find("if" + suffix) != std::string::npos;
            found_usb |= page.find("usb" + suffix) != std::string::npos;
            found_peripheral |= page.find("peripheral" + suffix) != std::string::npos;
            found_tool |= page.find("tool" + suffix) != std::string::npos;
        }
        if (!expect(found_interface && found_usb && found_peripheral && found_tool,
                    "a worst-case System Info item was unreachable")) return false;
    }
    bool found_gpsd = false;
    for (const auto& page : seen) found_gpsd |= page.find("GPSD ready") != std::string::npos;
    if (!expect(found_gpsd, "GPSD row was unreachable")) return false;

    for (auto cursor = route.rbegin() + 1; cursor != route.rend(); ++cursor) {
        navigation.previous_page();
        if (!expect(std::make_pair(navigation.page_index(), navigation.subpage_index()) == *cursor,
                    "reverse System Info traversal was not deterministic")) return false;
    }
    navigation.previous_page();
    return expect(navigation.page() == model::SystemInfoPage::Summary && navigation.subpage_index() == 0,
                  "System Info traversal escaped its lower bound");
}

bool wifi_details_are_single_record_and_bounded() {
    const std::vector<platform::wireless::WifiNetwork> networks = {
        {"00:11:22:33:44:00", "network-zero-long-name", 1, 2412, 91, "WPA2"},
        {"00:11:22:33:44:01", "network-one-long-name", 6, 2437, 72, "WPA2/WPA3"},
        {"00:11:22:33:44:02", "network-two-long-name", 11, 2462, 53, "OPEN"},
    };
    for (std::size_t index = 0; index < networks.size(); ++index) {
        const auto text = viewmodel::format::wifi_scan_page("wlan0", networks, "", false, true, index);
        if (!bounded(text, 8)) return false;
        if (!expect(text.find("WIFI DETAILS " + std::to_string(index + 1) + "/3") != std::string::npos,
                    "Wi-Fi detail index is incorrect")) return false;
        if (!expect(text.find(networks[index].ssid) != std::string::npos &&
                    text.find(networks[index].bssid) != std::string::npos,
                    "Wi-Fi detail record is incomplete")) return false;
        for (std::size_t other = 0; other < networks.size(); ++other) {
            if (other != index && !expect(text.find(networks[other].ssid) == std::string::npos,
                                         "Wi-Fi details leaked another record")) return false;
        }
        if (!expect(text.find("5/7 ITEM  6 SCAN  8 SUMMARY") != std::string::npos,
                    "Wi-Fi detail controls are incorrect")) return false;
    }
    const auto summary = viewmodel::format::wifi_scan_page("wlan0", networks, "", false, false, 0);
    return bounded(summary, 8) && expect(summary.find("8 DETAILS") != std::string::npos,
                                         "Wi-Fi summary return state is incorrect");
}

bool clipping_is_utf8_safe_and_conservative() {
    const std::string source = "ASCII-" + std::string("é界🙂") + std::string(50, 'W');
    const auto clipped = viewmodel::format::clip_for_display(source, 12);
    if (!expect(valid_utf8(clipped), "clipping split a UTF-8 sequence")) return false;
    if (!expect(viewmodel::format::display_width(clipped) <= 12, "clipping exceeded its display budget")) return false;
    const std::string invalid = std::string("ok") + static_cast<char>(0xc0) + static_cast<char>(0xaf) + "end";
    const auto cleaned = viewmodel::format::clip_for_display(invalid, 30);
    return expect(valid_utf8(cleaned) && cleaned.find('?') != std::string::npos,
                  "invalid UTF-8 was not conservatively sanitized");
}

} // namespace

int main() {
    if (!system_pages_are_bounded_and_exhaustive()) return 1;
    if (!wifi_details_are_single_record_and_bounded()) return 2;
    if (!clipping_is_utf8_safe_and_conservative()) return 3;
    return 0;
}
