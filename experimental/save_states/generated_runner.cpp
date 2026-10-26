#include "snapshot.h"
#include <guest_memory.h>
#include <iostream>
#include "fixture_bytes.h"
#define RECOMP_GENERATED_CODE
#include <recomp_types.h>
// x86 guests permit unaligned data. Use byte-aligned alias types in this host
// harness, just as the pinned runtime already does on ARM hosts.
typedef uint16_t __attribute__((aligned(1),may_alias)) proto_u16;
typedef uint32_t __attribute__((aligned(1),may_alias)) proto_u32;
typedef uint64_t __attribute__((aligned(1),may_alias)) proto_u64;
typedef float __attribute__((aligned(1),may_alias)) proto_f32;
typedef double __attribute__((aligned(1),may_alias)) proto_f64;
#undef MEM16
#undef MEM32
#undef MEM64
#undef MEMF
#undef MEMD
#define MEM16(a) (*(volatile proto_u16*)ADDR(a))
#define MEM32(a) (*(volatile proto_u32*)ADDR(a))
#define MEM64(a) (*(volatile proto_u64*)ADDR(a))
#define MEMF(a) (*(volatile proto_f32*)ADDR(a))
#define MEMD(a) (*(volatile proto_f64*)ADDR(a))
uint32_t g_eax=0,g_ecx=0,g_edx=0,g_ebx=0,g_esp=0,g_ebp=0,g_esi=0,g_edi=0;
double g_st[8]{};int g_fp_top=0;uint16_t g_fpu_cw=0x27f;
uint16_t g_seg_cs=0,g_seg_ds=0,g_seg_es=0,g_seg_fs=0,g_seg_gs=0;
uint32_t g_fs_base=0,g_gs_base=0,g_cur_func=0,g_mxcsr=0x1f80;
uint64_t g_mm[8]{};V128 g_xmm[8]{};ptrdiff_t g_mem_base=0;
// Only a synthetic, snapshot-contained service; no external side effects.
inline void generated_host_step(snapshot_prototype::State& s){
    using snapshot_prototype::require;
    require(s.host[1]==1&&s.host[0]>=1&&s.host[0]<=2,"Unknown synthetic host phase");
    if(s.host[0]==1){
        uint32_t count=MEM32(0x480100u);MEM32(0x480100u)=count+1;
        s.host[4]=s.host[2]+3u*s.host[3];s.host[0]=2;return;
    }
    require(!s.frames.empty(),"Missing host continuation");
    uint32_t stack=s.registers[4];require(MEM32(stack)==s.frames.back().pc,"Host return mismatch");
    s.registers[0]=s.host[4];s.registers[4]+=4;s.host={};
}
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
                writeAtomic(argv[5],state);std::cout<<"{\"host_phase\":"<<state.host[0]<<"}\n";return 0;
            }
            require(steps++<10000,"Generated fixture execution budget exceeded");generated_step(state);
        }
        require(mode!="capture","Checkpoint beyond completed program");
        Bytes image(size);std::memcpy(image.data(),memory.address(base,size),size);
        std::cout<<"{\"registers\":[";for(unsigned i=0;i<8;++i){if(i)std::cout<<',';std::cout<<state.registers[i];}
        std::cout<<"],\"flags\":"<<(state.flags&0xcd5u)<<",\"memory_crc\":"<<crc(image)<<",\"steps\":"<<steps;
        std::cout<<",\"xmm\":[";for(unsigned i=0;i<32;++i){if(i)std::cout<<',';std::cout<<state.xmm[i];}
        std::cout<<"],\"fp_control\":"<<state.fpControl<<",\"mxcsr\":"<<state.mxcsr<<"}\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
