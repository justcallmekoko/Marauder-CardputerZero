# Marauder for Cardputer Zero

Native C++17/LVGL 9.5 port of ESP32 Marauder for the ARM64 Linux-based Cardputer Zero.

> Status: early development. The current milestone includes diagnostics, radio setup, monitor-mode switching, and live access point and observed client lists from raw frames.

## Current milestone

- Cardputer Zero APPLaunch integration and Debian packaging scaffold
- 320x170 LVGL interface with a four-tile keyboard launcher
- Dashboard, wireless-adapter selection, and five bounded keyboard-reachable System Info pages
- Read-only wireless adapter catalog with interface, driver, PHY, bus, VID/PID, MAC, stable identity, and explicit monitor/injection verification state
- Keyboard-driven adapter browsing and in-app selection without changing interface state. WiFi → Setup and Bluetooth → Setup expose their respective adapter pickers, while Device → Radio Setup controls WiFi, Bluetooth, and GPS hardware from one inventory.
- Viewport-safe summary/details paging for adapter information (`D` toggles pages)
- Read-only kernel capability probing for monitor-mode support; injection remains unverified until an explicit hardware test
- Live C++ radiotap and 802.11 frame parsing on the selected monitor-mode adapter, with scrolling AP and observed-client lists, RSSI, frame counts, and offline vendor lookup
- AP information shows sequential EAPOL four-way frame progress per observed client and separately indicates PMKID sightings. "Observed" means matching frame metadata was seen; MICs are not cryptographically verified and a PCAP exists only when recording was enabled.
- Press `P` outside Bluetooth capture to start or stop raw monitor-mode Wi-Fi PCAP recording. The status bar shows `REC` while recording. Captures are saved under `~/Documents/Marauder/pcap`; Device → Files & Storage → Saved captures lists filenames, dates, and sizes. Recording stops at the configured free-space reserve and on app exit.
- AP and station operation menus offer explicit target recording. These PCAPs retain only frames whose sender or receiver matches the selected device on its AP channel; the channel and target remain fixed until recording stops. `P` continues to start an all-frame recording, or stops whichever recording is active. The status bar distinguishes `REC ALL`, `REC AP`, and `REC STA`.
- AP, station, Remote ID aircraft, and Pineapple candidate actions include **Fox hunt**: a passive, channel-pinned signal view with target identity, vendor when known, RSSI, freshness, frame count, and a muted 20-sample strength trace. Only frames transmitted by the selected device update its RSSI; a missing signal becomes stale rather than looking live. The display is a relative signal guide, not a distance estimate.
- **Bluetooth → Monitor → Raw Advertisements** shows a live LE advertisement sighting feed with address, name, signal, detector tag, and available advertisement field bytes. `T` opens combinable Find My, iBeacon, Flipper, Flock, Meta, and serial-module display filters; `B` toggles a bounded device list builder, and `L` opens the built list. `F`/`X` scrolls feed history or device rows. Select a device for Information (address type, observed device type, manufacturer evidence, RSSI, and sighting count) or Fox hunt. Bluetooth opens to Discover, Monitor, Attacks, and Setup tiles. Discover contains Nearby Devices plus direct Find My, iBeacon, Flipper, Flock, Meta, and Serial Modules filtered collectors in one keyboard-scrollable list. Monitor opens Raw Advertisements; Setup opens the Bluetooth adapter picker. Attacks retains its unavailable future-tool placeholders. Each filtered discovery entry opens the same collector with the matching filter and list building enabled. These signatures identify candidates, not verified product identities; HC-03/05/06 names are serial-module clues and do not establish that a card skimmer is present. A restricted HCI monitor helper reads each received LE advertising report on the enabled controller and forwards its full report payload bytes and RSSI to the C++ parser. BlueZ starts and stops discovery but does not supply the raw feed. Extended advertisements can arrive in segments; this screen shows each received report, not a reassembled higher-level message. Bluetooth PCAP recording is not yet supported.\r\n- **WiFi → Monitor → Raw Frames** shows each received monitor frame as a live, scrolling summary, with type, sender, receiver, signal when available, tuned channel, and shown/received counts. It starts on one supported channel; `Z`/`C` manually tunes to the previous/next supported channel, and `H` toggles automatic channel hopping. Unparsed frames appear as such rather than silently disappearing. `T` opens frame-type filters: Beacon, Probe, Deauth/disassoc, EAPOL/PMKID, Remote ID, Pineapple, SAE, Data, Association, Authentication, Action, Control, and Other. Enter toggles a type; All frames clears the display filter. Type colors match between the feed and filter list. Changing filters resets the shown count and recent feed; the received count continues. Filters only affect the on-screen feed, not PCAP recording. Remote ID and Pineapple detections update their respective live lists even when AP/station list building is paused or their display filters are off; those lists remain available during the app session on the same adapter. `B` starts or pauses AP/station list building without stopping raw capture. The BUILD badge shows its state. Enter opens the growing AP list; `D` opens the associated-station list. F/X scrolls lists and the frame feed. The AP and station lists remain available when switching from Raw Frames to WiFi → Discover → Access Points on the same adapter. The Access Points screen shows the currently tuned channel; `D` there starts a fresh list.
- **WiFi → Discover → Pineapple Detector** builds a live list of candidate devices from beacon frames. The detector follows ESP32 Marauder's OUI and short-beacon clues; a matching fingerprint is not proof that the hardware is a Wi-Fi Pineapple, since OUIs and beacon contents can be shared or spoofed. Select a candidate for Information (identity, vendor, security, channel, RSSI, beacon count, and match evidence), a channel-pinned traffic filter, Fox hunt, or target PCAP recording. The dedicated list scans supported channels; the candidate's actions pin its advertised primary channel. `D` restarts the list.
- **Location → Remote ID** listens for Wi-Fi Beacon and NAN broadcasts on the selected monitor-mode adapter and Bluetooth LE ASTM service-data broadcasts on the selected Bluetooth adapter. Either radio can collect alone when the other is unavailable. A live list shows aircraft ID, transport, Wi-Fi channel when known, signal, and distance when the Cardputer has a fresh GPS fix. The relative-position map centers on the Cardputer and shows aircraft positions and recent paths; without a fix it shows a clear GPS prompt while collection continues. `F`/`X` selects an aircraft; Enter opens its Information and Fox hunt actions. Information has identity, position, operator, and transport details; Esc steps back through actions to the map. Bluetooth decoding accepts complete legacy and extended advertising reports; segmented extended advertisements are not reassembled. The display depends on receiving an aircraft broadcast and on the aircraft broadcasting its own location.
- The Wi-Fi Remote ID decoder and relative-position math are adapted from ESP32 Marauder's MIT-licensed `RemoteIdDecoder` and `RemoteIdModel`; the original license is included in `REMOTE_ID_MIT_LICENSE.txt`. The Pineapple Pager Remote ID monitor was consulted for its Wi-Fi transport handling.
- Direct menu navigation with the printed `Z`/`X`/`C`/`F` arrow cluster, `Enter` for the primary action, and `D` for details on supported screens. Lists wrap from first to last and last to first; tile menus wrap across rows and columns.
- Context key guides highlight the key names in muted blue, separate from their action descriptions. Adapter mode changes show the observed interface state while the helper finishes and update the adapter screen without rerunning a full capability scan on the UI thread.
- APPLaunch icon uses the white JCMK LLC mark on a black background
- Read-only inventory of:
  - operating system, kernel, uptime, memory, and storage
  - network interfaces, Linux drivers, state, and wireless classification
  - USB VID/PID and product names
  - serial, I2C, SPI, and GPIO device nodes
  - availability of radio and diagnostic command-line tools
  - gpsd socket presence
