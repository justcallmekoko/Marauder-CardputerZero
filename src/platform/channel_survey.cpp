// SPDX-License-Identifier: AGPL-3.0-only
#include "channel_survey.h"
#ifdef __linux__
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <array>

namespace {
struct Socket {
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
    ~Socket() { if (fd >= 0) close(fd); }
};
template<typename T> bool read_value(const char* data, size_t size, T& value) {
    if (size != sizeof(T)) return false;
    std::memcpy(&value, data, size);
    return true;
}
template<typename Callback> void attributes(const char* p, size_t len, Callback cb) {
    while (len >= sizeof(nlattr)) {
        nlattr a;
        std::memcpy(&a, p, sizeof(a));
        if (a.nla_len < NLA_HDRLEN || a.nla_len > len) return;
        cb(a.nla_type & NLA_TYPE_MASK, p + NLA_HDRLEN, a.nla_len - NLA_HDRLEN);
        const size_t step = NLA_ALIGN(a.nla_len);
        if (step > len) return;
        len -= step;
        p += step;
    }
}
template<typename Callback>
bool query(int fd, unsigned family, unsigned cmd, unsigned attr, const void* data,
           size_t size, bool dump, Callback callback) {
    struct { nlmsghdr h; genlmsghdr g; nlattr a; char value[16]; } msg{};
    if (size > sizeof(msg.value)) return false;
    msg.h.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN + NLA_ALIGN(NLA_HDRLEN + size));
    msg.h.nlmsg_type = family;
    msg.h.nlmsg_flags = NLM_F_REQUEST | (dump ? NLM_F_DUMP : 0);
    msg.h.nlmsg_seq = cmd;
    msg.g.cmd = cmd;
    msg.g.version = 1;
    msg.a.nla_type = attr;
    msg.a.nla_len = NLA_HDRLEN + size;
    std::memcpy(msg.value, data, size);
    sockaddr_nl kernel{};
    kernel.nl_family = AF_NETLINK;
    if (sendto(fd, &msg, msg.h.nlmsg_len, 0, reinterpret_cast<sockaddr*>(&kernel), sizeof(kernel)) < 0) return false;
    alignas(nlmsghdr) std::array<char, 32768> buffer;
    for (;;) {
        auto n = recv(fd, buffer.data(), buffer.size(), MSG_TRUNC);
        if (n <= 0 || static_cast<size_t>(n) > buffer.size()) return false;
        for (auto* h = reinterpret_cast<nlmsghdr*>(buffer.data()); NLMSG_OK(h,n); h = NLMSG_NEXT(h,n)) {
            if (h->nlmsg_seq != cmd || (h->nlmsg_flags & NLM_F_DUMP_INTR)) return false;
            if (h->nlmsg_type == NLMSG_DONE) return true;
            if (h->nlmsg_type != family || h->nlmsg_len < NLMSG_LENGTH(GENL_HDRLEN)) return false;
            attributes(static_cast<const char*>(NLMSG_DATA(h)) + GENL_HDRLEN,
                       h->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN), callback);
            if (!dump) return true;
        }
    }
}
}
#endif
namespace platform::wireless {
std::vector<ChannelSample> channel_survey(const std::string& interface) {
    std::vector<ChannelSample> samples;
#ifdef __linux__
    const auto index = if_nametoindex(interface.c_str());
    Socket socket;
    if (!index || socket.fd < 0) return {};
    timeval timeout{0, 250000};
    if (setsockopt(socket.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) return {};
    uint16_t family{};
    const char name[] = "nl80211";
    if (!query(socket.fd, GENL_ID_CTRL, CTRL_CMD_GETFAMILY, CTRL_ATTR_FAMILY_NAME, name, sizeof(name), false,
        [&](int type, const char* p, size_t n) { if (type == CTRL_ATTR_FAMILY_ID) read_value(p,n,family); }) || !family) return {};
    if (!query(socket.fd, family, NL80211_CMD_GET_SURVEY, NL80211_ATTR_IFINDEX, &index, sizeof(index), true,
        [&](int type, const char* p, size_t n) {
            if (type != NL80211_ATTR_SURVEY_INFO) return;
            ChannelSample sample{};
            bool active = false, busy = false;
            attributes(p,n,[&](int field, const char* bytes, size_t len) {
                if (field == NL80211_SURVEY_INFO_FREQUENCY) read_value(bytes,len,sample.mhz);
                if (field == NL80211_SURVEY_INFO_TIME) active = read_value(bytes,len,sample.active);
                if (field == NL80211_SURVEY_INFO_TIME_BUSY) busy = read_value(bytes,len,sample.busy);
            });
            if (sample.mhz && active && busy) samples.push_back(sample);
        })) return {};
#endif
    return samples;
}
}
