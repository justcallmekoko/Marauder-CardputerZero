// SPDX-License-Identifier: AGPL-3.0-only
#include "radio_power.h"
#include "wireless_adapters.h"
#include "ble_classify.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#if defined(__linux__) && !USE_DESKTOP
#include <cerrno>
#include <cstring>
#include <spawn.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace platform {
MonitorSession::~MonitorSession() { stop(); }

std::string MonitorSession::start(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(__linux__) && !USE_DESKTOP
    int ends[2];
    if (channel_ >= 0 || socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ends) != 0)
        return "Monitor session unavailable";
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    if (!err) {
        err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDIN_FILENO);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO);
        char sudo[] = "/usr/bin/sudo", noninteractive[] = "-n", helper[] = "/usr/lib/marauder/monitor-session";
        char* args[] = {sudo, noninteractive, helper, const_cast<char*>(name.c_str()), nullptr};
        if (!err) err = posix_spawn(&child_, sudo, &actions, nullptr, args, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(ends[1]);
    if (err) { close(ends[0]); return "Cannot open monitor helper"; }
    channel_ = ends[0];
    std::string line;
    char c;
    while (line.size() < 128) {
        auto n = recv(channel_, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n != 1 || c == '\n') break;
        line += c;
    }
    if (line != "Monitor on") {
        shutdown(channel_, SHUT_WR);
        int status{};
        waitpid(child_, &status, 0);
        close(channel_);
        channel_ = child_ = -1;
    }
    return line.empty() ? "Monitor permission unavailable" : line;
#else
    return "Device control unavailable";
#endif
}

std::string MonitorSession::tune(unsigned mhz) {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(__linux__) && !USE_DESKTOP
    const auto command = std::to_string(mhz) + "\n";
    if (channel_ < 0 || send(channel_, command.data(), command.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(command.size()))
        return "Monitor session unavailable";
    std::string reply;
    char c;
    while (reply.size() < 64) {
        const auto n = recv(channel_, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n != 1) return "Monitor helper disconnected";
        if (c == '\n') return reply;
        reply += c;
    }
#endif
    return "Channel tune unavailable";
}

int MonitorSession::capture_socket() {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(__linux__) && !USE_DESKTOP
    constexpr char command[] = "CAPTURE\n";
    if (channel_ < 0 || send(channel_, command, sizeof(command) - 1, MSG_NOSIGNAL) != static_cast<ssize_t>(sizeof(command) - 1))
        return -1;
    char reply[64]{};
    char control[CMSG_SPACE(sizeof(int))]{};
    iovec payload{reply, sizeof(reply)};
    msghdr message{};
    message.msg_iov = &payload;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);
    const auto count = recvmsg(channel_, &message, MSG_CMSG_CLOEXEC);
    if (count < 3 || std::strncmp(reply, "FD\n", 3) != 0) return -1;
    for (auto* item = CMSG_FIRSTHDR(&message); item; item = CMSG_NXTHDR(&message, item)) {
        if (item->cmsg_level == SOL_SOCKET && item->cmsg_type == SCM_RIGHTS &&
            item->cmsg_len >= CMSG_LEN(sizeof(int))) {
            int fd;
            std::memcpy(&fd, CMSG_DATA(item), sizeof(fd));
            return fd;
        }
    }
#endif
    return -1;
}

std::string MonitorSession::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(__linux__) && !USE_DESKTOP
    if (channel_ >= 0) {
        shutdown(channel_, SHUT_WR);
        int status{};
        int rc;
        do { rc = waitpid(child_, &status, 0); } while (rc < 0 && errno == EINTR);
        close(channel_);
        channel_ = child_ = -1;
        if (rc < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return "Check adapter restore";
    }
#endif
    return "Monitor off";
}

GpsSession::~GpsSession() { stop(); }

void GpsSession::stop() {
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_WR);
        if (child_ > 0) {
            while (waitpid(child_, nullptr, 0) < 0 && errno == EINTR) {}
        }
        close(socket_);
    }
#endif
    child_ = socket_ = -1;
    pending_.clear();
    position_valid_ = false;
}

