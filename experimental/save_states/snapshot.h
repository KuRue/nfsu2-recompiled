#pragma once
// Experimental continuation format, deliberately NOT an NFSU2 runtime save state.
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
namespace snapshot_prototype {
using Bytes=std::vector<uint8_t>;
constexpr size_t maxBytes=16*1024*1024;
struct Frame {uint32_t function=0,pc=0;std::array<uint32_t,5> locals{};};
struct Region {uint32_t address=0;Bytes bytes;};
struct State {
    std::string build,game;
    uint32_t hostResources=0; // Nonzero is unsupported: never silently lose resources.
    std::array<uint32_t,8> registers{};
    uint32_t flags=0;
    std::array<uint64_t,8> fp{},fpInt{},mm{};
    std::array<uint32_t,32> xmm{};
    uint32_t fpTop=0,fpControl=0x27f,mxcsr=0x1f80;
    std::array<uint32_t,5> host{}; // phase, synthetic service ID, inputs, result

    std::vector<Frame> frames;
    std::vector<Region> regions;
};
inline void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
inline bool identity(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline void validate(const State& s){
    require(identity(s.build)&&identity(s.game),"Invalid compatibility identity");
    require(s.fpTop<=8&&s.fpControl<=0xffff&&s.mxcsr<=0xffff,"Invalid extended CPU state");
    require(s.host[0]<=2&&(!s.host[0]?s.host==std::array<uint32_t,5>{}:s.host[1]==1),"Invalid pending synthetic host call");
    require(!s.hostResources,"Host resource serialization is not implemented");
    require(!s.frames.empty()&&s.frames.size()<=64,"Invalid continuation count");
    require(!s.regions.empty()&&s.regions.size()<=128,"Invalid region count");
    std::vector<std::pair<uint64_t,uint64_t>> spans;uint64_t total=0;
    for(const auto& r:s.regions){
        uint64_t end=uint64_t(r.address)+r.bytes.size();total+=r.bytes.size();
        require(r.address>=4096&&!r.bytes.empty()&&end<=0x100000000ull&&total<=maxBytes,"Invalid memory region");
        spans.emplace_back(r.address,end);
    }
    std::sort(spans.begin(),spans.end());
    for(size_t i=1;i<spans.size();++i)require(spans[i-1].second<=spans[i].first,"Overlapping memory regions");
}
inline void word(Bytes& b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(n>>(i*8)));}
struct Reader {
    const Bytes& bytes;size_t offset=0;
    uint32_t word(){require(offset<=bytes.size()&&bytes.size()-offset>=4,"Truncated snapshot");uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(bytes[offset++])<<(i*8);return n;}
    Bytes take(size_t n){require(offset<=bytes.size()&&n<=bytes.size()-offset,"Truncated snapshot");Bytes out(bytes.begin()+offset,bytes.begin()+offset+n);offset+=n;return out;}
};
inline uint32_t crc(const Bytes& bytes){uint32_t value=~0u;for(uint8_t b:bytes){value^=b;for(unsigned i=0;i<8;++i)value=(value>>1)^(0xedb88320u&uint32_t(-int(value&1)));}return ~value;}
inline Bytes encode(const State& s){
    validate(s);Bytes payload;
    payload.insert(payload.end(),s.build.begin(),s.build.end());payload.insert(payload.end(),s.game.begin(),s.game.end());
    word(payload,s.hostResources);for(auto n:s.registers)word(payload,n);word(payload,s.flags);
    for(const auto* values:{&s.fp,&s.fpInt,&s.mm})for(auto n:*values){word(payload,uint32_t(n));word(payload,uint32_t(n>>32));}
    for(auto n:s.xmm)word(payload,n);
    word(payload,s.fpTop);word(payload,s.fpControl);word(payload,s.mxcsr);
    for(auto n:s.host)word(payload,n);
    word(payload,uint32_t(s.frames.size()));
    for(const auto& f:s.frames){word(payload,f.function);word(payload,f.pc);for(auto n:f.locals)word(payload,n);}
    word(payload,uint32_t(s.regions.size()));
    for(const auto& r:s.regions){word(payload,r.address);word(payload,uint32_t(r.bytes.size()));payload.insert(payload.end(),r.bytes.begin(),r.bytes.end());}
    Bytes file={'N','F','S','P','R','O','T','O'};word(file,2);word(file,uint32_t(payload.size()));word(file,crc(payload));
    file.insert(file.end(),payload.begin(),payload.end());return file;
}
inline State decode(const Bytes& file,const std::string& build,const std::string& game){
    require(file.size()<=maxBytes+65536,"Snapshot too large");Reader header{file};
    require(header.take(8)==Bytes({'N','F','S','P','R','O','T','O'}),"Not a prototype snapshot");
    require(header.word()==2,"Unsupported snapshot schema");uint32_t size=header.word(),checksum=header.word();
    require(size==file.size()-header.offset,"Invalid snapshot size");Bytes payload=header.take(size);
    require(crc(payload)==checksum,"Snapshot checksum mismatch");Reader r{payload};State s;
    auto b=r.take(64),g=r.take(64);s.build.assign(b.begin(),b.end());s.game.assign(g.begin(),g.end());
    require(s.build==build&&s.game==game,"Snapshot build/game mismatch");s.hostResources=r.word();
    for(auto& n:s.registers){n=r.word();}
    s.flags=r.word();
    for(auto* values:{&s.fp,&s.fpInt,&s.mm})for(auto& n:*values){uint32_t low=r.word(),high=r.word();n=uint64_t(low)|(uint64_t(high)<<32);}
    for(auto& n:s.xmm){n=r.word();}
    s.fpTop=r.word();s.fpControl=r.word();s.mxcsr=r.word();
    for(auto& n:s.host){n=r.word();}
    auto count=r.word();require(count>0&&count<=64,"Invalid continuation count");
    for(unsigned i=0;i<count;++i){Frame f;f.function=r.word();f.pc=r.word();for(auto& n:f.locals)n=r.word();s.frames.push_back(f);}
    count=r.word();require(count>0&&count<=128,"Invalid region count");
    for(unsigned i=0;i<count;++i){Region region;region.address=r.word();auto n=r.word();require(n<=maxBytes,"Region too large");region.bytes=r.take(n);s.regions.push_back(std::move(region));}
    require(r.offset==payload.size(),"Unexpected trailing state");validate(s);return s;
}
inline void writeAtomic(const std::filesystem::path& path,const State& s){
    auto bytes=encode(s);std::string pattern=path.string()+".partial-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    int fd=mkstemp(name.data());require(fd>=0,"Cannot create snapshot staging file");
    try{
        size_t written=0;while(written<bytes.size()){auto n=::write(fd,bytes.data()+written,bytes.size()-written);require(n>0,"Cannot write snapshot");written+=size_t(n);}
        require(fsync(fd)==0,"Cannot sync snapshot");int closed=close(fd);fd=-1;require(closed==0,"Cannot close snapshot");
        require(rename(name.data(),path.c_str())==0,"Cannot publish snapshot");
        auto parent=path.parent_path();if(parent.empty())parent=".";
        int directory=open(parent.c_str(),O_RDONLY|O_DIRECTORY);require(directory>=0,"Cannot open snapshot directory");
        int synced=fsync(directory);close(directory);require(synced==0,"Cannot sync snapshot directory");
    }catch(...){if(fd>=0)close(fd);unlink(name.data());throw;}
}
inline State read(const std::filesystem::path& path,const std::string& build,const std::string& game){
    std::ifstream input(path,std::ios::binary|std::ios::ate);require(bool(input),"Cannot open snapshot");
    auto size=input.tellg();require(size>=0&&uint64_t(size)<=maxBytes+65536,"Invalid snapshot file size");
    Bytes bytes(static_cast<size_t>(size));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());require(bool(input),"Cannot read snapshot");return decode(bytes,build,game);
}
}
