// SPDX-License-Identifier: AGPL-3.0-only
#include "radio_power.h"
#include "wireless_adapters.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>

#if defined(__linux__) && !USE_DESKTOP
#include <cerrno>
#include <cstring>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace platform {
MonitorSession::~MonitorSession() { stop(); }

std::string MonitorSession::start(const std::string& name) {
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
    if (line != "Monitor on") stop();
    return line.empty() ? "Monitor permission unavailable" : line;
#else
    return "Device control unavailable";
#endif
}

std::string MonitorSession::tune(unsigned mhz) {
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
        }
    }
#endif
    return state;
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