bool GpsSession::start(const std::string& id) {
    stop();
#if defined(__linux__) && !USE_DESKTOP
    int ends[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ends) != 0) return false;
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    if (!err) {
        err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDIN_FILENO);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO);
        char program[] = "/usr/lib/marauder/gps-session";
        char* args[] = {program, const_cast<char*>(id.c_str()), nullptr};
        if (!err) err = posix_spawn(&child_, program, &actions, nullptr, args, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(ends[1]);
    if (!err) { socket_ = ends[0]; return true; }
    close(ends[0]);
#endif
    return false;
}

std::string GpsSession::poll() {
    std::string state;
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ < 0) return {};
    char buffer[256];
    for (;;) {
        auto n = recv(socket_, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { stop(); return "Unavailable"; }
        pending_.append(buffer, n);
        std::size_t end;
        while ((end = pending_.find('\n')) != std::string::npos) {
            state = pending_.substr(0, end);
            pending_.erase(0, end + 1);
            const auto first = state.find('\t');
            if (first != std::string::npos) {
                const auto second = state.find('\t', first + 1);
                if (second != std::string::npos) {
                    try {
                        const auto lat = std::stoll(state.substr(first + 1, second - first - 1));
                        const auto lon = std::stoll(state.substr(second + 1));
                        if (lat >= -900000000 && lat <= 900000000 &&
                            lon >= -1800000000 && lon <= 1800000000 && (lat || lon)) {
                            latitude_e7_ = static_cast<int32_t>(lat);
                            longitude_e7_ = static_cast<int32_t>(lon);
                            position_time_ = std::chrono::steady_clock::now();
                            position_valid_ = true;
                        }
                    } catch (const std::exception&) { position_valid_ = false; }
                }
                state.resize(first);
            } else if (state == "No fix" || state == "No data" || state == "Wait" ||
                       state == "Unavailable" || state == "Missing" || state == "Busy") {
                position_valid_ = false;
            }
        }
    }
#endif
    return state;
}
bool GpsSession::position(int32_t& latitude_e7, int32_t& longitude_e7) const {
    if (!position_valid_ || std::chrono::steady_clock::now() - position_time_ > std::chrono::seconds(10))
        return false;
    latitude_e7 = latitude_e7_;
    longitude_e7 = longitude_e7_;
    return true;
}

BluetoothSession::~BluetoothSession() { stop(); }
void BluetoothSession::stop() {
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_WR);
        if (child_ > 0) {
            int status{};
            bool exited = false;
            for (int attempt = 0; attempt < 20; ++attempt) {
                const auto result = waitpid(child_, &status, WNOHANG);
                if (result == child_ || (result < 0 && errno == ECHILD)) { exited = true; break; }
                if (result < 0 && errno != EINTR) break;
                usleep(100000);
            }
            if (!exited) {
                kill(child_, SIGTERM);
                while (waitpid(child_, &status, 0) < 0 && errno == EINTR) {}
            }
        }
        close(socket_);
    }
