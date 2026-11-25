// SPDX-License-Identifier: AGPL-3.0-only
#include "wireless_scan.h"
using namespace platform::wireless;
int main() {
    WifiNetwork ap{};
    if (!parse_access_point("bssid=00:11:22:33:44:55\nssid=Cafe:Guest\nfreq=5955\nlevel=-53\nflags=[SAE][ESS]\n", ap)) return 1;
    if (ap.channel != 1 || ap.rssi != -53 || ap.ssid != "Cafe:Guest" || ap.security != "[SAE][ESS]") return 2;
    if (parse_access_point("bssid=invalid\n", ap)) return 3;
    return 0;
}
