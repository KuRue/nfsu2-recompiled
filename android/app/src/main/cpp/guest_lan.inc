// guest_lan.inc -- the nfsu2.xyz virtual LAN behind the Winsock HLE.
//
// The original game's LAN code (EA's server.dll and DirtySock) speaks UDP and
// TCP between real addresses. nfsu2.xyz does not expose such a network: its
// /lan endpoint is a WebSocket switch that relays frames between browsers,
// each with a virtual 10.77.x.x address. The browser port implements the whole
// virtual IP stack (net_lan.c); this is the same stack for the native game,
// with the WebSocket (TLS, hello, welcome) living in Java and only raw switch
// frames crossing JNI:
//
//   guest Winsock calls --guest_network.inc--> vlan::* --outbox--> Java /lan
//   guest Winsock calls <--vlan::incoming-- Java /lan <-- WebSocket switch
//
// The frame format is web/src/net/proto.ts: sixteen bytes of switch header
// (kind, source, destination, ports, big-endian) followed by the eight-byte
// LAN header (op, ttl, reserved, connection id) and the payload:
//
//   [0..3]  kind: 3 UDP, 4 TCP, 5 ICMP (little-endian u32)
//   [4..7]  source virtual address (big-endian; switched to the sender's)
//   [8..11] destination virtual address (big-endian, 0xffffffff broadcast)
//   [12..13] source port, [14..15] destination port (big-endian)
//   [16] op, [17] ttl, [18..19] reserved, [20..23] connection id (LE)
//
// Windows semantics are the reference's (net_lan.c): a second binder of a
// UDP/TCP port needs SO_REUSEADDR; broadcasts go to every socket bound to the
// port (and loop back to the sender, as Windows does); a unicast without a
// listener gets a "port unreachable" notice; TCP is SYN/SYNACK/DATA/FIN/RST
// with no retransmission (the WebSocket and the switch are ordered), a refused
// connect fails at once, and an unanswered SYN fails after three seconds.
//
// All state is under one mutex. Guest calls hold the guest machine lock when
// they enter; nothing here takes it, and the Java thread only ever takes this
// mutex, so the two never deadlock. Guest blocking waits release the machine
// (guest_network.inc) and sleep on changeCv.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <poll.h>
#include <string>
#include <vector>