#endif
    child_ = socket_ = -1;
    pending_.clear();
}
bool BluetoothSession::start(const std::string& adapter, bool le_only) {
    stop();
    if (adapter.size() < 4 || adapter.size() > 6 || adapter.compare(0, 3, "hci") != 0 ||
        !std::all_of(adapter.begin() + 3, adapter.end(),
                     [](unsigned char c) { return c >= '0' && c <= '9'; }))
        return false;
#if defined(__linux__) && !USE_DESKTOP
    int ends[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ends) != 0) return false;
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    if (!err) {
        err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDIN_FILENO);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO);
        char program[] = "/usr/lib/marauder/bluetooth-session";
        char le[] = "le";
        char* args[] = {program, const_cast<char*>(adapter.c_str()), le_only ? le : nullptr, nullptr};
        if (!err) err = posix_spawn(&child_, program, &actions, nullptr, args, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(ends[1]);
    if (!err) { socket_ = ends[0]; return true; }
    close(ends[0]);
#endif
    return false;
}
std::vector<BluetoothObservation> BluetoothSession::poll(std::string& status) {
    std::vector<BluetoothObservation> found;
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ < 0) return found;
    char buffer[1024];
    for (int reads = 0; reads < 8; ++reads) {
        const auto count = recv(socket_, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            if (status.empty()) status = "Bluetooth scan ended";
            stop();
            break;
        }
        pending_.append(buffer, static_cast<size_t>(count));
        if (pending_.size() > 8192) { status = "Bluetooth scan overflow"; stop(); break; }
        std::size_t end;
        while ((end = pending_.find('\n')) != std::string::npos) {
            const auto line = pending_.substr(0, end);
            pending_.erase(0, end + 1);
            if (line.compare(0, 7, "status\t") == 0) { status = line.substr(7); continue; }
            if (line.compare(0, 7, "device\t") != 0) continue;
            const auto a = line.find('\t', 7);
            if (a == std::string::npos) continue;
            const auto b = line.find('\t', a + 1);
            if (b == std::string::npos) continue;
            const auto c = line.find('\t', b + 1);
            if (c == std::string::npos) continue;
            const auto d = line.find('\t', c + 1);
            BluetoothObservation item{line.substr(7, a - 7), line.substr(a + 1, b - a - 1),
                                      line.substr(c + 1, d == std::string::npos ? d : d - c - 1)};
            if (d != std::string::npos) {
                const auto e = line.find('\t', d + 1);
                const auto f = e == std::string::npos ? e : line.find('\t', e + 1);
                const auto g = f == std::string::npos ? f : line.find('\t', f + 1);
                if (g == std::string::npos) continue;
                item.manufacturer_data = line.substr(d + 1, e - d - 1);
                item.service_data = line.substr(e + 1, f - e - 1);
                item.advertising_data = line.substr(f + 1, g - f - 1);
                item.uuids = line.substr(g + 1);
            }
            try { item.rssi = std::stoi(line.substr(b + 1, c - b - 1)); }
            catch (const std::exception&) { continue; }
            if (item.address.size() != 17 || item.rssi < -127 || item.rssi > 20) continue;
            bool valid = true;
            for (std::size_t i = 0; i < item.address.size(); ++i)
                valid &= i % 3 == 2 ? item.address[i] == ':' :
                    std::isxdigit(static_cast<unsigned char>(item.address[i])) != 0;
            if (valid) found.push_back(std::move(item));
        }
    }
#endif
    return found;
}

BluetoothHciSession::~BluetoothHciSession() { stop(); }
void BluetoothHciSession::stop() {
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_WR);
        if (child_ > 0) {
            int status{};
            bool exited = false;
            for (int attempt = 0; attempt < 20; ++attempt) {
                const auto result = waitpid(child_, &status, WNOHANG);
                if (result == child_ || (result < 0 && errno == ECHILD)) { exited = true; break; }
                if (result < 0 && errno != EINTR) break;
                usleep(50000);
            }
            if (!exited) {
                kill(child_, SIGTERM);
                while (waitpid(child_, &status, 0) < 0 && errno == EINTR) {}
            }
        }
        close(socket_);
    }
