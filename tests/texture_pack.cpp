#include "../android/app/src/main/cpp/texture_pack.h"
#include <filesystem>
#include <iostream>
using namespace nfs_tex;
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){
    const uint8_t text[]="123456789";auto known=digest(text,9,1,9);
    require(known.crc==0xcbf43926u&&known.adler==0x091e01deu,"Hash differs from zlib reference");
    std::vector<uint8_t> padded(80,0xff),packed(64);
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<16;++x)padded[y*20+x]=packed[y*16+x]=uint8_t(y*16+x);
    auto a=digest(padded.data(),16,4,20),b=digest(packed.data(),16,4,16);
    require(a.crc==b.crc&&a.adler==b.adler,"Row padding changed texture identity");
    auto path=std::filesystem::temp_directory_path()/("nfs-pack-check-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".pak");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove(path,e);}} cleanup{path};
    Entry good{a.crc,a.adler,4,4,ARGB,8,8,ARGB,4,56,340,0};
    auto fixture=[&](Entry e,uint32_t version=1){
        std::ofstream file(path,std::ios::binary|std::ios::trunc);uint32_t head[]={0x5058544e,version,1,0};
        file.write(reinterpret_cast<char*>(head),16);file.write(reinterpret_cast<char*>(&e),40);
        std::vector<char> data(340,42);file.write(data.data(),data.size());
    };
    fixture(good);Pack pack;pack.open(path.string());
    require(pack.mayMatch(4,4,ARGB)&&!pack.mayMatch(8,8,ARGB),"Shape prefilter differs");
    auto entry=pack.find(a,4,4,ARGB);require(entry&&pack.read(*entry).size()==340,"Payload read differs");
    require(!pack.find({0,0},4,4,ARGB),"Unknown texture matched");
    auto rejected=[&](Entry e,uint32_t version=1){fixture(e,version);try{Pack bad;bad.open(path.string());}catch(const std::runtime_error&){return;}throw std::runtime_error("Malformed pack was accepted");};
    auto bad=good;bad.offset=0xfffffff0;rejected(bad);
    bad=good;bad.size=341;rejected(bad);
    bad=good;bad.levels=5;rejected(bad);
    bad=good;bad.fmt=123;rejected(bad);
    bad=good;bad.w=4097;rejected(bad);
    bad=good;bad.offset=16;rejected(bad);
    rejected(good,2);
    require(bytes(1,1,DXT1,1)==8&&bytes(5,3,DXT5,1)==32,"Compressed edge blocks differ");
    std::cout<<"NTXP tests passed: zlib hashes, row padding, lookup, mip layout and malformed bounds\n";
}
