#include "cpu_test_runtime.h"
#include "logical_scheduler.h"
#include "fixture_bytes.h"
using namespace logical_snapshot;
static Process* activeProcess=nullptr;
static Thread* activeThread=nullptr;
inline void generated_host_step(State& s){
    require(activeProcess&&activeThread && &activeThread->cpu==&s,"Missing logical host context");
    if(s.host[0]==1){request(*activeProcess,*activeThread);return;}
    require(s.host[0]==2&&!s.frames.empty(),"Invalid logical host return");
    uint32_t stack=s.registers[4];require(MEM32(stack)==s.frames.back().pc,"Logical host return mismatch");
    s.registers[0]=s.host[4];s.registers[4]+=4;s.host={};
}
#include "fixture_continuations.h"
constexpr uint32_t base=0x400000,size=0x100000;
Process capture(Process p,GuestMemory& memory){
    Region region;region.address=base;region.bytes.resize(size);std::memcpy(region.bytes.data(),memory.address(base,size),size);
    for(auto& t:p.threads){t.cpu.regions={region};}return p;
}
Process initial(const std::string& build,const std::string& scenario,GuestMemory& memory){
    Process p;p.build=build;p.game=std::string(64,'6');p.events.push_back({1,scenario=="manual_event",0,0});p.nextHandle=2;
    std::vector<uint32_t> roots;
    if(scenario=="auto_event")roots={entry_waiter,entry_signaler};
    else if(scenario=="manual_event")roots={entry_waiter,entry_waiter,entry_signaler};
    else if(scenario=="timer"||scenario=="periodic"){roots={scenario=="timer"?entry_waiter:entry_twice_waiter};p.timers.push_back({2,1,20,scenario=="timer"?0u:20u,1});p.nextHandle=3;}
    else if(scenario=="timeout")roots={entry_timeout_waiter};
    else if(scenario=="sleep")roots={entry_sleeper};
    else if(scenario=="deadlock")roots={entry_waiter};
    else throw std::runtime_error("Unknown scheduler scenario");
    std::memcpy(memory.address(0x401000,sizeof fixture_bytes),fixture_bytes,sizeof fixture_bytes);
    for(unsigned i=0;i<roots.size();++i){Thread t;t.id=i+1;t.cpu.build=build;t.cpu.game=p.game;t.cpu.flags=0x202;t.cpu.registers[4]=0x4f0000-i*0x1000;t.cpu.registers[7]=0x480000+i*0x20;t.cpu.frames.push_back({roots[i],roots[i],{0x202,0,FK_EFLAGS,0}});uint32_t stop=0x400100;std::memcpy(memory.address(t.cpu.registers[4],4),&stop,4);p.threads.push_back(t);}
    return p;
}
template<class F>void rejected(F f){bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}require(caught,"Malformed process state accepted");}
void selftest(const std::string& build,GuestMemory& memory){
    auto p=capture(initial(build,"timer",memory),memory);auto bytes=encodeProcess(p);auto copy=decodeProcess(bytes,build,p.game);require(copy.timers[0].event==1&&copy.nextHandle==3,"Logical handles not reconstructed");
    for(unsigned kind=0;kind<9;++kind){auto invalid=p;
        if(kind==0){invalid.events.push_back(invalid.events[0]);}
        if(kind==1){invalid.timers[0].event=99;}
        if(kind==2){invalid.nextHandle=1;}
        if(kind==3){invalid.cursor=10;}
        if(kind==4){invalid.threads.push_back(invalid.threads[0]);}
        if(kind==5){invalid.threads[0].status=Waiting;invalid.threads[0].wait=99;}
        if(kind==6){invalid.now=21;}
        if(kind==7){invalid.events[0].manual=2;}
        if(kind==8){auto& t=invalid.threads[0];t.status=Waiting;t.wait=2;t.deadline=infinite;t.cpu.host={1,2,2,infinite,0};}
        rejected([&]{encodeProcess(invalid);});
    }
    for(size_t n:{size_t(0),size_t(19),bytes.size()-1}){auto cut=Bytes(bytes.begin(),bytes.begin()+n);rejected([&]{decodeProcess(cut,build,p.game);});}
    // Recompute the checksum after changing graph fields: decoder must validate
    // semantics, not merely reject accidental corruption.
    auto put=[](Bytes& b,size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)b[offset+i]=uint8_t(value>>(i*8));};
    for(auto offset:{size_t(24),size_t(28),size_t(60),size_t(84)}){
        auto forged=bytes;put(forged,offset,offset==28?1:99);Bytes body(forged.begin()+20,forged.end());put(forged,16,crc(body));
        rejected([&]{decodeProcess(forged,build,p.game);});
    }
    auto poll=p;poll.threads[0].cpu.host={1,2,1,0,0};request(poll,poll.threads[0]);
    require(poll.threads[0].status==Runnable&&poll.threads[0].cpu.host[4]==0x102,"Zero-time wait should not block");
    auto corrupt=bytes;corrupt.back()^=1;rejected([&]{decodeProcess(corrupt,build,p.game);});
    rejected([&]{decodeProcess(bytes,std::string(64,'1'),p.game);});
    auto inconsistent=capture(initial(build,"auto_event",memory),memory);inconsistent.threads[1].cpu.regions[0].bytes[0]^=1;
    rejected([&]{encodeProcess(inconsistent);});
    std::cout<<"Logical scheduler graph/handle validation passed\n";
}
int main(int argc,char** argv){try{
    require(argc>=4,"mode/build/scenario required");std::string mode=argv[1],build=argv[2],scenario=argv[3];GuestMemory memory;memory.commit(base,size);g_mem_base=reinterpret_cast<ptrdiff_t>(memory.base());
    if(mode=="selftest"){selftest(build,memory);return 0;}
    auto p=initial(build,scenario,memory);
    if(mode=="restore"){require(argc==5,"restore path required");p=decodeProcess(readBytes(argv[4]),build,p.game);const auto& regions=p.threads[0].cpu.regions;require(regions.size()==1&&regions[0].address==base&&regions[0].bytes.size()==size,"Unexpected shared memory layout");std::memcpy(memory.address(base,size),regions[0].bytes.data(),size);}
    unsigned turns=0,checkpoint=0;if(mode=="capture"){require(argc==6,"capture turn/path required");checkpoint=unsigned(std::stoul(argv[4]));}else require(mode=="baseline"||mode=="restore","Unknown mode");
    while(!finished(p)){
        if(mode=="capture"&&turns==checkpoint){auto state=capture(p,memory);writeAtomicBytes(argv[5],encodeProcess(state));std::cout<<"{\"statuses\":[";for(size_t i=0;i<p.threads.size();++i){if(i)std::cout<<',';std::cout<<p.threads[i].status;}std::cout<<"]}\n";return 0;}
        require(turns++<1000,"Scheduler budget exceeded");turn(p,[&](Thread& t){activeProcess=&p;activeThread=&t;generated_step(t.cpu);activeProcess=nullptr;activeThread=nullptr;});
    }
    require(mode!="capture","Checkpoint beyond completed process");auto final=capture(p,memory);auto bytes=encodeProcess(final);
    std::cout<<"{\"state_crc\":"<<crc(bytes)<<",\"time\":"<<p.now<<",\"turns\":"<<turns<<",\"signals\":"<<p.events[0].signals<<",\"event_set\":"<<p.events[0].signaled<<",\"counter\":"<<MEM32(0x480100u)<<",\"results\":[";
    for(size_t i=0;i<p.threads.size();++i){if(i)std::cout<<',';std::cout<<MEM32(0x480000+uint32_t(i)*0x20);}std::cout<<"],\"fp_outputs\":[";
    for(size_t i=0;i<p.threads.size();++i){if(i)std::cout<<',';uint64_t bits=MEM64(0x480008+uint32_t(i)*0x20);std::cout<<bits;}std::cout<<"]}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
