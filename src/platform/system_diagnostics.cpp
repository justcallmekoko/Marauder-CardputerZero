/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "system_diagnostics.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

#include <sys/statvfs.h>
#include <sys/utsname.h>

namespace platform::diagnostics {
namespace {

namespace fs = std::filesystem;

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

bool read_first_line(const fs::path& path, std::string& value) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::getline(file, value);
    value = trim(value);
    return true;
}

std::string read_or(const fs::path& path, const char* fallback) {
    std::string value;
    return read_first_line(path, value) && !value.empty() ? value : fallback;
}

std::string unquote(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string read_os_name() {
    std::ifstream file("/etc/os-release");
    std::string line;
    while (std::getline(file, line)) {
        constexpr const char* prefix = "PRETTY_NAME=";
        if (line.rfind(prefix, 0) == 0) {
            return unquote(line.substr(std::char_traits<char>::length(prefix)));
        }
    }
    return "Unknown Linux";
}

std::string format_bytes(uint64_t bytes) {
    static constexpr std::array<const char*, 5> units = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(unit > 1 ? 1 : 0) << value << ' ' << units[unit];
    return out.str();
}

std::string read_memory() {
    std::ifstream file("/proc/meminfo");
    std::string key;
    uint64_t value = 0;
    std::string unit;
    uint64_t total_kib = 0;
    uint64_t available_kib = 0;
    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") total_kib = value;
        if (key == "MemAvailable:") available_kib = value;
    }
    if (!total_kib) return "Unknown";
    return format_bytes(available_kib * 1024) + " free / " + format_bytes(total_kib * 1024);
}

std::string read_storage() {
    struct statvfs info {};
    if (statvfs("/", &info) != 0) return "Unknown";
    const uint64_t total = static_cast<uint64_t>(info.f_blocks) * info.f_frsize;
    const uint64_t available = static_cast<uint64_t>(info.f_bavail) * info.f_frsize;
    return format_bytes(available) + " free / " + format_bytes(total);
}

std::string read_uptime() {
    std::ifstream file("/proc/uptime");
    double seconds = 0;
    if (!(file >> seconds)) return "Unknown";
    const auto minutes = static_cast<uint64_t>(seconds / 60.0);
    const auto days = minutes / (24 * 60);
    const auto hours = (minutes / 60) % 24;
    const auto mins = minutes % 60;
    std::ostringstream out;
    if (days) out << days << "d ";
    out << hours << "h " << mins << "m";
    return out.str();
}

std::string kernel_version() {
    struct utsname value {};
    if (uname(&value) != 0) return "Unknown";
    return std::string(value.release) + " " + value.machine;
}

std::string driver_name(const fs::path& interface_path) {
    std::error_code ec;
    const auto target = fs::canonical(interface_path / "device/driver", ec);
    return ec ? "unknown" : target.filename().string();
}

std::vector<NetworkInterface> read_interfaces() {
    std::vector<NetworkInterface> result;
    std::error_code ec;
    const fs::path root("/sys/class/net");
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (ec) break;
        const auto name = entry.path().filename().string();
        if (name == "lo") continue;
        result.push_back({name,
                          driver_name(entry.path()),
                          read_or(entry.path() / "operstate", "unknown"),
                          fs::exists(entry.path() / "wireless", ec)});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return result;
}

std::vector<UsbDevice> read_usb_devices() {
    std::vector<UsbDevice> result;
    std::error_code ec;
    const fs::path root("/sys/bus/usb/devices");
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (ec) break;
        std::string vendor;
        std::string product;
        if (!read_first_line(entry.path() / "idVendor", vendor) ||
            !read_first_line(entry.path() / "idProduct", product)) {
            continue;
        }
        std::string manufacturer = read_or(entry.path() / "manufacturer", "");
        std::string product_name = read_or(entry.path() / "product", "USB device");
        std::string name = manufacturer.empty() ? product_name : manufacturer + " " + product_name;
        result.push_back({vendor, product, trim(name)});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return result;
}

bool tool_available(const std::string& name) {
    static constexpr std::array<const char*, 7> roots = {
        "/usr/local/sbin", "/usr/local/bin", "/usr/sbin", "/usr/bin", "/sbin", "/bin", "/opt/bin"};
    std::error_code ec;
    for (const auto* root : roots) {
        const auto candidate = fs::path(root) / name;
        const auto permissions = fs::status(candidate, ec).permissions();
        if (!ec && fs::exists(candidate) && permissions != fs::perms::unknown) return true;
        ec.clear();
    }
    return false;
}

std::vector<std::string> read_peripheral_nodes() {
    std::vector<std::string> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/dev", ec)) {
        if (ec) break;
        const auto name = entry.path().filename().string();
        const bool match = name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0 ||
                           name.rfind("ttyAMA", 0) == 0 || name.rfind("ttyS", 0) == 0 ||
                           name.rfind("spidev", 0) == 0 || name.rfind("i2c-", 0) == 0 ||
                           name.rfind("gpiochip", 0) == 0;
        if (match) result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::string join(const std::vector<std::string>& values, const char* separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) out << separator;
        out << values[i];
    }
    return out.str();
}

} // namespace

Snapshot collect() {
    Snapshot snapshot;
    snapshot.model = read_or("/proc/device-tree/model", "Development host");
    snapshot.operating_system = read_os_name();
    snapshot.kernel = kernel_version();
    snapshot.uptime = read_uptime();
    snapshot.memory = read_memory();
    snapshot.storage = read_storage();
    snapshot.network_interfaces = read_interfaces();
    snapshot.usb_devices = read_usb_devices();
    snapshot.peripheral_nodes = read_peripheral_nodes();
    snapshot.gpsd_socket = fs::exists("/run/gpsd.sock") || fs::exists("/var/run/gpsd.sock");
    for (const auto* tool : {"iw", "nmcli", "tcpdump", "aircrack-ng", "gpsctl", "bluetoothctl"}) {
        snapshot.tools.push_back({tool, tool_available(tool)});
    }
    return snapshot;
}

std::string dashboard_summary(const Snapshot& snapshot) {
    size_t wireless = 0;
    for (const auto& interface : snapshot.network_interfaces) wireless += interface.wireless ? 1 : 0;
    std::ostringstream out;
    out << snapshot.model << '\n'
        << "Memory: " << snapshot.memory << '\n'
        << "Storage: " << snapshot.storage << '\n'
        << "Adapters: " << wireless << " wireless, " << snapshot.usb_devices.size() << " USB";
    return out.str();
}

std::string detail_text(const Snapshot& snapshot) {
    std::ostringstream out;
    out << "SYSTEM\n" << snapshot.operating_system << '\n' << snapshot.kernel
        << "\nUptime: " << snapshot.uptime << "\n\nINTERFACES";
    for (const auto& interface : snapshot.network_interfaces) {
        out << '\n' << interface.name << "  " << interface.driver << "  " << interface.state;
        if (interface.wireless) out << "  wireless";
    }
    out << "\n\nUSB";
    for (const auto& device : snapshot.usb_devices) {
        out << '\n' << device.vendor_id << ':' << device.product_id << ' ' << device.name;
    }
    out << "\n\nPERIPHERALS\n" << (snapshot.peripheral_nodes.empty() ? "None detected" : join(snapshot.peripheral_nodes, ", "));
    out << "\nGPSD socket: " << (snapshot.gpsd_socket ? "ready" : "not detected");
    out << "\n\nTOOLS";
    for (const auto& tool : snapshot.tools) {
        out << '\n' << (tool.available ? "[OK] " : "[--] ") << tool.name;
    }
    return out.str();
}

} // namespace platform::diagnostics