namespace vlan {
using Clock=std::chrono::steady_clock;

// Winsock error numbers, so the HLE can return them unchanged.
constexpr int kInval=10022,kWouldBlock=10035,kNotSock=10038,kMsgSize=10040,kAddrInUse=10048,
    kAddrNotAvail=10049,kNetDown=10050,kNetReset=10052,kConnAborted=10053,kConnReset=10054,
    kNotConn=10057,kNoBufs=10055,kTimedOut=10060,kConnRefused=10061,kHostUnreach=10065;

constexpr size_t kMaxPayload=1472-8;   // net_proto.h NET_MAX_PAYLOAD - NET_LAN_HDR
constexpr size_t kOutboxMax=8192;      // switch frames waiting for the Java writer
constexpr size_t kDgramQueueMax=1024;
constexpr size_t kDgramBytesMax=1u<<20;
constexpr size_t kStreamRxMax=1u<<20;
constexpr size_t kStreamRxRemoteMax=16u<<20;
constexpr uint32_t kSynTimeoutMs=3000;

// net_proto.h
enum : uint8_t { KindUdp=3,KindTcp=4,KindIcmp=5 };
enum : uint8_t { UdpData=0,UdpUnreach=1 };
enum : uint8_t { TcpSyn=1,TcpSynAck=2,TcpRst=3,TcpData=4,TcpFin=5,TcpGone=6 };

// The three ports EA's LAN code uses (runtime/winsock.c's loopback map).
inline bool wantsPort(uint16_t port){return port==9999||port==9900||port==3658;}
inline uint32_t be32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline uint16_t be16(const uint8_t* p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
inline void putBe32(uint8_t* p,uint32_t v){p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
inline void putBe16(uint8_t* p,uint16_t v){p[0]=uint8_t(v>>8);p[1]=uint8_t(v);}
inline void putLe32(uint8_t* p,uint32_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);p[2]=uint8_t(v>>16);p[3]=uint8_t(v>>24);}

struct Dgram {uint32_t src=0;uint16_t sport=0;bool unreachable=false;std::vector<uint8_t> data;};
struct VConn;
struct VSock;
struct VConn {
    enum State{SynSent=1,Established,Refused,Failed};
    uint32_t cid=0,lip=0,rip=0;uint16_t lport=0,rport=0;
    int state=SynSent;bool local=false;
    bool rxFin=false,rxRst=false,txFin=false,rstSent=false;
    std::vector<uint8_t> rx;size_t rxOff=0;
    std::weak_ptr<VConn> peer;      // local connections only
    VSock* listener=nullptr;        // waiting in a listener's accept queue
    std::shared_ptr<VConn> accNext; // accept queue link (listener side)
    Clock::time_point deadline{};
};
struct VSock {
    bool used=false,tcp=false,bound=false,reuse=false,listening=false,icmp=false,hasPeer=false;
    uint32_t ip=0,peerIp=0;uint16_t port=0,peerPort=0;int backlog=1;
    std::deque<Dgram> q;size_t qBytes=0;
    std::shared_ptr<VConn> stream;              // connected or accepted stream
    std::shared_ptr<VConn> accHead,accTail;     // listener's accept queue
    int pending=0;
    struct Sent{uint32_t ip=0;uint16_t port=0;uint32_t seq=0;};
    Sent sent[8]{};uint32_t sentSeq=0;
};

std::mutex mu;
std::condition_variable changeCv,outCv;
std::map<uint32_t,VSock> socks;
std::map<uint32_t,std::weak_ptr<VConn>> conns;   // by connection id
std::deque<std::vector<uint8_t>> outbox;
std::atomic<bool> providerActive{false},linkOnline{false};
uint32_t vip=0,mask=0,generation=0;
std::string lastStatus="not connected";
bool saidBind=false,saidFrame=false;
Clock::time_point lastFrameLog{};

inline bool active(){return providerActive.load(std::memory_order_acquire);}
inline bool online(){return linkOnline.load(std::memory_order_acquire);}
inline uint32_t ownHost(){return vip;}
inline uint32_t address(){return vip;}
inline uint32_t netmask(){return mask;}

void setActive(bool on){
    providerActive.store(on,std::memory_order_release);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: nfsu2.xyz provider %s",on?"selected":"off");
}
inline bool inSubnet(uint32_t ip){
    return mask&&(ip&mask)==(vip&mask);
}
inline bool broadcastAddress(uint32_t ip){
    return ip==0xffffffffu||(mask&&ip==((vip&mask)|~mask));
}
uint32_t rngState=0x9e3779b9u;
uint32_t random32(){  // xorshift32; every caller holds mu
    if(!rngState)rngState=uint32_t(Clock::now().time_since_epoch().count())|1u;
    rngState^=rngState<<13;rngState^=rngState>>17;rngState^=rngState<<5;
    return rngState;
}
uint32_t newCid(){
    uint32_t cid=0;
    while(!cid)cid=random32();
    return cid;
}
void noteFrame(){
    auto now=Clock::now();
    ++generation;
    if(!saidFrame){saidFrame=true;NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: first frame from the switch");}
    else if(now-lastFrameLog>std::chrono::seconds(30)){
        lastFrameLog=now;
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: %u frames received, address %u.%u.%u.%u",
            generation,(vip>>24)&255,(vip>>16)&255,(vip>>8)&255,vip&255);
    }
}

// ---- the Java side -----------------------------------------------------------
void welcome(uint32_t address,uint32_t netmask){
    std::lock_guard<std::mutex> lock(mu);
    vip=address;mask=netmask;linkOnline.store(true,std::memory_order_release);
    lastStatus="online";
    changeCv.notify_all();
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: on the nfsu2.xyz switch as %u.%u.%u.%u/%u",
        (vip>>24)&255,(vip>>16)&255,(vip>>8)&255,vip&255,__builtin_popcount(mask));
}
void disconnected(const char* reason){
    std::lock_guard<std::mutex> lock(mu);
    if(!linkOnline.exchange(false))return;
    lastStatus=reason?reason:"connection lost";
    for(auto& entry:conns){
        auto conn=entry.second.lock();
        if(!conn||conn->local)continue;
        if(conn->state==VConn::SynSent)conn->state=VConn::Failed;
        else if(conn->state==VConn::Established){conn->rxRst=true;conn->rx.clear();conn->rxOff=0;}
    }
    changeCv.notify_all();
    NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Virtual LAN: disconnected (%s)",lastStatus.c_str());
}
bool waitChange(int timeoutMs){
    std::unique_lock<std::mutex> lock(mu);
    if(timeoutMs<=0)return false;
    return changeCv.wait_for(lock,std::chrono::milliseconds(timeoutMs))!=std::cv_status::timeout;
}
bool takeOutgoing(std::vector<uint8_t>& frame,int timeoutMs){
    std::unique_lock<std::mutex> lock(mu);
    if(outbox.empty()&&timeoutMs>0)outCv.wait_for(lock,std::chrono::milliseconds(timeoutMs));
    if(outbox.empty())return false;
    frame=std::move(outbox.front());outbox.pop_front();
    return true;
}

// ---- framing -----------------------------------------------------------------
bool txRaw(uint8_t kind,uint32_t dst,uint16_t sport,uint16_t dport,uint8_t op,uint32_t cid,
           const uint8_t* data,size_t length){
    if(!online()||length>kMaxPayload)return false;
    if(outbox.size()>=kOutboxMax)return false;
    std::vector<uint8_t> frame(24+length);
    frame[0]=kind;frame[1]=frame[2]=frame[3]=0;
    putBe32(&frame[4],vip);putBe32(&frame[8],dst);
    putBe16(&frame[12],sport);putBe16(&frame[14],dport);
    frame[16]=op;frame[17]=64;frame[18]=frame[19]=0;putLe32(&frame[20],cid);
    if(length)std::memcpy(&frame[24],data,length);
    outbox.push_back(std::move(frame));
    outCv.notify_one();
    return true;
}

// Automatic answers (a port unreachable, a RST to a stray SYN, an echo reply)
// share a small budget: someone on the public switch cannot make this phone
// spend its upstream on answers to traffic the game never asked for.
bool allowAuto(size_t length){
    static Clock::time_point window{};
    static size_t used=0;
    auto now=Clock::now();
    if(window.time_since_epoch().count()==0||now-window>=std::chrono::seconds(1)){window=now;used=0;}
    if(used+length+64>32768)return false;
    used+=length+64;
    return true;
}

// ---- sockets -----------------------------------------------------------------
VSock* find(uint32_t handle){
    auto found=socks.find(handle);
    return found==socks.end()?nullptr:&found->second;
}
void open(uint32_t handle,int kind){
    std::lock_guard<std::mutex> lock(mu);
    if(socks.count(handle))return;
    VSock& socket=socks[handle];
    socket.used=true;
    socket.tcp=kind!=1;
    socket.icmp=kind==4;
    if(!saidBind){
        saidBind=true;
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: first socket opened (%s)",kind==1?"UDP":kind==4?"ICMP":"TCP");
    }
}
uint16_t ephemeral(bool tcp){
    for(int attempt=0;attempt<20000;++attempt){
        uint16_t port=uint16_t(49152u+random32()%(65535u-49152u));
        bool used=false;
        for(auto& entry:socks){
            const VSock& other=entry.second;
            if(other.used&&other.bound&&other.tcp==tcp&&other.port==port){used=true;break;}
        }
        if(!used)return port;
    }
    return 0;
}
bool portUsed(bool tcp,uint16_t port,uint32_t ip,VSock* except,bool* reuseAll){
    bool used=false;*reuseAll=true;
    for(auto& entry:socks){
        VSock& other=entry.second;
        if(!other.used||&other==except||!other.bound||other.tcp!=tcp||other.port!=port)continue;
        if(other.ip==ip||!other.ip||!ip){used=true;if(!other.reuse)*reuseAll=false;}
    }
    return used;
}
int bind(uint32_t handle,bool tcp,uint32_t ip,uint16_t port,bool reuse,uint16_t& boundPort){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket)return kNotSock;
    if(ip&&ip!=vip&&(ip>>24)!=127)return kAddrNotAvail;
    if(socket->bound)return kInval;
    if(!port){
        port=ephemeral(tcp);
        if(!port)return kNoBufs;
    }else{
        bool reuseAll=false;
        if(portUsed(tcp,port,ip,socket,&reuseAll)&&!reuse)return kAddrInUse;
        (void)reuseAll;
    }
    socket->ip=ip;socket->port=port;socket->bound=true;socket->reuse=reuse;boundPort=port;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: bind %s %u.%u.%u.%u:%u",tcp?"tcp":"udp",
        (ip>>24)&255,(ip>>16)&255,(ip>>8)&255,ip&255,port);
    return 0;
}
int listen(uint32_t handle,int backlog){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket||!socket->tcp)return kInval;
    if(!socket->bound){socket->port=ephemeral(true);socket->bound=true;if(!socket->port)return kNoBufs;}
    socket->listening=true;socket->backlog=backlog>0?backlog:1;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: listening on tcp port %u (backlog %d)",socket->port,socket->backlog);
    return 0;
}
VSock* findListener(uint32_t dst,uint16_t port){
    for(auto& entry:socks){
        VSock& socket=entry.second;
        if(!socket.used||!socket.tcp||!socket.listening||socket.port!=port)continue;
        if(!socket.ip||socket.ip==dst||(((dst>>24)==127)&&((socket.ip>>24)==127)))return &socket;
    }
    return nullptr;
}
void sendRst(std::shared_ptr<VConn> conn){
    if(!conn||conn->rstSent||conn->rxRst)return;
    conn->rstSent=true;
    if(conn->local){
        auto peer=conn->peer.lock();
        if(peer){peer->rxRst=true;peer->rx.clear();peer->rxOff=0;}
        return;
    }
    if(conn->state==VConn::Established||conn->state==VConn::SynSent)
        txRaw(KindTcp,conn->rip,conn->lport,conn->rport,TcpRst,conn->cid,nullptr,0);
}
int accept(uint32_t handle,uint32_t newHandle){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket||!socket->listening)return kInval;
    if(!socket->accHead)return kWouldBlock;
    auto conn=socket->accHead;
    socket->accHead=conn->accNext;conn->accNext.reset();
    if(!socket->accHead)socket->accTail.reset();
    --socket->pending;conn->listener=nullptr;
    VSock& accepted=socks[newHandle];
    accepted.used=true;accepted.tcp=true;accepted.bound=true;accepted.ip=conn->lip;accepted.port=conn->lport;
    accepted.hasPeer=true;accepted.peerIp=conn->rip;accepted.peerPort=conn->rport;accepted.stream=conn;
    return 0;
}
// Starts a connection and returns at once: kWouldBlock means the SYN is out
// (the HLE waits on connectResult with the machine released).
int connectStream(uint32_t handle,uint32_t ip,uint16_t port){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket)return kNotSock;
    if(!socket->bound){
        socket->port=ephemeral(true);
        if(!socket->port)return kNoBufs;
        socket->bound=true;socket->ip=0;
    }
    if(!online())return kNetDown;
    if(socket->stream){
        sendRst(socket->stream);
        socket->stream.reset();
    }
    bool loop=(ip>>24)==127,self=ip==vip;
    uint32_t lip=loop?0x7f000001u:vip;
    uint32_t cid=newCid();
    if(loop||self){
        VSock* listener=findListener(ip,port);
        if(!listener||listener->pending>=listener->backlog+4)return kConnRefused;
        auto client=std::make_shared<VConn>(),server=std::make_shared<VConn>();
        client->cid=server->cid=cid;client->local=server->local=true;
        client->lip=lip;client->lport=socket->port;client->rip=ip;client->rport=port;
        server->lip=ip;server->lport=port;server->rip=lip;server->rport=socket->port;
        client->state=server->state=VConn::Established;
        client->peer=server;server->peer=client;
        server->listener=listener;server->accNext.reset();
        if(listener->accTail)listener->accTail->accNext=server;else listener->accHead=server;
        listener->accTail=server;++listener->pending;
        socket->stream=client;socket->hasPeer=true;socket->peerIp=ip;socket->peerPort=port;
        return 0;
    }
    if(!inSubnet(ip))return kHostUnreach;
    auto conn=std::make_shared<VConn>();
    conn->cid=cid;conn->lip=lip;conn->lport=socket->port;conn->rip=ip;conn->rport=port;
    conn->state=VConn::SynSent;conn->deadline=Clock::now()+std::chrono::milliseconds(kSynTimeoutMs);
    if(!txRaw(KindTcp,ip,socket->port,port,TcpSyn,cid,nullptr,0))return kTimedOut;
    conns[cid]=conn;socket->stream=conn;socket->hasPeer=true;socket->peerIp=ip;socket->peerPort=port;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: tcp connect %u.%u.%u.%u:%u from port %u",
        (ip>>24)&255,(ip>>16)&255,(ip>>8)&255,ip&255,port,socket->port);
    return kWouldBlock;
}
int connectDatagram(uint32_t handle,uint32_t ip,uint16_t port){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket)return kNotSock;
    socket->hasPeer=true;socket->peerIp=ip;socket->peerPort=port;
    return 0;
}
int connectResult(uint32_t handle){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket||!socket->stream)return kNotConn;
    auto& conn=socket->stream;
    if(conn->state==VConn::SynSent&&Clock::now()>=conn->deadline)conn->state=VConn::Failed;
    if(conn->state==VConn::Established)return 0;
    if(conn->state==VConn::Refused)return kConnRefused;
    if(conn->state==VConn::Failed)return kTimedOut;
    return kWouldBlock;
}
ssize_t sendStream(uint32_t handle,const uint8_t* data,size_t length,int& error){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);error=0;
    if(!socket||!socket->stream){error=kNotConn;return -1;}
    auto conn=socket->stream;
    if(conn->state!=VConn::Established){error=kNotConn;return -1;}
    if(conn->rxRst||conn->rstSent||conn->txFin){error=kConnReset;return -1;}
    if(conn->local){
        auto peer=conn->peer.lock();
        if(!peer||peer->rxRst||peer->txFin){error=kConnReset;return -1;}
        size_t room=peer->rx.size()<kStreamRxMax?kStreamRxMax-peer->rx.size():0;
        size_t count=std::min(length,room);
        if(!count){error=kWouldBlock;return -1;}
        peer->rx.insert(peer->rx.end(),data,data+count);
        changeCv.notify_all();
        return ssize_t(count);
    }
    size_t sent=0;
    while(sent<length){
        if(outbox.size()>=kOutboxMax){if(!sent){error=kWouldBlock;return -1;}break;}
        size_t chunk=std::min(length-sent,kMaxPayload);
        if(!txRaw(KindTcp,conn->rip,conn->lport,conn->rport,TcpData,conn->cid,data+sent,chunk))break;
        sent+=chunk;
    }
    if(!sent&&length){error=kWouldBlock;return -1;}
    return ssize_t(sent);
}
int recvStream(uint32_t handle,uint8_t* data,size_t length,size_t& got,bool& reset){
    std::lock_guard<std::mutex> lock(mu);
    got=0;reset=false;
    VSock* socket=find(handle);
    if(!socket||!socket->stream)return kNotConn;
    auto conn=socket->stream;
    if(conn->rxOff<conn->rx.size()){
        size_t count=std::min(length,conn->rx.size()-conn->rxOff);
        std::memcpy(data,conn->rx.data()+conn->rxOff,count);
        conn->rxOff+=count;got=count;
        if(conn->rxOff==conn->rx.size()){conn->rx.clear();conn->rxOff=0;}
        changeCv.notify_all();
        return 0;
    }
    conn->rx.clear();conn->rxOff=0;
    if(conn->rxRst){reset=true;return 0;}
    if(conn->rxFin)return 0;
    if(conn->state!=VConn::Established)return kNotConn;
    return kWouldBlock;
}
int recvDatagram(uint32_t handle,uint8_t* data,size_t length,uint32_t& source,uint16_t& sport,size_t& got,bool& unreachable){
    std::lock_guard<std::mutex> lock(mu);
    got=0;unreachable=false;
    VSock* socket=find(handle);
    if(!socket)return kNotSock;
    while(!socket->q.empty()){
        Dgram& dgram=socket->q.front();
        if(dgram.unreachable){
            bool ours=socket->hasPeer&&dgram.src==socket->peerIp&&dgram.sport==socket->peerPort;
            socket->qBytes-=dgram.data.size();socket->q.pop_front();
            if(!ours)continue;
            unreachable=true;return 0;
        }
        source=dgram.src;sport=dgram.sport;
        size_t count=std::min(length,dgram.data.size());
        std::memcpy(data,dgram.data.data(),count);got=count;
        int result=0;
        if(dgram.data.size()>length)result=kMsgSize;
        socket->qBytes-=dgram.data.size();socket->q.pop_front();
        return result;
    }
    return kWouldBlock;
}
void sentNote(VSock& socket,uint32_t ip,uint16_t port){
    int slot=-1;
    for(int i=0;i<8&&slot<0;++i)if(socket.sent[i].seq&&socket.sent[i].ip==ip&&socket.sent[i].port==port)slot=i;
    for(int i=0;i<8&&slot<0;++i)if(!socket.sent[i].seq)slot=i;
    if(slot<0){
        slot=0;
        for(int i=1;i<8;++i)if(int32_t(socket.sent[i].seq-socket.sent[slot].seq)<0)slot=i;
    }
    if(!++socket.sentSeq)++socket.sentSeq;
    socket.sent[slot]={ip,port,socket.sentSeq};
}
int sendDatagram(uint32_t handle,const uint8_t* data,size_t length,uint32_t ip,uint16_t port){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket)return kNotSock;
    if(length>kMaxPayload)return kMsgSize;
    // While the switch is away a datagram is dropped, as UDP would drop it;
    // the game's discovery loop must not stall waiting for a welcome.
    if(!online())return 0;
    if(!socket->bound){
        socket->port=ephemeral(false);
        if(!socket->port)return kNoBufs;
        socket->bound=true;
    }
    bool loop=(ip>>24)==127,self=ip==vip;
    uint32_t source=loop?0x7f000001u:vip;
    if(loop||self){
        // A datagram to our own address never leaves the browser.
        for(auto& entry:socks){
            VSock& other=entry.second;
            if(!other.used||other.tcp||other.icmp||!other.bound||other.port!=port)continue;
            if(!(!other.ip||other.ip==ip||((ip>>24)==127&&(other.ip>>24)==127)))continue;
            if(other.q.size()>=kDgramQueueMax||other.qBytes+length>kDgramBytesMax)continue;
            Dgram dgram;dgram.src=source;dgram.sport=socket->port;dgram.data.assign(data,data+length);
            other.qBytes+=length;other.q.push_back(std::move(dgram));
        }
        changeCv.notify_all();
        return 0;
    }
    if(broadcastAddress(ip)){
        // Windows loops a broadcast back to the sender's own sockets.
        for(auto& entry:socks){
            VSock& other=entry.second;
            if(!other.used||other.tcp||other.icmp||!other.bound||other.port!=port)continue;
            if(other.q.size()>=kDgramQueueMax||other.qBytes+length>kDgramBytesMax)continue;
            Dgram dgram;dgram.src=vip;dgram.sport=socket->port;dgram.data.assign(data,data+length);
            other.qBytes+=length;other.q.push_back(std::move(dgram));
        }
        txRaw(KindUdp,0xffffffffu,socket->port,port,UdpData,0,data,length);
    }else if(inSubnet(ip)){
        if(txRaw(KindUdp,ip,socket->port,port,UdpData,0,data,length))sentNote(*socket,ip,port);
    }
    // Outside the switch's subnet the datagram is dropped, as on a host with
    // no route to it; the game sees a successful send.
    changeCv.notify_all();
    return 0;
}
int sendIcmp(uint32_t handle,const uint8_t* data,size_t length,uint32_t ip){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket)return kNotSock;
    if(length>kMaxPayload)return kMsgSize;
    if(!online())return 0;
    bool loop=(ip>>24)==127,self=ip==vip;
    if(loop||self){
        if(length>=8&&data[0]==8){
            std::vector<uint8_t> reply(data,data+length);
            reply[0]=0;reply[1]=0;reply[2]=reply[3]=0;
            uint32_t sum=0;
            for(size_t i=0;i+1<reply.size();i+=2)sum+=uint32_t(reply[i])<<8|reply[i+1];
            if(reply.size()&1)sum+=uint32_t(reply.back())<<8;
            while(sum>>16)sum=(sum&0xffff)+(sum>>16);
            sum=~sum&0xffff;reply[2]=uint8_t(sum>>8);reply[3]=uint8_t(sum);
            for(auto& entry:socks){
                VSock& other=entry.second;
                if(!other.used||!other.icmp)continue;
                Dgram dgram;dgram.src=ip;dgram.data=reply;
                if(other.q.size()<kDgramQueueMax&&other.qBytes+reply.size()<=kDgramBytesMax){
                    other.qBytes+=reply.size();other.q.push_back(std::move(dgram));
                }
            }
        }
    }else if(inSubnet(ip)&&!broadcastAddress(ip)){
        txRaw(KindIcmp,ip,0,0,0,0,data,length);
    }
    changeCv.notify_all();
    return 0;
}
int closeSocket(uint32_t handle,bool abortive){
    std::lock_guard<std::mutex> lock(mu);
    auto found=socks.find(handle);
    if(found==socks.end())return kNotSock;
    VSock& socket=found->second;
    if(socket.stream){
        auto conn=socket.stream;
        if(abortive)sendRst(conn);
        else if(conn->state==VConn::Established&&!conn->txFin&&!conn->rxRst){
            conn->txFin=true;
            if(conn->local){auto peer=conn->peer.lock();if(peer)peer->rxFin=true;}
            else txRaw(KindTcp,conn->rip,conn->lport,conn->rport,TcpFin,conn->cid,nullptr,0);
        }else if(conn->state==VConn::SynSent)sendRst(conn);
        if(conn->local){auto peer=conn->peer.lock();if(peer)peer->peer.reset();}
        conns.erase(conn->cid);
    }
    if(socket.listening){
        while(socket.accHead){
            auto conn=socket.accHead;
            socket.accHead=conn->accNext;conn->accNext.reset();
            sendRst(conn);conns.erase(conn->cid);
        }
        socket.accTail.reset();socket.pending=0;
    }
    socket.q.clear();socket.qBytes=0;
    socks.erase(found);
    changeCv.notify_all();
    return 0;
}
int shutdownStream(uint32_t handle,int how){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket||!socket->stream)return kNotConn;
    auto conn=socket->stream;
    if(how==1||how==2){
        if(conn->state==VConn::Established&&!conn->txFin&&!conn->rxRst){
            conn->txFin=true;
            if(conn->local){auto peer=conn->peer.lock();if(peer)peer->rxFin=true;}
            else txRaw(KindTcp,conn->rip,conn->lport,conn->rport,TcpFin,conn->cid,nullptr,0);
        }
    }
    changeCv.notify_all();
    return 0;
}
int localAddress(uint32_t handle,uint32_t& ip,uint16_t& port,bool peer){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket)return kNotSock;
    if(peer){
        if(!socket->hasPeer&&!socket->stream)return kNotConn;
        ip=socket->stream?socket->stream->rip:socket->peerIp;
        port=socket->stream?socket->stream->rport:socket->peerPort;
    }else{
        ip=socket->ip;port=socket->port;
    }
    return 0;
}
bool readable(uint32_t handle,size_t& bytes){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    bytes=0;
    if(!socket)return false;
    if(socket->stream){bytes=socket->stream->rx.size()-socket->stream->rxOff;return bytes!=0||socket->stream->rxFin||socket->stream->rxRst;}
    if(!socket->q.empty()){bytes=socket->q.front().data.size();return true;}
    return false;
}
short pollEvents(uint32_t handle,short events){
    std::lock_guard<std::mutex> lock(mu);
    VSock* socket=find(handle);
    if(!socket)return POLLNVAL;
    short result=0;
    if(socket->stream){
        auto& conn=socket->stream;
        if(conn->state==VConn::SynSent)return 0;
        if(conn->state!=VConn::Established)return POLLERR;
        if(conn->rx.size()>conn->rxOff||conn->rxFin||conn->rxRst)result|=POLLIN;
        if(conn->rxRst)result|=POLLHUP;
        else{
            bool writable=false;
            if(conn->local){auto peer=conn->peer.lock();writable=peer&&peer->rx.size()<kStreamRxMax;}
            else writable=online()&&outbox.size()<kOutboxMax-4;
            if(writable)result|=POLLOUT;
        }
    }else if(socket->listening){
        if(socket->accHead)result|=POLLIN;
    }else if(!socket->q.empty()){
        result|=POLLIN;
    }else if(socket->tcp&&socket->bound){
        result|=POLLOUT;
    }else{
        result|=POLLIN|POLLOUT;
    }
    return short(result&(events|POLLHUP|POLLERR|POLLNVAL));
}

