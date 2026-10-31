// Virtual LAN stack test: the switch frames are simulated locally, so bind,
// broadcast loopback, unicast delivery, port-unreachable, and the TCP
// handshake/stream/close path are covered without a network.
#include "../android/app/src/main/cpp/runtime_log.h"
#include "../android/app/src/main/cpp/guest_lan.inc"

#include <cstdio>
#include <cstdlib>

// Free-roam kind-1 frames bypass the guest Winsock sockets. Keep a receiver
// here so this standalone transport test covers the new dispatch boundary.
static std::vector<uint8_t> roamFrame;
extern "C" void roam_switch_receive(const uint8_t* data,size_t length){roamFrame.assign(data,data+length);}

static int failures;
static void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

static std::vector<uint8_t> frame(uint8_t kind, uint32_t src, uint32_t dst, uint16_t sport, uint16_t dport,
                                  uint8_t op, uint32_t cid, const void* data = nullptr, size_t length = 0) {
    std::vector<uint8_t> out(24 + length);
    out[0] = kind;
    vlan::putBe32(&out[4], src);
    vlan::putBe32(&out[8], dst);
    vlan::putBe16(&out[12], sport);
    vlan::putBe16(&out[14], dport);
    out[16] = op;
    out[17] = 64;
    vlan::putLe32(&out[20], cid);
    if (length) std::memcpy(&out[24], data, length);
    return out;
}

static std::vector<uint8_t> next() {
    std::vector<uint8_t> out;
    if (!vlan::takeOutgoing(out, 0)) { std::fprintf(stderr, "FAIL: expected an outbound frame\n"); ++failures; }
    return out;
}

