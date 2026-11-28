// SPDX-License-Identifier: AGPL-3.0-only
// Read-only HCI monitor tap. The only output is LE advertisement reports for one controller.
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <grp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
struct HciAddress { sa_family_t family; uint16_t device; uint16_t channel; };
constexpr int kAfBluetooth = 31, kProtoHci = 1, kMonitorChannel = 2;
constexpr uint16_t kAnyController = 0xffff, kMonitorEvent = 0x0003;

uint16_t le16(const uint8_t* bytes) { return uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8); }

void emit(const uint8_t* address, uint8_t address_type, int8_t rssi,
          const uint8_t* data, size_t length) {
    if (length > 255) return;
    char line[600];
    int used = std::snprintf(line, sizeof(line), "raw\t%02X:%02X:%02X:%02X:%02X:%02X\t%u\t%d\t",
                             address[5], address[4], address[3], address[2], address[1], address[0],
                             address_type, rssi);
    if (used < 0 || static_cast<size_t>(used) + length * 2 + 2 > sizeof(line)) return;
    static constexpr char digits[] = "0123456789ABCDEF";
    for (size_t i = 0; i < length; ++i) {
        line[used++] = digits[data[i] >> 4];
        line[used++] = digits[data[i] & 15];
    }
    line[used++] = '\n';
    // The parent drains this pipe every 100 ms; write a complete bounded record.
    size_t offset = 0;
    while (offset < static_cast<size_t>(used)) {
        const auto n = write(STDOUT_FILENO, line + offset, used - offset);
        if (n > 0) offset += static_cast<size_t>(n);
        else if (n < 0 && errno == EINTR) continue;
        else return;
    }
}

void reports(const uint8_t* event, size_t length) {
    if (length < 4 || event[0] != 0x3e || static_cast<size_t>(event[1]) + 2 != length) return;
    const uint8_t* data = event + 2;
    const size_t size = length - 2;
    const uint8_t subevent = data[0];
    if (subevent != 0x02 && subevent != 0x0d) return;
    const unsigned count = data[1];
    size_t offset = 2;
    for (unsigned i = 0; i < count; ++i) {
        if (subevent == 0x02) {
            if (offset + 9 > size) return;
            const uint8_t type = data[offset + 1];
            const uint8_t* address = data + offset + 2;
            const size_t bytes = data[offset + 8];
            if (offset + 9 + bytes + 1 > size) return;
            emit(address, type, static_cast<int8_t>(data[offset + 9 + bytes]),
                 data + offset + 9, bytes);
            offset += 10 + bytes;
        } else {
            if (offset + 24 > size) return;
            const uint8_t type = data[offset + 2];
            const uint8_t* address = data + offset + 3;
            const int8_t rssi = static_cast<int8_t>(data[offset + 13]);
            const size_t bytes = data[offset + 23];
            if (offset + 24 + bytes > size) return;
            emit(address, type, rssi, data + offset + 24, bytes);
            offset += 24 + bytes;
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::strncmp(argv[1], "hci", 3) != 0 || !argv[1][3]) return 2;
    unsigned controller = 0;
    for (const char* p = argv[1] + 3; *p; ++p) {
        if (*p < '0' || *p > '9') return 2;
        controller = controller * 10 + unsigned(*p - '0');
        if (controller > 255) return 2;
    }
    char path[64];
    std::snprintf(path, sizeof(path), "/sys/class/bluetooth/hci%u", controller);
    struct stat info{};
    if (stat(path, &info) != 0 || !S_ISDIR(info.st_mode)) return 2;
    const int monitor = socket(kAfBluetooth, SOCK_RAW | SOCK_CLOEXEC, kProtoHci);
    if (monitor < 0) return 3;
    HciAddress address{static_cast<sa_family_t>(kAfBluetooth), kAnyController, kMonitorChannel};
    if (bind(monitor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(monitor);
        return 3;
    }
    if (setgroups(0, nullptr) != 0 || setgid(65534) != 0 || setuid(65534) != 0) {
        close(monitor);
        return 3;
    }
    if (write(STDOUT_FILENO, "status\tReady\n", 13) != 13) { close(monitor); return 4; }
    std::array<uint8_t, 4096> packet{};
    pollfd descriptors[2]{{STDIN_FILENO, POLLIN | POLLHUP, 0}, {monitor, POLLIN, 0}};
    while (poll(descriptors, 2, 1000) >= 0) {
        if (descriptors[0].revents & (POLLHUP | POLLERR | POLLNVAL)) break;
        if (descriptors[0].revents & POLLIN) {
            char discard[8];
            if (read(STDIN_FILENO, discard, sizeof(discard)) <= 0) break;
        }
        if (!(descriptors[1].revents & POLLIN)) continue;
        const auto size = recv(monitor, packet.data(), packet.size(), 0);
        if (size < 6) continue;
        if (le16(packet.data()) != kMonitorEvent || le16(packet.data() + 2) != controller ||
            static_cast<size_t>(size) != 6u + le16(packet.data() + 4)) continue;
        reports(packet.data() + 6, static_cast<size_t>(size) - 6);
    }
    close(monitor);
    return 0;
}
