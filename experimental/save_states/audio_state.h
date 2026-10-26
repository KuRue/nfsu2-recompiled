#pragma once
// Isolated logical audio mixer snapshot proof; never used by the installed game.
// Sample data is not saved: buffers are assets reloaded by ID and checked by CRC.
#include "snapshot.h"
#include <set>
namespace audio_snapshot {
using namespace snapshot_prototype;
constexpr uint32_t maxVoices=16,maxBuffers=16,block=256,maxBlocks=4096;
struct Buffer {uint32_t id=0;std::vector<int16_t> samples;};
struct Voice {uint32_t id=0,buffer=0,position=0,step=0x10000,volume=256,loop=0,playing=1;}; // position/step 16.16
struct Mixer {uint32_t blockIndex=0,nextVoice=1;std::vector<Voice> voices;};
inline uint32_t bufferCrc(const Buffer& b){Bytes bytes;for(auto s:b.samples){bytes.push_back(uint8_t(s));bytes.push_back(uint8_t(uint16_t(s)>>8));}return crc(bytes);}
inline const Buffer& find(const std::vector<Buffer>& buffers,uint32_t id){for(const auto& b:buffers)if(b.id==id)return b;throw std::runtime_error("Unknown audio buffer");}
inline void validate(const Mixer& m,const std::vector<Buffer>& buffers){
    require(m.voices.size()<=maxVoices&&m.blockIndex<=maxBlocks&&m.nextVoice,"Invalid mixer state");std::set<uint32_t> ids;
    for(const auto& v:m.voices){
        require(v.id&&v.id<m.nextVoice&&ids.insert(v.id).second,"Invalid voice handle");
        const auto& b=find(buffers,v.buffer);
        require(v.step&&v.step<=0x40000&&v.volume<=256&&v.loop<=1&&v.playing<=1,"Invalid voice parameters");
        require(uint64_t(v.position)<(uint64_t(b.samples.size())<<16)||!v.playing,"Voice position outside buffer");
    }
}
inline std::vector<int16_t> mix(Mixer& m,const std::vector<Buffer>& buffers){
    std::vector<int32_t> acc(block,0);
    for(auto& v:m.voices){if(!v.playing)continue;const auto& b=find(buffers,v.buffer);uint64_t end=uint64_t(b.samples.size())<<16;
        for(uint32_t i=0;i<block&&v.playing;++i){
            acc[i]+=b.samples[v.position>>16]*int32_t(v.volume)/256;
            uint64_t next=uint64_t(v.position)+v.step;
            if(next>=end){if(v.loop)next%=end;else{v.playing=0;next=0;}}
            v.position=uint32_t(next);
        }
    }
    std::vector<int16_t> out(block);for(uint32_t i=0;i<block;++i)out[i]=int16_t(std::clamp(acc[i],-32768,32767));
    ++m.blockIndex;return out;
}
inline Bytes encode(const Mixer& m,const std::vector<Buffer>& buffers){
    validate(m,buffers);Bytes payload;word(payload,m.blockIndex);word(payload,m.nextVoice);
    std::set<uint32_t> used;for(const auto& v:m.voices)used.insert(v.buffer);
    word(payload,uint32_t(used.size()));for(auto id:used){word(payload,id);word(payload,bufferCrc(find(buffers,id)));}
    word(payload,uint32_t(m.voices.size()));
    for(const auto& v:m.voices)for(auto n:{v.id,v.buffer,v.position,v.step,v.volume,v.loop,v.playing})word(payload,n);
    Bytes out{'N','F','S','A','U','D','I','O'};word(out,1);word(out,uint32_t(payload.size()));word(out,crc(payload));out.insert(out.end(),payload.begin(),payload.end());return out;
}
inline Mixer decode(const Bytes& bytes,const std::vector<Buffer>& buffers){
    require(bytes.size()<=4096,"Audio snapshot too large");Reader r{bytes};
    require(r.take(8)==Bytes({'N','F','S','A','U','D','I','O'})&&r.word()==1,"Wrong audio schema");
    auto size=r.word(),checksum=r.word();auto payload=r.take(size);require(r.offset==bytes.size()&&crc(payload)==checksum,"Corrupt audio envelope");
    Reader p{payload};Mixer m;m.blockIndex=p.word();m.nextVoice=p.word();auto count=p.word();require(count<=maxBuffers,"Too many buffers");
    std::set<uint32_t> listed;
    for(uint32_t i=0;i<count;++i){auto id=p.word(),sum=p.word();require(listed.insert(id).second,"Duplicate buffer reference");require(bufferCrc(find(buffers,id))==sum,"Audio asset changed");}
    count=p.word();require(count<=maxVoices,"Too many voices");
    for(uint32_t i=0;i<count;++i){Voice v;for(auto* n:{&v.id,&v.buffer,&v.position,&v.step,&v.volume,&v.loop,&v.playing})*n=p.word();require(listed.count(v.buffer),"Voice references unlisted buffer");m.voices.push_back(v);}
    require(p.offset==payload.size(),"Trailing audio state");validate(m,buffers);return m;
}
}