int main() {
    vlan::setActive(true);
    check(!vlan::online(), "offline before the welcome");
    vlan::welcome(0x0a4d0947u, 0xffffe000u);
    check(vlan::online(), "online after the welcome");
    check(vlan::ownHost() == 0x0a4d0947u, "vip");

    auto peerPacket=frame(1,0x0a4d0948u,vlan::ownHost(),9900,9900,0,0);
    vlan::incoming(peerPacket.data(),peerPacket.size());
    check(roamFrame==peerPacket,"free-roam frame delivered unchanged to the peer receiver");

    // UDP bind and broadcast: the sender gets its own copy, as Windows does.
    vlan::open(10, 1);
    uint16_t port = 0;
    check(vlan::bind(10, false, 0, 9999, true, port) == 0 && port == 9999, "udp bind 9999");
    vlan::open(11, 1);
    check(vlan::bind(11, false, 0, 9999, true, port) == 0, "second udp bind with reuse");
    const char advertisement[] = "NFSU2NA";
    check(vlan::sendDatagram(10, reinterpret_cast<const uint8_t*>(advertisement), 7, 0xffffffffu, 9999) == 0,
          "send broadcast");
    auto sent = next();
    check(sent[0] == vlan::KindUdp, "broadcast kind is UDP");
    check(vlan::be32(&sent[8]) == 0xffffffffu, "broadcast destination");
    check(vlan::be16(&sent[12]) == 9999 && vlan::be16(&sent[14]) == 9999, "broadcast ports");
    check(vlan::be32(&sent[4]) == 0x0a4d0947u, "source is our vip");
    check(sent.size() == 24 + 7 && std::memcmp(&sent[24], advertisement, 7) == 0, "broadcast payload");
    for (uint32_t handle : {10u, 11u}) {
        uint32_t source = 0; uint16_t sport = 0; size_t got = 0; bool unreachable = false;
        uint8_t buffer[64];
        check(vlan::recvDatagram(handle, buffer, sizeof buffer, source, sport, got, unreachable) == 0,
              "own broadcast loops back");
        check(got == 7 && std::memcmp(buffer, advertisement, 7) == 0, "loopback payload");
        check(source == 0x0a4d0947u && sport == 9999, "loopback source");
    }

    // A frame from another player reaches the socket bound to the port.
    const char peer[] = "PEER";
    auto incoming = frame(vlan::KindUdp, 0x0a4d0948u, 0xffffffffu, 9999, 9999, vlan::UdpData, 0, peer, 4);
    vlan::incoming(incoming.data(), incoming.size());
    uint32_t source = 0; uint16_t sport = 0; size_t got = 0; bool unreachable = false;
    uint8_t buffer[64];
    check(vlan::recvDatagram(10, buffer, sizeof buffer, source, sport, got, unreachable) == 0, "peer datagram delivered");
    check(got == 4 && std::memcmp(buffer, peer, 4) == 0 && source == 0x0a4d0948u, "peer payload and source");

    // Port unreachable: the sender's socket hears WSAECONNRESET on recv.
    vlan::open(12, 1);
    check(vlan::bind(12, false, 0, 9999, true, port) == 0, "bind 9999 for unreachable");
    check(vlan::connectDatagram(12, 0x0a4d0949u, 9998) == 0, "connect the datagram socket");
    check(vlan::sendDatagram(12, reinterpret_cast<const uint8_t*>(peer), 4, 0x0a4d0949u, 9998) == 0, "unicast send");
    (void)next();
    auto unreachableFrame = frame(vlan::KindUdp, 0x0a4d0949u, 0x0a4d0947u, 9998, 9999, vlan::UdpUnreach, 0);
    vlan::incoming(unreachableFrame.data(), unreachableFrame.size());
    check(vlan::recvDatagram(12, buffer, sizeof buffer, source, sport, got, unreachable) == 0 && unreachable,
          "port unreachable reaches the sender");

    // TCP listener: SYN -> SYNACK, accept, data both ways, FIN.
    vlan::open(20, 2);
    check(vlan::bind(20, true, 0, 9999, true, port) == 0, "tcp bind 9999");
    check(vlan::listen(20, 4) == 0, "tcp listen");
    uint32_t cid = 0x1234abcdu;
    auto syn = frame(vlan::KindTcp, 0x0a4d0948u, 0x0a4d0947u, 50000, 9999, vlan::TcpSyn, cid);
    vlan::incoming(syn.data(), syn.size());
    auto synack = next();
    check(synack[0] == vlan::KindTcp && synack[16] == vlan::TcpSynAck, "SYN answered with SYNACK");
    check(vlan::be16(&synack[12]) == 9999 && vlan::be16(&synack[14]) == 50000, "SYNACK ports");
    uint32_t accepted = 0x5000;
    check(vlan::accept(20, accepted) == 0, "accept queued connection");
    uint32_t peerIp = 0; uint16_t peerPort = 0;
    check(vlan::localAddress(accepted, peerIp, peerPort, true) == 0 && peerIp == 0x0a4d0948u && peerPort == 50000,
          "accepted peer address");
    const char reply[] = "HELLO";
    int error = 0;
    check(vlan::sendStream(accepted, reinterpret_cast<const uint8_t*>(reply), 5, error) == 5, "send stream data");
    auto data = next();
    check(data[0] == vlan::KindTcp && data[16] == vlan::TcpData && data.size() == 24 + 5, "DATA frame");
    check(std::memcmp(&data[24], reply, 5) == 0, "DATA payload");

    const char question[] = "JOIN";
    auto peerData = frame(vlan::KindTcp, 0x0a4d0948u, 0x0a4d0947u, 50000, 9999, vlan::TcpData, cid, question, 4);
    vlan::incoming(peerData.data(), peerData.size());
    size_t received = 0; bool reset = false;
    check(vlan::recvStream(accepted, buffer, sizeof buffer, received, reset) == 0 && received == 4, "received DATA");
    check(std::memcmp(buffer, question, 4) == 0, "received payload");
    auto fin = frame(vlan::KindTcp, 0x0a4d0948u, 0x0a4d0947u, 50000, 9999, vlan::TcpFin, cid);
    vlan::incoming(fin.data(), fin.size());
    check(vlan::recvStream(accepted, buffer, sizeof buffer, received, reset) == 0 && received == 0 && !reset,
          "FIN reads as EOF");

    // TCP client: connect -> SYN, SYNACK completes it.
    vlan::open(30, 3);
    check(vlan::connectStream(30, 0x0a4d0948u, 9999) == vlan::kWouldBlock, "connect sends a SYN and waits");
    auto clientSyn = next();
    check(clientSyn[0] == vlan::KindTcp && clientSyn[16] == vlan::TcpSyn, "client SYN frame");
    check(vlan::connectResult(30) == vlan::kWouldBlock, "connect still pending");
    uint32_t cid2 = uint32_t(clientSyn[20]) | uint32_t(clientSyn[21]) << 8 | uint32_t(clientSyn[22]) << 16 | uint32_t(clientSyn[23]) << 24;
    auto clientSynAck = frame(vlan::KindTcp, 0x0a4d0948u, 0x0a4d0947u, 9999, 0, vlan::TcpSynAck, cid2);
    // The client's local port is in the SYN frame; echo it back.
    clientSynAck[14] = clientSyn[12];
    clientSynAck[15] = clientSyn[13];
    vlan::incoming(clientSynAck.data(), clientSynAck.size());
    check(vlan::connectResult(30) == 0, "SYNACK establishes the client");

    // A client's discovery socket often sends its broadcast from an ephemeral
    // port yet still hears the hosts' 9999 announcements on that socket.
    vlan::open(60, 1);
    check(vlan::sendDatagram(60, reinterpret_cast<const uint8_t*>(peer), 4, 0xffffffffu, 9999) == 0,
          "ephemeral discovery broadcast");
    (void)next();
    const char hostAdvert[] = "HOST";
    auto hostBroadcast = frame(vlan::KindUdp, 0x0a4d094au, 0xffffffffu, 9999, 9999, vlan::UdpData, 0,
                               hostAdvert, 4);
    vlan::incoming(hostBroadcast.data(), hostBroadcast.size());
    check(vlan::recvDatagram(60, buffer, sizeof buffer, source, sport, got, unreachable) == 0 &&
          got == 4 && std::memcmp(buffer, hostAdvert, 4) == 0 && source == 0x0a4d094au,
          "host broadcast reaches the discovery socket");

    // ICMP: an echo request to a peer goes out on the switch, and a reply
    // comes back to the raw socket with an IPv4 header.
    vlan::open(50, 4);
    uint8_t echo[8] = {8, 0, 0, 0, 0x12, 0x34, 0, 1};
    check(vlan::sendIcmp(50, echo, sizeof echo, 0x0a4d0948u) == 0, "send ICMP echo");
    auto icmp = next();
    check(icmp[0] == vlan::KindIcmp && icmp.size() == 24 + sizeof echo, "ICMP frame");
    uint8_t echoReply[8]; std::memcpy(echoReply, echo, sizeof echo); echoReply[0] = 0;
    auto icmpReply = frame(vlan::KindIcmp, 0x0a4d0948u, 0x0a4d0947u, 0, 0, 0, 0, echoReply, sizeof echoReply);
    vlan::incoming(icmpReply.data(), icmpReply.size());
    uint32_t icmpSource = 0; uint16_t icmpPort = 0; size_t icmpGot = 0; bool icmpUnreachable = false;
    check(vlan::recvDatagram(50, buffer, sizeof buffer, icmpSource, icmpPort, icmpGot, icmpUnreachable) == 0 &&
          icmpGot == sizeof echoReply && buffer[0] == 0 && icmpSource == 0x0a4d0948u, "ICMP reply delivered");

    vlan::disconnected("test link down");
    check(!vlan::online(), "offline after the link drops");

    vlan::closeSocket(10, false);
    vlan::closeSocket(11, false);
    vlan::closeSocket(12, false);
    vlan::closeSocket(20, false);
    vlan::closeSocket(accepted, false);
    vlan::closeSocket(30, false);
    vlan::closeSocket(50, false);
    vlan::closeSocket(60, false);
    if (failures) std::fprintf(stderr, "%d virtual LAN check(s) failed\n", failures);
    else std::puts("virtual LAN passed");
    return failures ? 1 : 0;
}
