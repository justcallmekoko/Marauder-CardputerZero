# Marauder for Cardputer Zero

Native C++17/LVGL 9.5 port of ESP32 Marauder for the ARM64 Linux-based Cardputer Zero.

> Status: early development. The current milestone contains the application scaffold and read-only system diagnostics. Wi-Fi, Bluetooth, LoRa, GNSS logging, and packet operations are not implemented yet.

## Current milestone

- Cardputer Zero APPLaunch integration and Debian packaging scaffold
- 320x170 LVGL interface with keyboard-only navigation
- Dashboard, wireless-adapter selection, and scrollable diagnostics screens
- Read-only wireless adapter catalog with interface, driver, PHY, bus, VID/PID, MAC, stable identity, and explicit monitor/injection verification state
- Keyboard-driven adapter browsing and in-app selection without changing interface state
- Read-only inventory of:
  - operating system, kernel, uptime, memory, and storage
  - network interfaces, Linux drivers, state, and wireless classification
  - USB VID/PID and product names
  - serial, I2C, SPI, and GPIO device nodes
  - availability of radio and diagnostic command-line tools
  - gpsd socket presence
- Light and dark themes
- No background service and no permanent root process

## Hardware contract under evaluation

The first validation platform currently includes:

- Cardputer Zero (Raspberry Pi Compute Module 0)
- CardputerZero CAP LoRa1262/GNSS module on the GPIO expansion
- Alfa AWUS036ACM USB Wi-Fi adapter (MediaTek MT7612U / `mt76x2u`)

The adapter layer enumerates Linux wireless interfaces, correlates USB metadata, identifies backing drivers and PHYs, and lets the user choose an adapter without changing radio state. Monitor mode remains `UNKNOWN` until a read-only nl80211 capability query is available, and injection remains `UNVERIFIED` until a later safe, reversible runtime validation; a USB device name or driver alone is not proof of injection support.

## Architecture

The project follows the official Cardputer Zero template:

- `src/model`: durable application state
- `src/viewmodel`: LVGL subjects and user actions
- `src/view`: screens, widgets, theme, and layout
- `src/platform`: Linux hardware and system interfaces
- `src/app`: lifecycle, display, input, and screen routing

Hardware discovery stays in the platform layer. Blocking scans and packet processing must not run on the LVGL event thread.

## UI direction

The Cardputer Zero application should maintain a similar look and feel to the Cardputer ADV version of ESP32Marauder. Preserve its compact information hierarchy, high-contrast Marauder visual identity, familiar terminology, and keyboard-first interaction patterns where practical, while adapting layouts to Linux/LVGL and the Cardputer Zero display rather than forcing a pixel-for-pixel copy.

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

The expected artifact is `dist/Marauder_0.2.0_rev1_arm64.deb`. Packaging and deployment remain separate actions.

## Safety and scope

Use only on systems and radio environments you are authorized to test. Initial diagnostic features are read-only. Later radio features must be capability-gated, reversible, and explicit about the selected interface and resulting interface state.

## License

This project is licensed under AGPL-3.0. Portions retained from the official Cardputer Zero template remain under their original MIT notices.
