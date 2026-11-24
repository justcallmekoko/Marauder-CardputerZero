/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wireless_scan.h"

using platform::wireless::parse_nmcli_wifi_scan;
using platform::wireless::split_nmcli_terse_row;

int main() {
    const auto escaped = split_nmcli_terse_row(R"(AA\:BB\:CC\:DD\:EE\:FF:Cafe\:Guest:11:2462:67:WPA2)");
    if (escaped.size() != 6 || escaped[0] != "AA:BB:CC:DD:EE:FF" || escaped[1] != "Cafe:Guest") return 1;

    const auto result = parse_nmcli_wifi_scan(R"(00\:11\:22\:33\:44\:55:Weak:6:2437:20:WPA2
AA\:BB\:CC\:DD\:EE\:FF:Cafe\:Guest:11:2462 MHz:83:WPA2 WPA3
12\:34\:56\:78\:9A\:BC::1:2412:55:
)");
    if (!result.error.empty() || result.networks.size() != 3) return 2;
    if (result.networks[0].ssid != "Cafe:Guest" || result.networks[0].signal_percent != 83) return 3;
    if (result.networks[1].ssid != "<hidden>" || result.networks[1].security != "OPEN") return 4;
    if (result.networks[2].channel != 6 || result.networks[2].frequency_mhz != 2437) return 5;

    const auto malformed = parse_nmcli_wifi_scan("not-a-valid-row\n");
    if (malformed.error.empty() || !malformed.networks.empty()) return 6;

    std::string unsafe = R"(22\:33\:44\:55\:66\:77:Bad)";
    unsafe.push_back('\x01');
    unsafe += "Name:1:2412:50:WPA2\n";
    const auto sanitized = parse_nmcli_wifi_scan(unsafe);
    if (!sanitized.error.empty() || sanitized.networks.size() != 1 ||
        sanitized.networks[0].ssid != "Bad Name") return 7;
    return 0;
}
