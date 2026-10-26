#pragma once
// NTXP v1 interchange format from th0mk/nfsu2-web tools/texpacks.py (MIT).
// Independent bounded reader; guest resources always keep their original dimensions.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace nfs_tex {
constexpr uint32_t DXT1=0x31545844, DXT3=0x33545844, DXT5=0x35545844, ARGB=21;
inline uint32_t pitch(uint32_t w,uint32_t fmt) {
    if(fmt==ARGB)return w*4;
    if(fmt==DXT1)return ((w+3)/4)*8;
    if(fmt==DXT3||fmt==DXT5)return ((w+3)/4)*16;
    return 0;
}
inline uint32_t rows(uint32_t h,uint32_t fmt){return fmt==ARGB?h:(h+3)/4;}
inline uint64_t bytes(uint32_t w,uint32_t h,uint32_t fmt,uint32_t levels){
    if(!w||!h||w>4096||h>4096||!levels||levels>13||!pitch(w,fmt))return 0;
    uint32_t maxLevels=1;for(uint32_t n=std::max(w,h);n>1;n>>=1)++maxLevels;
    if(levels>maxLevels)return 0;
    uint64_t result=0;for(uint32_t i=0;i<levels;++i)result+=uint64_t(pitch(std::max(1u,w>>i),fmt))*rows(std::max(1u,h>>i),fmt);
    return result;
}
struct Digest {uint32_t crc,adler;};
inline Digest digest(const uint8_t* pixels,uint32_t rowBytes,uint32_t rowCount,uint32_t stride){
    if(!pixels||!rowBytes||!rowCount||stride<rowBytes)throw std::invalid_argument("Invalid texture rows");
    static const auto table=[] {std::vector<uint32_t> t(256);for(uint32_t i=0;i<256;++i){auto c=i;for(unsigned b=0;b<8;++b)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));t[i]=c;}return t;}();
    uint32_t crc=~0u,a=1,b=0,count=0;
    for(uint32_t y=0;y<rowCount;++y)for(uint32_t x=0;x<rowBytes;++x){
        uint8_t v=pixels[size_t(y)*stride+x];crc=(crc>>8)^table[(crc^v)&255];a+=v;b+=a;
        if(++count==5552){a%=65521;b%=65521;count=0;}
    }
    return {~crc,((b%65521)<<16)|(a%65521)};
}
struct Entry {
    uint32_t crc,adler;uint16_t ow,oh;uint32_t ofmt;uint16_t w,h;
    uint32_t fmt,levels,offset,size,name;
};
static_assert(sizeof(Entry)==40,"NTXP entry size");
class Pack {
    std::ifstream file;
    uint64_t fileSize=0;
    std::set<uint64_t> shapes;
    static uint64_t shape(uint32_t w,uint32_t h,uint32_t f){return uint64_t(w)|(uint64_t(h)<<16)|(uint64_t(f)<<32);}
public:
    std::vector<Entry> entries;
    std::string path;
    void open(const std::string& name){
        file=std::ifstream(name,std::ios::binary);path=name;entries.clear();shapes.clear();
        if(!file)throw std::runtime_error("Texture pack not readable: "+name);
        file.seekg(0,std::ios::end);auto end=file.tellg();
        if(end<16||uint64_t(end)>512ull*1024*1024)throw std::runtime_error("Invalid texture pack size");
        fileSize=uint64_t(end);file.seekg(0);
        uint32_t head[4]{};file.read(reinterpret_cast<char*>(head),16);
        if(std::memcmp(head,"NTXP",4)||head[1]!=1||!head[2]||head[2]>65536||head[3])throw std::runtime_error("Invalid NTXP header");
        uint64_t first=16+uint64_t(head[2])*40;
        if(first>fileSize)throw std::runtime_error("Truncated NTXP index");
        entries.resize(head[2]);file.read(reinterpret_cast<char*>(entries.data()),std::streamsize(entries.size()*40));
        if(!file)throw std::runtime_error("Truncated NTXP entries");
        std::pair<uint32_t,uint32_t> previous{0,0};bool havePrevious=false;
        for(auto& e:entries){
            std::pair<uint32_t,uint32_t> key{e.crc,e.adler};
            if((havePrevious&&key<previous)||!bytes(e.ow,e.oh,e.ofmt,1)||!bytes(e.w,e.h,e.fmt,e.levels)||
               bytes(e.w,e.h,e.fmt,e.levels)!=e.size||e.offset<first||uint64_t(e.offset)+e.size>fileSize||
               e.w<e.ow||e.h<e.oh||uint64_t(e.w)*e.oh!=uint64_t(e.h)*e.ow)
                throw std::runtime_error("Invalid NTXP texture entry");
            previous=key;havePrevious=true;shapes.insert(shape(e.ow,e.oh,e.ofmt));
        }
    }
    bool mayMatch(uint32_t w,uint32_t h,uint32_t fmt)const{return shapes.count(shape(w,h,fmt));}
    const Entry* find(Digest hash,uint32_t w,uint32_t h,uint32_t fmt)const{
        auto key=std::make_pair(hash.crc,hash.adler);
        auto it=std::lower_bound(entries.begin(),entries.end(),key,[](const Entry& e,auto k){return std::make_pair(e.crc,e.adler)<k;});
        for(;it!=entries.end()&&it->crc==hash.crc&&it->adler==hash.adler;++it)
            if(it->ow==w&&it->oh==h&&it->ofmt==fmt)return &*it;
        return nullptr;
    }
    std::vector<uint8_t> read(const Entry& e){
        if(uint64_t(e.offset)+e.size>fileSize||e.size>128u*1024*1024)throw std::runtime_error("Texture payload exceeds limit");
        std::vector<uint8_t> data(e.size);file.clear();file.seekg(e.offset);file.read(reinterpret_cast<char*>(data.data()),e.size);
        if(!file)throw std::runtime_error("Texture pack payload read failed");
        return data;
    }
};
}