- Graphite theme with muted category accents
- No background service and no permanent root process

## Hardware contract under evaluation

The first validation platform currently includes:

- Cardputer Zero (Raspberry Pi Compute Module 0)
- CardputerZero CAP LoRa1262/GNSS module on the GPIO expansion
- Alfa AWUS036ACM USB Wi-Fi adapter (MediaTek MT7612U / `mt76x2u`)

The adapter layer enumerates Linux wireless interfaces, correlates USB metadata, identifies backing drivers and PHYs, and lets the user choose an adapter without changing radio state. It queries kernel-advertised monitor-mode support through nl80211/`iw` without changing interface state. Injection remains `UNVERIFIED` until a later safe, reversible runtime validation; a USB device name or driver alone is not proof of injection support.

## Keyboard workflow

The launcher has four tiles. Use `F/X` for up/down and `Z/C` for left/right. `Enter` opens the focused tile and `Esc` returns from submenus. At the launcher root, `Esc` opens an exit confirmation. Press `Enter` to exit or `Esc` to stay on the current tile. Unimplemented functions carry a “soon” label.

The menu text uses the classic 5×7 glyphs from the Cardputer firmware. Under **Device → Display & Privacy → Battery display**, press `Enter` to cycle through icon, percentage, and both. The choice is saved per user. Battery indicators are red at 20% or below, amber through 50%, and green above 50%.