// ---- received frames ---------------------------------------------------------
void deliverUdp(uint32_t src,uint16_t sport,uint32_t dst,uint16_t dport,const uint8_t* data,size_t length,bool broadcast){
    VSock* first=nullptr;VSock* exact=nullptr;
    int delivered=0;
    for(auto& entry:socks){
        VSock& socket=entry.second;
        if(!socket.used||socket.tcp||socket.icmp||!socket.bound||socket.port!=dport)continue;
        if(!(!socket.ip||socket.ip==dst||(broadcast&&socket.ip==vip)||((dst>>24)==127&&(socket.ip>>24)==127)))continue;
        if(broadcast){
            if(socket.q.size()>=kDgramQueueMax||socket.qBytes+length>kDgramBytesMax)continue;
            Dgram dgram;dgram.src=src;dgram.sport=sport;dgram.data.assign(data,data+length);
            socket.qBytes+=length;socket.q.push_back(std::move(dgram));++delivered;continue;
        }
        if(!first)first=&socket;
        if(!exact&&socket.ip==dst)exact=&socket;
    }
    if(!broadcast&&(exact||first)){
        VSock& socket=exact?*exact:*first;
        if(socket.q.size()<kDgramQueueMax&&socket.qBytes+length<=kDgramBytesMax){
            Dgram dgram;dgram.src=src;dgram.sport=sport;dgram.data.assign(data,data+length);
            socket.qBytes+=length;socket.q.push_back(std::move(dgram));++delivered;
        }
    }
    if(!delivered&&!broadcast&&allowAuto(0)){
        // "ICMP port unreachable" from this browser: the sender's next recv
        // on a connected socket fails, as on a real network.
        txRaw(KindUdp,src,dport,sport,UdpUnreach,0,nullptr,0);
    }
}
void deliverUnreachable(uint32_t src,uint16_t sport,uint16_t dport){
    VSock* sender=nullptr;VSock* connected=nullptr;VSock* first=nullptr;uint32_t best=0;
    for(auto& entry:socks){
        VSock& socket=entry.second;
        if(!socket.used||socket.tcp||socket.icmp||!socket.bound||socket.port!=dport)continue;
        for(auto& sent:socket.sent)
            if(sent.seq&&sent.ip==src&&sent.port==sport&&(!sender||int32_t(sent.seq-best)>0)){sender=&socket;best=sent.seq;}
        if(!connected&&socket.hasPeer&&socket.peerIp==src&&socket.peerPort==sport)connected=&socket;
        if(!first&&!socket.hasPeer)first=&socket;
    }
    VSock* socket=sender?sender:connected?connected:first;
    if(socket){Dgram dgram;dgram.src=src;dgram.sport=sport;dgram.unreachable=true;socket->q.push_back(std::move(dgram));}
}
std::shared_ptr<VConn> findConn(uint32_t src,uint16_t sport,uint16_t dport,uint32_t cid){
    auto found=conns.find(cid);
    if(found==conns.end())return nullptr;
    auto conn=found->second.lock();
    if(!conn||conn->local||conn->rip!=src||conn->rport!=sport||conn->lport!=dport)return nullptr;
    return conn;
}
void incoming(const uint8_t* frame,size_t length){
    if(length<24)return;
    uint8_t kind=frame[0];
    uint32_t src=be32(frame+4),dst=be32(frame+8);
    uint16_t sport=be16(frame+12),dport=be16(frame+14);
    uint8_t op=frame[16];
    uint32_t cid=uint32_t(frame[20])|uint32_t(frame[21])<<8|uint32_t(frame[22])<<16|uint32_t(frame[23])<<24;
    const uint8_t* payload=frame+24;size_t payloadLength=length-24;
    std::lock_guard<std::mutex> lock(mu);
    if(!online())return;
    noteFrame();
    bool broadcast=broadcastAddress(dst);
    if(!broadcast&&dst!=vip)return;
    if(kind==KindUdp){
        if(op==UdpUnreach){deliverUnreachable(src,sport,dport);}
        else deliverUdp(src,sport,broadcast?0xffffffffu:vip,dport,payload,payloadLength,broadcast);
    }else if(kind==KindIcmp){
        if(payloadLength<8)return;
        for(auto& entry:socks){
            VSock& socket=entry.second;
            if(socket.used&&socket.icmp){
                Dgram dgram;dgram.src=src;dgram.data.assign(payload,payload+payloadLength);
                if(socket.q.size()<kDgramQueueMax&&socket.qBytes+payloadLength<=kDgramBytesMax){
                    socket.qBytes+=payloadLength;socket.q.push_back(std::move(dgram));
                }
            }
        }
        if(payload[0]==8&&!broadcast&&allowAuto(payloadLength)){
            std::vector<uint8_t> reply(payload,payload+payloadLength);
            reply[0]=0;reply[1]=0;reply[2]=reply[3]=0;
            uint32_t sum=0;
            for(size_t i=0;i+1<reply.size();i+=2)sum+=uint32_t(reply[i])<<8|reply[i+1];
            if(reply.size()&1)sum+=uint32_t(reply.back())<<8;
            while(sum>>16)sum=(sum&0xffff)+(sum>>16);
            sum=~sum&0xffff;reply[2]=uint8_t(sum>>8);reply[3]=uint8_t(sum);
            txRaw(KindIcmp,src,0,0,0,0,reply.data(),reply.size());
        }
    }else if(kind==KindTcp){
        if(op==TcpGone){
            for(auto& entry:conns){
                auto conn=entry.second.lock();
                if(conn&&!conn->local&&conn->rip==src){conn->rxRst=true;conn->rx.clear();conn->rxOff=0;}
            }
            changeCv.notify_all();
            return;
        }
        if(broadcast)return;
        auto conn=findConn(src,sport,dport,cid);
        switch(op){
        case TcpSyn:{
            if(conn)break;
            VSock* listener=findListener(vip,dport);
            if(!listener||listener->pending>=listener->backlog+4){
                if(allowAuto(0))txRaw(KindTcp,src,dport,sport,TcpRst,cid,nullptr,0);
                NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: tcp %u.%u.%u.%u:%u -> %u refused",
                    (src>>24)&255,(src>>16)&255,(src>>8)&255,src&255,sport,dport);
                break;
            }
            auto accepted=std::make_shared<VConn>();
            accepted->cid=cid;accepted->lip=vip;accepted->lport=dport;accepted->rip=src;accepted->rport=sport;
            accepted->state=VConn::Established;accepted->listener=listener;
            accepted->accNext.reset();
            if(listener->accTail)listener->accTail->accNext=accepted;else listener->accHead=accepted;
            listener->accTail=accepted;++listener->pending;
            conns[cid]=accepted;
            txRaw(KindTcp,src,dport,sport,TcpSynAck,cid,nullptr,0);
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Virtual LAN: tcp %u.%u.%u.%u:%u -> local port %u accepted",
                (src>>24)&255,(src>>16)&255,(src>>8)&255,src&255,sport,dport);
            break;}
        case TcpSynAck:
            if(conn&&conn->state==VConn::SynSent)conn->state=VConn::Established;
            else if(!conn||conn->state!=VConn::Established)txRaw(KindTcp,src,dport,sport,TcpRst,cid,nullptr,0);
            break;
        case TcpRst:
            if(conn){
                if(conn->state==VConn::SynSent)conn->state=VConn::Refused;
                conn->rxRst=true;conn->rx.clear();conn->rxOff=0;
            }
            break;
        case TcpData:
            if(!conn||conn->state!=VConn::Established||conn->rxRst){
                if(!conn||!conn->rstSent)txRaw(KindTcp,src,dport,sport,TcpRst,cid,nullptr,0);
                break;
            }
            if(conn->rxFin)break;
            if(conn->rx.size()+conn->rxOff+payloadLength>kStreamRxRemoteMax){
                sendRst(conn);conn->rxRst=true;conn->rx.clear();conn->rxOff=0;
                break;
            }
            conn->rx.insert(conn->rx.end(),payload,payload+payloadLength);
            break;
        case TcpFin:
            if(conn)conn->rxFin=true;
            break;
        default:break;
        }
    }
    changeCv.notify_all();
}
}  // namespace vlan
