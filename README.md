# Marauder for Cardputer Zero

Native C++17/LVGL 9.5 port of ESP32 Marauder for the ARM64 Linux-based Cardputer Zero.

> Status: early development. The current milestone includes read-only diagnostics, adapter selection and capability reporting, plus managed-mode Wi-Fi scanning. Bluetooth, LoRa, GNSS logging, monitor-mode capture, and packet operations are not implemented yet.

## Current milestone

- Cardputer Zero APPLaunch integration and Debian packaging scaffold
- 320x170 LVGL interface with a four-tile keyboard launcher
- Dashboard, wireless-adapter selection, and five bounded keyboard-reachable System Info pages
- Read-only wireless adapter catalog with interface, driver, PHY, bus, VID/PID, MAC, stable identity, and explicit monitor/injection verification state
- Keyboard-driven adapter browsing and in-app selection without changing interface state
- Viewport-safe summary/details paging for adapter information (`D` toggles pages)
- Read-only kernel capability probing for monitor-mode support; injection remains unverified until an explicit hardware test
- Non-blocking managed-mode Wi-Fi scanning on the selected adapter, with scrolling results and offline vendor lookup
- Direct menu navigation with the printed `Z`/`X`/`C`/`F` arrow cluster, `Enter` for the primary action, and `D` for details on supported screens
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

The menu text uses the classic 5×7 glyphs from the Cardputer firmware. Under **Device → Display and power → Battery display**, press `Enter` to cycle through icon, percentage, and both. The choice is saved per user. Battery indicators are red at 20% or below, amber through 50%, and green above 50%.

**Device → Radios and GPS → Radio setup** lists WiFi, Bluetooth and GPS devices. Browse with F/X, toggle the highlighted adapter with Enter, and refresh after connecting hardware with D. Marauder remembers confirmed WiFi and Bluetooth choices per adapter and reapplies them on launch or refresh. On exit, it releases GPS and restores the original radio block and Bluetooth power settings for connected adapters it changed. The WiFi picker remembers the selected hardware identity and only selects it when detected. It remembers the enabled GPS receiver and resumes reading it on launch. Disable GPS to forget the selection. Select a USB or Cap/Grove port to read it. Marauder listens through gpsd when it already manages that port. Otherwise it checks serial data for checksum-valid NMEA. Disable GPS or exit to release the reader, restore serial settings, and return connector power to its initial state.

**Device → Accounts and uploads** stores a WiGLE API name and token and a WDGWars API key in separate owner-only files under the app configuration directory. Type a replacement and press Enter to save, or Esc to cancel. Del followed by Enter removes a saved field.

Display and power offers System, 25%, 50%, 75%, and 100% brightness. Idle dimming defaults to Never. The first key wakes the screen and exiting restores the entry brightness.
Storage and memory groups the screenshot destination, free-space reserve (64 MiB by default), and image cache. Screenshots check the destination volume before writing.
Accounts and uploads defaults to Manual. Select Upload policy, review the active NetworkManager WiFi profile, and press Enter again to trust it for future automatic uploads. Select the policy again to revert to Manual. This version stores the policy locally and has no uploader or survey logger.

The GPS badge shows Off, Wait, No data, Busy, or the number of satellites used in the current fix. It shows Fix until the receiver supplies a count. A valid sentence can confirm a receiver even before a satellite fix. USB serial bridges appear as candidates until receiver data is recognized.

Open **WiFi → General → Adapter Setup** to choose the WiFi interface used by the app. The border follows keyboard focus, and an asterisk marks the interface selected for use. Press `Enter` to select the highlighted adapter or `D` to inspect its details.

Under **WiFi > Networks > Access Points**, the list refreshes every 20 seconds while browsing. Use F/X to highlight a network, Enter to inspect its managed-scan details, Esc to return, and D to rescan. The C++ collector speaks to the local Wi-Fi service directly and parses its discovery records. **Hotspot stations** lists clients of the selected adapter when it hosts an AP. Both inventories scroll one row at a time. **WiFi > Activity > Traffic graph** plots the selected adapter’s received and transmitted packets per second over the last 30 samples. The graph uses interface counters and scales to the visible peak. On drivers that report channel surveys, Z/C selects a frequency and plots changes in busy airtime. Sampling follows the driver’s channel visits. Choosing an adapter with a live Wi-Fi connection, a hosted hotspot, or an unknown connection state prompts for confirmation. Adapter details show monitor capability and current monitor status.

The bottom strip shows the current screen’s shortcuts. System Info is canonically located at **Device > System Info** and exposes Summary, System, Interfaces, USB/Peripherals, and Tools pages. On Adapters and System Info, `D` toggles details/summary. On System Info, `F`/`X` changes page, `C`/`Enter` refreshes, and `Z`/`Esc` returns to Device with its selection retained.

Managed-mode scans use the target’s existing NetworkManager service when available; the Cardputer Zero base image has been validated with it. Scans do not enable monitor mode and do not test injection.

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

The expected artifact is `dist/Marauder_0.6.18_rev1_arm64.deb`. Packaging and deployment remain separate actions.

## Safety and scope

Use only on systems and radio environments you are authorized to test. Initial diagnostic features are read-only. Later radio features must be capability-gated, reversible, and explicit about the selected interface and resulting interface state.

## License

This project is licensed under AGPL-3.0. Portions retained from the official Cardputer Zero template remain under their original MIT notices.