**Device → Display & Privacy → Censor display** is a saved, off-by-default content-creation setting. When enabled, on-screen SSIDs and other shown identifiers keep their first three characters, MAC addresses keep their first four hex digits, and the rest become `X`; very short names are also masked. Coordinate fractional digits become `X`. The masking covers Wi-Fi and Pineapple lists, frame feeds, Fox hunt, Remote ID details, adapter and radio addresses, and target capture filenames shown in the app. It changes display text only: monitor collection and PCAP file contents retain their original bytes. Check the setting before recording or sharing a screen.

**Device → Radio Setup** lists WiFi, Bluetooth and GPS devices. Browse with F/X, toggle the highlighted adapter with Enter, and refresh after connecting hardware with D. Marauder remembers confirmed WiFi and Bluetooth choices per adapter and reapplies them on launch or refresh. On exit, it releases GPS and restores the original radio block and Bluetooth power settings for connected adapters it changed. The WiFi picker remembers the selected hardware identity and only selects it when detected. It remembers the enabled GPS receiver and resumes reading it on launch. Disable GPS to forget the selection. Select a USB or Cap/Grove port to read it. Marauder listens through gpsd when it already manages that port. Otherwise it checks serial data for checksum-valid NMEA. Disable GPS or exit to release the reader, restore serial settings, and return connector power to its initial state.

**Location → GPS Status** shows the selected receiver, live fix and satellite status, and coordinates. **Location → GPS Track** records positions once per second while a fix is available. **Location → Wardrive** starts collecting on entry, listens for beacon or probe-response APs through idle monitor-capable Wi-Fi adapters, hops supported channels, and records each AP once at the location where it was observed with a valid GPS fix. Location → Wardrive Settings controls channel dwell, priority channels, and whether all idle adapters or only the selected adapter take part. With multiple adapters, supported channels are divided into blocks; priority channels receive an extra visit per pass. Connected Wi-Fi and hotspot adapters are skipped, and temporary monitor sessions are restored when collection stops. It also uses an enabled Bluetooth adapter through BlueZ discovery to log nearby Bluetooth devices with address, advertised name, address type and RSSI at the first location with a valid GPS fix. It can record Bluetooth alone if no WiFi monitor adapter is available. The WiFi, Bluetooth, total, and channel counts include only distinct devices observed with a valid GPS position and successfully written to the current CSV. Detections without a fix are not counted or deduplicated, so a later observation after GPS recovers can be recorded. The **CSV** count shows the geotagged rows in that file, split into WiFi and Bluetooth. The 512-entry AP recon list does not limit wardrive counts or CSV logging. Enter stops collection; pressing it again starts a new file and resets the observed counts. Esc stops and saves. Files are CSV in `~/Documents/Marauder/gps`, with a UTC timestamp, radio, device details and precise coordinates. Bluetooth random addresses may rotate, so a device can appear more than once. The display censor setting masks on-screen names and addresses but does not alter saved GPS files. Wardrive does not upload observations.

Adapter Setup remembers monitor mode choices by stable adapter ID. At app exit, Marauder restores the original radio mode; at its next launch, it attempts to re-enable monitor mode for each remembered adapter that is present and idle. A connected adapter is left alone.

**Device → Accounts and uploads** stores a WiGLE API name and token and a WDGWars API key in separate owner-only files under the app configuration directory. Type a replacement and press Enter to save, or Esc to cancel. Del followed by Enter removes a saved field. Check WiGLE login and Check WDGWars key validate the saved credentials through each service's account endpoint and show Valid, Invalid, or Unavailable. WiGLE's profile endpoint currently returns the success value as the text `"true"`; the checker accepts that response. Uploads require an active Wi-Fi connection.

**Device → Files & Storage → Saved captures** lists PCAP files. Select one and press `Delete`, then confirm, to remove it. **Wardrive logs** in the same menu lists saved GPS-tagged CSVs with size, date, and separate WiGLE/WDGWars status. `U` uploads the selected log to WiGLE; `W` uploads it to WDGWars; `B` uploads the selected log to both; `A` uploads every pending service destination across saved logs; `Delete` removes the selected log after confirmation. Enter shows full details. Uploads show bytes sent, batch progress, and the current phase. **Sent 100%** means the file reached the service; the app may still be awaiting a server response or WDGWars job processing, with elapsed seconds shown. Upload converts the app's CSV to WiGLE 1.6 format while preserving the original. A server-confirmed result is shown as **Uploaded**. A WDGWars job that remains unconfirmed after polling is shown as **Queued**. Bulk upload skips uploaded and queued destinations and logs without devices. Older logs have no observed frequency, so their upload derives conventional 2.4/5 GHz frequency from channel and cannot disambiguate 6 GHz. New logs save observed frequency.

