// SPDX-License-Identifier: AGPL-3.0-only
#include "wireless_scan.h"
using namespace platform::wireless;
int main() {
    WifiNetwork ap{};
    if (!parse_access_point("bssid=00:11:22:33:44:55\nssid=Cafe:Guest\nfreq=5955\nlevel=-53\nflags=[SAE][ESS]\n", ap)) return 1;
    if (ap.channel != 1 || ap.rssi != -53 || ap.ssid != "Cafe:Guest" || ap.security != "[SAE][ESS]") return 2;
    if (parse_access_point("bssid=invalid\n", ap)) return 3;
    WifiNetwork client{};
    if (!parse_hotspot_station("00:11:22:33:44:55\nsignal=-61\nrx_packets=12\ntx_packets=33\n", client)) return 4;
    if (client.rx_packets != "12" || client.tx_packets != "33" || client.rssi != -61) return 5;
    if (parse_hotspot_station("FAIL\n", client)) return 6;
    return 0;
}