#endif
    socket_ = child_ = -1;
    pending_.clear();
}
bool BluetoothHciSession::start(const std::string& adapter) {
    stop();
    if (adapter.size() < 4 || adapter.size() > 6 || adapter.compare(0, 3, "hci") != 0 ||
        !std::all_of(adapter.begin() + 3, adapter.end(),
                     [](unsigned char c) { return c >= '0' && c <= '9'; })) return false;
#if defined(__linux__) && !USE_DESKTOP
    int ends[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ends) != 0) return false;
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    if (!err) {
        err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDIN_FILENO);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO);
        char sudo[] = "/usr/bin/sudo", no_prompt[] = "-n", helper[] = "/usr/lib/marauder/ble-hci-monitor";
        char* args[] = {sudo, no_prompt, helper, const_cast<char*>(adapter.c_str()), nullptr};
        if (!err) err = posix_spawn(&child_, sudo, &actions, nullptr, args, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(ends[1]);
    if (err) { close(ends[0]); return false; }
    socket_ = ends[0];
    pollfd descriptor{socket_, POLLIN, 0};
    if (::poll(&descriptor, 1, 2000) <= 0) { stop(); return false; }
    std::string line;
    char c;
    while (line.size() < 64) {
        const auto n = recv(socket_, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n != 1 || c == '\n') break;
        line += c;
    }
    if (line == "status\tReady") return true;
    stop();
#endif
    return false;
}
std::vector<BluetoothObservation> BluetoothHciSession::poll(std::string& status) {
    std::vector<BluetoothObservation> found;
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ < 0) return found;
    char buffer[1024];
    for (int reads = 0; reads < 16; ++reads) {
        const auto count = recv(socket_, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { status = "Raw Bluetooth capture ended"; stop(); break; }
        pending_.append(buffer, static_cast<size_t>(count));
        if (pending_.size() > 32768) { status = "Raw Bluetooth capture overflow"; stop(); break; }
        size_t end;
        while ((end = pending_.find('\n')) != std::string::npos) {
            const auto line = pending_.substr(0, end);
            pending_.erase(0, end + 1);
            if (line.compare(0, 4, "raw\t") != 0) continue;
            const auto a = line.find('\t', 4);
            const auto b = a == std::string::npos ? a : line.find('\t', a + 1);
            const auto c = b == std::string::npos ? b : line.find('\t', b + 1);
            if (c == std::string::npos) continue;
            const auto mac = line.substr(4, a - 4);
            if (mac.size() != 17) continue;
            int type{}, rssi{};
            try {
                type = std::stoi(line.substr(a + 1, b - a - 1));
                rssi = std::stoi(line.substr(b + 1, c - b - 1));
            } catch (const std::exception&) { continue; }
            if (type < 0 || type > 3 || rssi < -128 || rssi > 127) continue;
            const auto kind = type == 0 ? "public" : type == 1 ? "random" :
                              type == 2 ? "public identity" : "random identity";
            found.push_back(ble::decode_report(mac, kind, rssi, line.substr(c + 1)));
        }
    }
#endif
    return found;
}

std::string begin_radio_session() {
#if defined(__linux__) && !USE_DESKTOP
    char directory[] = "/tmp/marauder-radios-XXXXXX";
    if (mkdtemp(directory)) return directory;
#endif
    return {};
}

void end_radio_session(const std::string& directory) {
#if defined(__linux__) && !USE_DESKTOP
    if (directory.empty()) return;
    const auto cmd = "/usr/bin/timeout 25 /usr/lib/marauder/radio-inventory --session " + directory + " --release";
    if (std::system(cmd.c_str()) != 0)
        std::fputs("Marauder: radio cleanup incomplete\n", stderr);
    rmdir(directory.c_str());
#endif
}

RadioInventory read_radios(std::string id, bool enable, const std::string& directory) {
#if !defined(__linux__) || USE_DESKTOP
    return {"Device radios unavailable", {}};
#else
    std::string command = "/usr/bin/timeout 20 /usr/bin/python3 -I /usr/lib/marauder/radio-inventory";
    if (directory.empty()) return {"Radio session unavailable", {}};
    command += " --session " + directory;
    if (id.empty()) command += " --restore";
    else {
        if (id.size() != 64 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
            return {"Invalid device", {}};
        command += " " + id + (enable ? " on" : " off");
    }
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {"Inventory unavailable", {}};
    RadioInventory result{"Inventory unavailable", {}};
    char buffer[2048];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        line.erase(line.find_last_not_of("\r\n") + 1);
        std::istringstream row(line);
        std::vector<std::string> fields;
        for (std::string field; std::getline(row, field, '\t');) fields.push_back(field);
        if (fields.size() == 2 && fields[0] == "status") result.message = fields[1];
        if (fields.size() == 9 && fields[0] == "radio")
            result.devices.push_back({fields[1], fields[2], fields[3], fields[4],
                                      fields[5], fields[6], fields[7], fields[8]});
    }
    if (pclose(pipe) != 0) return {"Inventory unavailable", {}};
    const auto catalog = wireless::enumerate();
    for (auto& device : result.devices) {
        if (device.kind != "WiFi") continue;
        for (const auto& adapter : catalog.adapters) {
            if (adapter.interface_name != device.name) continue;
            if (!adapter.manufacturer.empty() || !adapter.product_name.empty())
                device.product = adapter.display_name();
            device.bands = adapter.bands;
            break;
        }
    }
    return result;
#endif
}
}