Display and power offers System, 25%, 50%, 75%, and 100% brightness. Idle dimming defaults to Never. The first key wakes the screen and exiting restores the entry brightness.
Files & Storage groups saved captures and wardrive logs with the screenshot destination, free-space reserve (64 MiB by default), and image cache. Screenshots check the destination volume before writing.
Accounts and uploads defaults to Manual. Select Upload policy, review the active NetworkManager WiFi profile, and press Enter again to trust it for uploads. Select the policy again to revert to Manual. Uploads remain manual in both modes; a trusted policy restricts them to that Wi-Fi profile.

The GPS badge shows Off, Wait, No data, Busy, or the number of satellites used in the current fix. It shows Fix until the receiver supplies a count. A valid sentence can confirm a receiver even before a satellite fix. USB serial bridges appear as candidates until receiver data is recognized.

Open **WiFi → Setup → WiFi Adapter** to choose the WiFi interface used by the app. The border follows keyboard focus, and an asterisk marks the interface selected for use. Press `Enter` to select the highlighted adapter or `D` to inspect its details.

Under **WiFi > Discover > Access Points**, the raw-frame inventory scrolls as APs arrive. Enter opens AP operations: Information, Observed clients, and Traffic filter. Each observed client has Information and Traffic filter operations. A traffic filter pins the selected AP's channel and shows matching MAC-header frames live with frame type, sender, receiver, RSSI when present, channel, and a cumulative frame count. The AP's ESSID replaces its BSSID in individual frame rows when known; hidden or unresolved names retain the MAC address. F/X scrolls older frames and back to the live tail. The feed retains at most 128 recent frame summaries, not payload bytes. In Adapters, M switches the selected idle adapter into monitor mode. M again or exiting the app releases it. **WiFi > Monitor > Traffic Graph** plots the selected adapter’s received and transmitted packets per second over the last 30 samples. Under **WiFi > Setup > Traffic Source**, choose Monitor (default; requires monitor mode and tunes to a supported channel) or Adapter (interface counters without monitor mode). In Monitor mode, Z/C tunes the radio to another enabled channel; driver-reported busy airtime appears alongside packet rate when available. Choosing an adapter with a live Wi-Fi connection, a hosted hotspot, or an unknown connection state prompts for confirmation. Adapter details show monitor capability and current monitor status.

The package installs a restricted sudo rule for local users in the `sudo` group. It permits only `/usr/lib/marauder/monitor-session` and `/usr/lib/marauder/ble-hci-monitor` without a password, so Wi-Fi mode switching and passive Bluetooth capture need no separate enrollment. The HCI helper drops root privileges immediately after opening the read-only monitor socket. The helper accepts a wireless interface name, checks that the adapter is idle, and restores its mode when the app closes its session. Package removal removes the rule.

The bottom strip shows the current screen’s shortcuts. System Info is canonically located at **Device > System Info** and exposes Summary, System, Interfaces, USB/Peripherals, and Tools pages. On Adapters and System Info, `D` toggles details/summary. On System Info, `F`/`X` changes page, `C`/`Enter` refreshes, and `Z`/`Esc` returns to Device with its selection retained.

Open **WiFi → Discover → Access Points** after selecting an adapter and enabling monitor mode. The app tunes through the adapter's supported channels and adds APs as beacon, response, or data frames arrive. Enter opens AP information, then the observed-client list, then client information. The app pins the AP's channel while its information or clients are open. Client RSSI is shown only when a frame transmitted by that client was observed. Frame counts include observed retransmissions and are not an association guarantee. Leaving the screen closes capture; exiting also restores the adapter through the monitor session.

