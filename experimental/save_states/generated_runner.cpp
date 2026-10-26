#include "snapshot.h"
#include <guest_memory.h>
#include <iostream>
#include "fixture_bytes.h"
#define RECOMP_GENERATED_CODE
#include <recomp_types.h>
uint32_t g_eax=0,g_ecx=0,g_edx=0,g_ebx=0,g_esp=0,g_ebp=0,g_esi=0,g_edi=0;
double g_st[8]{};int g_fp_top=0;uint16_t g_fpu_cw=0x27f;
uint16_t g_seg_cs=0,g_seg_ds=0,g_seg_es=0,g_seg_fs=0,g_seg_gs=0;
uint32_t g_fs_base=0,g_gs_base=0,g_cur_func=0,g_mxcsr=0x1f80;
uint64_t g_mm[8]{};V128 g_xmm[8]{};ptrdiff_t g_mem_base=0;
#include "fixture_continuations.h"
using namespace snapshot_prototype;
constexpr uint32_t base=0x400000,size=0x100000;
int main(int argc,char** argv){
    try{
        require(argc>=4,"mode/build/root required");std::string mode=argv[1],build=argv[2];auto root=uint32_t(std::stoul(argv[3]));
        GuestMemory memory;memory.commit(base,size);g_mem_base=reinterpret_cast<ptrdiff_t>(memory.base());
        State state;state.build=build;state.game=std::string(64,'5');state.flags=0x202;
        state.registers[4]=0x4f0000;state.frames.push_back({root,root,{0x202,0,FK_EFLAGS,0}});
        std::memcpy(memory.address(0x401000,sizeof fixture_bytes),fixture_bytes,sizeof fixture_bytes);
        uint32_t stop=0x400100;std::memcpy(memory.address(state.registers[4],4),&stop,4);
        if(mode=="restore"){
            require(argc==5,"restore path required");state=read(argv[4],build,std::string(64,'5'));
            require(state.regions.size()==1&&state.regions[0].address==base&&state.regions[0].bytes.size()==size,"Unexpected generated memory image");
            std::memcpy(memory.address(base,size),state.regions[0].bytes.data(),size);
        }
        unsigned steps=0,checkpoint=0;
        if(mode=="capture"){require(argc==6,"capture step/path required");checkpoint=unsigned(std::stoul(argv[4]));}
        else require(mode=="baseline"||mode=="restore","Unknown mode");
        while(!state.frames.empty()){
            if(mode=="capture"&&steps==checkpoint){
                Region region;region.address=base;region.bytes.resize(size);std::memcpy(region.bytes.data(),memory.address(base,size),size);state.regions={std::move(region)};
                writeAtomic(argv[5],state);return 0;
            }
            require(steps++<10000,"Generated fixture execution budget exceeded");generated_step(state);
        }
        require(mode!="capture","Checkpoint beyond completed program");
        Bytes image(size);std::memcpy(image.data(),memory.address(base,size),size);
        std::cout<<"{\"registers\":[";for(unsigned i=0;i<8;++i){if(i)std::cout<<',';std::cout<<state.registers[i];}
        std::cout<<"],\"flags\":"<<(state.flags&0xcd5u)<<",\"memory_crc\":"<<crc(image)<<",\"steps\":"<<steps<<"}\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
