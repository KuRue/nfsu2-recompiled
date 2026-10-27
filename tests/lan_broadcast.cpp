#include "../android/app/src/main/cpp/lan_broadcast.h"
#include <cstdio>
#include <cstdlib>
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
    require(kLanDiscoveryPort == 9999, "discovery port is 9999");
    require(!lanBroadcastsWanted(nullptr, 0), "no sockets means no lock");
    LanSocketView none[] = {LanSocketView(false, false, 0)};
    require(!lanBroadcastsWanted(none, 0), "an empty list means no lock");
    LanSocketView tcp[] = {LanSocketView(false, true, kLanDiscoveryPort)};
    require(!lanBroadcastsWanted(tcp, 1), "a stream socket does not need broadcasts");
    LanSocketView unicast[] = {LanSocketView(true, false, 80)};
    require(!lanBroadcastsWanted(unicast, 1), "ordinary datagram traffic stays unlocked");
    LanSocketView sending[] = {LanSocketView(true, true, 0)};
    require(lanBroadcastsWanted(sending, 1), "SO_BROADCAST on a datagram socket holds the lock");
    LanSocketView listening[] = {LanSocketView(true, false, kLanDiscoveryPort)};
    require(lanBroadcastsWanted(listening, 1), "the discovery port holds the lock");
    LanSocketView otherPort[] = {LanSocketView(true, false, 9998)};
    require(!lanBroadcastsWanted(otherPort, 1), "another port without broadcast stays unlocked");
    LanSocketView mixed[] = {LanSocketView(true, false, 80), LanSocketView(true, false, kLanDiscoveryPort)};
    require(lanBroadcastsWanted(mixed, 2), "one discovery socket is enough");
    LanSocketView cleared[] = {LanSocketView(true, false, 0)};
    require(!lanBroadcastsWanted(cleared, 1), "clearing broadcast and the port releases the lock");
    std::puts("LAN broadcasts passed");
}
