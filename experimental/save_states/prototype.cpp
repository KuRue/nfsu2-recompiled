#include "snapshot.h"
#include <guest_memory.h>
#include <iostream>
using namespace snapshot_prototype;
constexpr uint32_t address=0x400000;
// Handwritten guest-style integer fixture: not generated game code.
struct Machine {
    GuestMemory memory;
    State state;
    Machine(const std::string& build){
        state.build=build;state.game=std::string(64,'2');state.frames.push_back({1,0,{}});
        memory.commit(address,4096);std::memset(memory.address(address,4096),0,4096);
    }
    uint32_t ticks()const{uint32_t n;std::memcpy(&n,memory.address(address,4),4);return n;}
    void tick(){auto n=ticks()+1;std::memcpy(memory.address(address,4),&n,4);}
    void step(){
        require(!state.frames.empty(),"Fixture completed");auto& f=state.frames.back();
        if(f.function==1){
            if(f.pc==0){f.pc=1;state.frames.push_back({2,0,{}});return;}
            require(f.pc==1,"Invalid root continuation");state.registers[0]^=0x12345u;state.frames.pop_back();return;
        }
        if(f.function==2){
            if(f.pc==1){tick();f.pc=0;return;}
            require(f.pc==0,"Invalid loop continuation");
            if(f.locals[0]==100){state.frames.pop_back();return;}
            state.registers[1]=++f.locals[0];f.pc=1;state.frames.push_back({3,0,{}});return;
        }
        require(f.function==3,"Unknown fixture function");
        if(f.pc==0){state.registers[0]+=state.registers[1];state.flags=state.registers[0]&1;f.pc=1;return;}
        require(f.pc==1,"Invalid leaf continuation");state.registers[0]+=state.registers[1]*2;state.frames.pop_back();
    }
    bool checkpoint()const{return state.frames.size()==3&&state.frames.back().pc==1&&state.registers[1]==50;}
    State capture(){
        State out=state;Region r;r.address=address;r.bytes.resize(4096);std::memcpy(r.bytes.data(),memory.address(address,4096),4096);out.regions={std::move(r)};return out;
    }
    void restore(State saved){
        require(saved.regions.size()==1&&saved.regions[0].address==address&&saved.regions[0].bytes.size()==4096,"Unexpected fixture memory");
        std::memcpy(memory.address(address,4096),saved.regions[0].bytes.data(),4096);state=std::move(saved);
    }
    void finish(){unsigned budget=1000;while(!state.frames.empty()){require(budget-->0,"Fixture execution budget exceeded");step();}}
    void result(){std::cout<<"result="<<state.registers[0]<<" ticks="<<ticks()<<" flags="<<state.flags<<"\n";}
};
template<class F>void rejected(F&& f){bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}require(caught,"Invalid state unexpectedly accepted");}
void selftest(const std::string& build){
    Machine m(build);while(!m.checkpoint())m.step();auto state=m.capture();
    for(unsigned i=0;i<8;++i){state.fp[i]=0x8000000000000000ull+i;state.fpInt[i]=9007199254740993ull+i;state.mm[i]=0xdeadbeef00000000ull+i;}
    for(unsigned i=0;i<32;++i)state.xmm[i]=0x87654321u+i;
    state.fpTop=3;state.fpControl=0xf7f;state.mxcsr=0x3f80;state.host={2,1,7,11,40};state.frames.back().locals[4]=0xffffffffu;
    auto bytes=encode(state);
    auto decoded=decode(bytes,build,state.game);require(decoded.frames.size()==3&&decoded.registers==state.registers,"State round trip failed");
    require(decoded.fp==state.fp&&decoded.fpInt==state.fpInt&&decoded.mm==state.mm&&decoded.xmm==state.xmm&&decoded.fpTop==3&&decoded.fpControl==0xf7f&&decoded.mxcsr==0x3f80&&decoded.host==state.host&&decoded.frames.back().locals[4]==0xffffffffu,"Extended state round trip failed");
    for(size_t n=0;n<bytes.size();++n){Bytes cut(bytes.begin(),bytes.begin()+n);rejected([&]{decode(cut,build,state.game);});}
    auto damaged=bytes;damaged.back()^=1;rejected([&]{decode(damaged,build,state.game);});
    for(unsigned kind=0;kind<5;++kind){auto invalid=state;
        if(kind==0){invalid.fpTop=9;}if(kind==1){invalid.fpControl=0x10000;}if(kind==2){invalid.mxcsr=0x10000;}
        if(kind==3){invalid.host[0]=3;}if(kind==4){invalid.host[1]=99;}
        rejected([&]{encode(invalid);});
    }
    auto old=bytes;old[8]=1;rejected([&]{decode(old,build,state.game);});
    auto unknown=bytes;unknown[8]=99;rejected([&]{decode(unknown,build,state.game);});
    rejected([&]{decode(bytes,std::string(64,'3'),state.game);});
    rejected([&]{decode(bytes,build,std::string(64,'4'));});
    auto resource=state;resource.hostResources=1;rejected([&]{encode(resource);});
    auto overlap=state;overlap.regions.push_back(overlap.regions[0]);rejected([&]{encode(overlap);});
    auto wrap=state;wrap.regions[0].address=0xfffffff0u;rejected([&]{encode(wrap);});
    auto empty=state;empty.frames.clear();rejected([&]{encode(empty);});
    auto extra=bytes;extra.push_back(0);rejected([&]{decode(extra,build,state.game);});
    std::cout<<"Codec checks passed: all truncations, corruption, schema/build/game mismatch, unsupported host resources, overlap and overflow\n";
}
int main(int argc,char** argv){
    try{
        require(argc>=3,"mode and build identity required");std::string mode=argv[1],build=argv[2];
        if(mode=="selftest"){selftest(build);return 0;}
        Machine m(build);
        if(mode=="baseline"){m.finish();m.result();return 0;}
        require(argc==4,"snapshot path required");
        if(mode=="capture"||mode=="capture-unsupported"){
            while(!m.checkpoint()){m.step();}
            auto saved=m.capture();if(mode=="capture-unsupported")saved.hostResources=1;
            writeAtomic(argv[3],saved);
            std::cout<<"captured frames="<<m.state.frames.size()<<" ticks="<<m.ticks()<<" sum="<<m.state.registers[0]<<"\n";return 0;
        }
        require(mode=="restore","Unknown mode");m.restore(read(argv[3],build,std::string(64,'2')));m.finish();m.result();return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
