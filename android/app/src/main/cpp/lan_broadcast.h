#pragma once
#include <cstddef>
// Wi-Fi drops packets that are not addressed to this device unless a multicast
// lock is held. LAN discovery uses UDP broadcasts, not the EA Nation hostname.
// A datagram socket needs that lock while it may send broadcasts, or while it
// is bound to the discovery port and can receive them.
inline constexpr unsigned kLanDiscoveryPort = 9999;
struct LanSocketView {
    bool datagram;
    bool broadcast;
    unsigned boundPort;
    LanSocketView(bool datagram, bool broadcast, unsigned boundPort)
        : datagram(datagram), broadcast(broadcast), boundPort(boundPort) {}
};
inline bool lanSocketNeedsWifiBroadcast(const LanSocketView& socket) {
    return socket.datagram && (socket.broadcast || socket.boundPort == kLanDiscoveryPort);
}
inline bool lanBroadcastsWanted(const LanSocketView* sockets, std::size_t count) {
    if (!sockets) return false;
    for (std::size_t index = 0; index < count; ++index)
        if (lanSocketNeedsWifiBroadcast(sockets[index])) return true;
    return false;
}
