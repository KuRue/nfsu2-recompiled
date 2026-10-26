#pragma once
// Isolated logical graphics-device snapshot proof; never used by the installed game.
// Asset textures are recreated from their source and checked by CRC. Render-target textures
// (drawn at run time, no source) carry their pixels. No host GPU objects are ever saved.
#include "snapshot.h"
#include <set>
#include <functional>
namespace graphics_snapshot {
using namespace snapshot_prototype;
constexpr uint32_t maxTextures=16,maxSize=128,maxFrames=4096,screen=64;
enum Blend:uint32_t{Opaque,Alpha,Additive};
enum Origin:uint32_t{Asset,RenderTarget};
struct Texture {uint32_t id=0,width=0,height=0,origin=Asset,asset=0;std::vector<uint32_t> pixels;}; // RGBA8
struct State {uint32_t blend=Opaque,texture=0,target=0,scissor[4]{0,0,screen,screen},tint=0xffffffffu;};
struct Device {uint32_t frame=0,nextHandle=1;std::vector<Texture> textures;State state;};
inline uint32_t pixelCrc(const std::vector<uint32_t>& p){Bytes b;for(auto v:p)word(b,v);return crc(b);}
inline Texture& find(Device& d,uint32_t id){for(auto& t:d.textures)if(t.id==id)return t;throw std::runtime_error("Unknown texture handle");}
inline const Texture& find(const Device& d,uint32_t id){return find(const_cast<Device&>(d),id);}
inline void validate(const Device& d){
    require(d.frame<=maxFrames&&d.nextHandle&&d.textures.size()<=maxTextures,"Invalid device state");std::set<uint32_t> ids;
    for(const auto& t:d.textures){
        require(t.id&&t.id<d.nextHandle&&ids.insert(t.id).second,"Invalid texture handle");
        require(t.width&&t.height&&t.width<=maxSize&&t.height<=maxSize&&t.origin<=RenderTarget&&t.pixels.size()==size_t(t.width)*t.height,"Invalid texture descriptor");
        require(t.origin==Asset?t.asset!=0:t.asset==0,"Texture origin mismatch");
    }
    const auto& s=d.state;require(s.blend<=Additive,"Invalid blend state");
    require(!s.texture||ids.count(s.texture),"Bound texture missing");
    uint32_t w=screen,h=screen;
    if(s.target){const auto& t=find(d,s.target);require(t.origin==RenderTarget,"Bound target is not a render target");w=t.width;h=t.height;require(s.texture!=s.target,"Texture bound as source and target");}
    require(s.scissor[0]<=s.scissor[2]&&s.scissor[1]<=s.scissor[3]&&s.scissor[2]<=w&&s.scissor[3]<=h,"Scissor outside target");
}
inline uint32_t channel(uint32_t c,unsigned shift){return (c>>shift)&255;}
inline uint32_t pack(uint32_t r,uint32_t g,uint32_t b,uint32_t a){return r|(g<<8)|(b<<16)|(a<<24);}
// Draw an axis-aligned textured rectangle with the current state into target pixels.
inline void draw(Device& d,std::vector<uint32_t>& screenPixels,uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1){
    const auto& s=d.state;uint32_t width=screen;std::vector<uint32_t>* out=&screenPixels;
    if(s.target){auto& t=find(d,s.target);width=t.width;out=&t.pixels;}
    const Texture* tex=s.texture?&find(d,s.texture):nullptr;
    for(uint32_t y=std::max(y0,s.scissor[1]);y<std::min(y1,s.scissor[3]);++y)for(uint32_t x=std::max(x0,s.scissor[0]);x<std::min(x1,s.scissor[2]);++x){
        uint32_t src=tex?tex->pixels[((y-y0)*tex->height/(y1-y0))*tex->width+(x-x0)*tex->width/(x1-x0)]:0xffffffffu;
        uint32_t c[4];for(unsigned i=0;i<4;++i)c[i]=channel(src,i*8)*channel(s.tint,i*8)/255;
        auto& dst=(*out)[y*width+x];uint32_t r[4];
        for(unsigned i=0;i<4;++i){uint32_t old=channel(dst,i*8);
            r[i]=s.blend==Opaque?c[i]:s.blend==Alpha?(c[i]*c[3]+old*(255-c[3]))/255:std::min(255u,old+c[i]*c[3]/255);}
        dst=pack(r[0],r[1],r[2],r[3]);
    }
}
// Asset loader: (asset ID, width, height) -> pixels, as the game would reload from its files.
using Loader=std::function<std::vector<uint32_t>(uint32_t,uint32_t,uint32_t)>;
inline Bytes encode(const Device& d){
    validate(d);Bytes p;word(p,d.frame);word(p,d.nextHandle);
    const auto& s=d.state;for(auto n:{s.blend,s.texture,s.target,s.scissor[0],s.scissor[1],s.scissor[2],s.scissor[3],s.tint})word(p,n);
    word(p,uint32_t(d.textures.size()));
    for(const auto& t:d.textures){
        for(auto n:{t.id,t.width,t.height,t.origin,t.asset})word(p,n);
        if(t.origin==Asset)word(p,pixelCrc(t.pixels));else for(auto v:t.pixels)word(p,v);
    }
    Bytes out{'N','F','S','G','R','A','P','H'};word(out,1);word(out,uint32_t(p.size()));word(out,crc(p));out.insert(out.end(),p.begin(),p.end());return out;
}
inline Device decode(const Bytes& bytes,const Loader& load){
    require(bytes.size()<=20+44+maxTextures*(20+4*maxSize*maxSize),"Graphics snapshot too large");Reader r{bytes};
    require(r.take(8)==Bytes({'N','F','S','G','R','A','P','H'})&&r.word()==1,"Wrong graphics schema");
    auto size=r.word(),checksum=r.word();auto payload=r.take(size);require(r.offset==bytes.size()&&crc(payload)==checksum,"Corrupt graphics envelope");
    Reader p{payload};Device d;d.frame=p.word();d.nextHandle=p.word();auto& s=d.state;
    for(auto* n:{&s.blend,&s.texture,&s.target,&s.scissor[0],&s.scissor[1],&s.scissor[2],&s.scissor[3],&s.tint})*n=p.word();
    auto count=p.word();require(count<=maxTextures,"Too many textures");
    for(uint32_t i=0;i<count;++i){Texture t;for(auto* n:{&t.id,&t.width,&t.height,&t.origin,&t.asset})*n=p.word();
        require(t.width&&t.height&&t.width<=maxSize&&t.height<=maxSize&&t.origin<=RenderTarget,"Invalid texture descriptor");
        if(t.origin==Asset){auto sum=p.word();t.pixels=load(t.asset,t.width,t.height);require(t.pixels.size()==size_t(t.width)*t.height&&pixelCrc(t.pixels)==sum,"Texture asset changed");}
        else{t.pixels.resize(size_t(t.width)*t.height);for(auto& v:t.pixels)v=p.word();}
        d.textures.push_back(std::move(t));}
    require(p.offset==payload.size(),"Trailing graphics state");validate(d);return d;
}
}