On AP information, press **D** to cycle through overview, authentication, and encryption pages. The security interpreter reads the AP's beacon or probe-response advertisements. It identifies legacy WPA, RSN AKM selectors, pairwise/group/management ciphers, PMF capable/required bits, SAE H2E and SAE-PK flags, WPS IE presence, and malformed or unknown selectors. Common summaries include WPA2 Personal/Enterprise, WPA3 Personal SAE, WPA2/WPA3 Personal transition, OWE, DPP, and WPA3 Enterprise 192 when its advertised cipher and PMF profile matches. A privacy bit without a valid WPA/RSN element is reported as uncertain legacy privacy, not proven WEP. These labels describe advertised capabilities, not a client's negotiated security, Wi-Fi Alliance certification, or resistance to a particular attack. Selector mappings follow [hostap's WPA definitions](https://github.com/zephyrproject-rtos/hostap/blob/main/src/common/wpa_common.h); OWE discovery follows [RFC 8110](https://datatracker.ietf.org/doc/rfc8110/).

## Offline Remote ID maps

Open **Location → Offline Maps** to see free storage and the
download and installed size of each optional world pack. Select **OSM Bright**
or **Dark Matter** and press Enter to download it; once installed, Enter selects
it for Remote ID. The **Around GPS** entry downloads dark tiles for a 3 km
radius around the current GPS fix. Its size estimate changes with the zoom
level. Z/C chooses map zoom: z1–z6 for world packs or z6–z16
for nearby tiles. The scrolling list also offers regions, countries, and US
states in the Dark Matter style. These range from z1–z10; the displayed estimate
changes with zoom. Downloads require enough free storage for the estimate plus
working space. A progress bar shows completed tiles (or bytes for the starter
world packs); **Esc** pauses a download and **Enter** resumes it from saved tiles.
Regional downloads reuse complete partial files and retry only failed tiles;
the storage estimate allows about 20 KiB per tile. Large regions still contain
many thousands of requests and may take several minutes.
**Delete** opens a confirmation before removing the highlighted pack, including a
partial pack, to reclaim its storage. The packs use geographic rectangles, so
border tiles can include nearby countries. Enter saves the zoom with the selected
pack. Downloads run in the background. The
Remote ID map uses the saved style and zoom beneath aircraft paths when GPS
has a fix. With **Censor display** enabled, the app hides map tiles so the
operator's location cannot be inferred from the visible streets. The Remote ID
map now states when Censor hides tiles, when a GPS fix is needed, or when the
selected pack has no tile at the current location. To show downloaded tiles,
turn Censor off under **Device → Display & Privacy → Censor display** and use a
valid GPS fix inside the pack's coverage.

The compact world starter packs are useful for regional context. Their maximum
z6 zoom does not show local streets or buildings; **Around GPS** provides local
detail. The world packs come from
[Meshtastic's map starter kit](https://github.com/meshtastic/device-ui/tree/master/maps)
at a pinned revision. Nearby tiles come from [Oxed's self-hosted tile
service](https://download.tiles.coalition.space/faq/). Both use OpenStreetMap
data: © OpenStreetMap contributors. Tiles are stored under
`~/Documents/Marauder/maps`. The app never bulk downloads from the public
OpenStreetMap tile service. Wardriving uses the same selected pack and zoom in its
map view. Wardrive starts logging and Bluetooth discovery when opened; Enter
stops or starts a new recording. Press **M** to cycle through the live device feed, map,
and Wi-Fi AP distribution by primary channel. Use Up/Down to scroll the channel view;
its bars show unique APs and percentages, while Bluetooth is counted separately. The map
plots Wi-Fi AP and Bluetooth sightings at the Cardputer's GPS position when first
observed, plus a red route from GPS fixes recorded during the current wardrive session.
Coincident sightings share a colored ring rather than implying separate device
locations. The last recorded position keeps
the map available after GPS drops. Censor display hides both tiles and plotted
locations; the feed remains available. The on-screen route and point history is
bounded for responsiveness, while the CSV log retains the saved observations.

## Architecture

The project follows the official Cardputer Zero template:

- `src/model`: durable application state
- `src/viewmodel`: LVGL subjects and user actions
- `src/view`: screens, widgets, theme, and layout
- `src/platform`: Linux hardware and system interfaces
- `src/app`: lifecycle, display, input, and screen routing

Hardware discovery stays in the platform layer. Blocking scans and packet processing must not run on the LVGL event thread.

## UI direction

The 320×170 screen reserves a top bar for device status and a bottom strip for keyboard controls. Pastel accents identify categories and outlined tiles show keyboard focus.

## Build

Use the repository's existing CMake presets.

### Desktop simulator

Dependencies on Debian/Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build libpng-dev libjpeg-dev \
  libfmt-dev libsdl2-dev libfreetype-dev zlib1g-dev
cmake --preset linux-x86-64
cmake --build --preset linux-x86-64-dbg
./build/linux-x86-64/Debug/marauder_cardputerzero
```

### Cardputer Zero package

```sh
cmake --workflow --preset cp0-cross-package
```

The expected artifact is `dist/Marauder_0.6.72_rev1_arm64.deb`. Debian dependencies install the runtime tools needed by the WiFi, Bluetooth, GPS, monitor, and optional map pack downloads. Packaging and deployment remain separate actions.

## Safety and scope

Use only on systems and radio environments you are authorized to test. Initial diagnostic features are read-only. Later radio features must be capability-gated, reversible, and explicit about the selected interface and resulting interface state.

## License

This project is licensed under AGPL-3.0. Portions retained from the official Cardputer Zero template remain under their original MIT notices.
