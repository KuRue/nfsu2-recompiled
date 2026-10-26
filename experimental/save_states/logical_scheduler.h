#pragma once
#include "snapshot.h"
#include <set>
namespace logical_snapshot {
using namespace snapshot_prototype;
constexpr uint32_t infinite=0xffffffffu,maxTime=1000000;
enum Status:uint32_t{Runnable,Waiting,Sleeping,Finished};
struct Thread{uint32_t id=0,status=Runnable,wait=0,deadline=0;State cpu;};
struct Event{uint32_t id=0,manual=0,signaled=0,signals=0;};
struct Timer{uint32_t id=0,event=0,due=0,period=0,active=1;};
struct Process{
    std::string build,game;
    uint32_t now=0,cursor=0,nextHandle=1;
    std::vector<Thread> threads;
    std::vector<Event> events;
    std::vector<Timer> timers;
};
inline Event& event(Process& p,uint32_t id){for(auto& e:p.events)if(e.id==id)return e;throw std::runtime_error("Unknown event handle");}
inline uint32_t after(uint32_t now,uint32_t delta){require(uint64_t(now)+delta<=maxTime,"Virtual timer overflow");return now+delta;}
inline void validateGraph(const Process& p){
    require(identity(p.build)&&identity(p.game),"Invalid process identity");
    require(!p.threads.empty()&&p.threads.size()<=8&&p.cursor<p.threads.size()&&p.now<=maxTime,"Invalid scheduler state");
    require(p.events.size()<=32&&p.timers.size()<=32,"Resource count limit");
    std::set<uint32_t> handles,ids;
    for(const auto& e:p.events){require(e.id&&handles.insert(e.id).second&&e.manual<=1&&e.signaled<=1&&e.signals<=maxTime,"Invalid event descriptor");}
    for(const auto& timer:p.timers){
        require(timer.id&&handles.insert(timer.id).second&&timer.active<=1&&timer.due<=maxTime&&timer.period<=maxTime,"Invalid timer descriptor");
        require(std::any_of(p.events.begin(),p.events.end(),[&](const Event& e){return e.id==timer.event;}),"Timer references unknown event");
        require(!timer.active||timer.due>=p.now,"Expired timer not processed");
    }
    require(p.nextHandle&& (handles.empty()||p.nextHandle>*handles.rbegin()),"Handle allocator state would reuse an ID");
    for(const auto& t:p.threads){
        require(t.id&&ids.insert(t.id).second&&t.status<=Finished,"Invalid logical thread");
        require(t.cpu.build==p.build&&t.cpu.game==p.game&&!t.cpu.frames.empty(),"Thread CPU identity/continuation mismatch");
        if(t.status==Finished){require(t.cpu.frames.size()==1&&!t.cpu.frames[0].function&&!t.cpu.frames[0].pc&&!t.cpu.host[0],"Invalid finished continuation");}
        else require(t.cpu.frames.back().function&&t.cpu.frames.back().pc,"Missing live continuation");
        if(t.status==Waiting){
            require(t.cpu.host[0]==1&&t.cpu.host[1]==2&&t.wait==t.cpu.host[2]&&handles.count(t.wait),"Invalid blocked event import");
            require(std::any_of(p.events.begin(),p.events.end(),[&](const Event& e){return e.id==t.wait;}),"Wait handle is not an event");
            require(t.deadline==infinite||(t.deadline>=p.now&&t.deadline<=maxTime),"Invalid wait deadline");
        }else if(t.status==Sleeping){require(t.cpu.host[0]==1&&t.cpu.host[1]==4&&!t.wait&&t.deadline>=p.now&&t.deadline<=maxTime,"Invalid sleep import");}
        else require(!t.wait&&!t.deadline,"Runnable/finished thread retains a wait");
    }
}
inline void ready(Thread& t,uint32_t result){t.status=Runnable;t.wait=t.deadline=0;t.cpu.host[0]=2;t.cpu.host[4]=result;}
inline void signal(Process& p,Event& e){
    e.signaled=1;++e.signals;
    // Thread vector is creation order, serialized unchanged: deterministic wake order.
    for(auto& t:p.threads)if(t.status==Waiting&&t.wait==e.id){ready(t,0);if(!e.manual){e.signaled=0;break;}}
}
inline void fire(Process& p){
    for(auto& timer:p.timers)if(timer.active&&timer.due<=p.now){signal(p,event(p,timer.event));if(timer.period)timer.due=after(timer.due,timer.period);else timer.active=0;}
    for(auto& t:p.threads){
        if(t.status==Waiting&&t.deadline!=infinite&&t.deadline<=p.now)ready(t,0x102);
        if(t.status==Sleeping&&t.deadline<=p.now)ready(t,0);
    }
}
inline void request(Process& p,Thread& t){
    auto& h=t.cpu.host;require(h[0]==1,"Synthetic host call not requested");
    if(h[1]==2){auto& e=event(p,h[2]);if(e.signaled){if(!e.manual)e.signaled=0;h[4]=0;h[0]=2;}
        else if(!h[3]){h[4]=0x102;h[0]=2;}
        else{t.status=Waiting;t.wait=e.id;t.deadline=h[3]==infinite?infinite:after(p.now,h[3]);}}
    else if(h[1]==3){signal(p,event(p,h[2]));h[4]=1;h[0]=2;}
    else if(h[1]==4){if(!h[2]){h[4]=0;h[0]=2;}else{t.status=Sleeping;t.deadline=after(p.now,h[2]);}}
    else throw std::runtime_error("Undeclared scheduler synthetic service");
}
inline bool finished(const Process& p){return std::all_of(p.threads.begin(),p.threads.end(),[](const Thread& t){return t.status==Finished;});}
template<class Step>void turn(Process& p,Step step){
    fire(p);
    for(size_t n=0;n<p.threads.size();++n){size_t index=(p.cursor+n)%p.threads.size();auto& t=p.threads[index];
        if(t.status!=Runnable)continue;
        p.cursor=uint32_t((index+1)%p.threads.size());step(t);
        if(t.cpu.frames.empty()){t.status=Finished;t.cpu.frames.push_back({0,0,{}});}
        p.now=after(p.now,1);fire(p);return;
    }
    uint32_t next=infinite;
    for(const auto& t:p.threads)if(t.status==Sleeping||(t.status==Waiting&&t.deadline!=infinite))next=std::min(next,t.deadline);
    for(const auto& timer:p.timers)if(timer.active)next=std::min(next,timer.due);
    require(next!=infinite&&next>=p.now,"Logical scheduler deadlock");p.now=next;fire(p);
}
inline void validateShared(const Process& p){
    const auto& memory=p.threads.front().cpu.regions;
    for(const auto& t:p.threads){require(t.cpu.regions.size()==memory.size(),"Threads disagree on shared memory");for(size_t i=0;i<memory.size();++i)require(t.cpu.regions[i].address==memory[i].address&&t.cpu.regions[i].bytes==memory[i].bytes,"Threads disagree on shared memory");}
}
inline Bytes encodeProcess(const Process& p){
    validateGraph(p);validateShared(p);Bytes payload;word(payload,p.now);word(payload,p.cursor);word(payload,p.nextHandle);
    word(payload,uint32_t(p.events.size()));for(const auto& e:p.events){word(payload,e.id);word(payload,e.manual);word(payload,e.signaled);word(payload,e.signals);}
    word(payload,uint32_t(p.timers.size()));for(const auto& t:p.timers){word(payload,t.id);word(payload,t.event);word(payload,t.due);word(payload,t.period);word(payload,t.active);}
    word(payload,uint32_t(p.threads.size()));
    for(const auto& t:p.threads){word(payload,t.id);word(payload,t.status);word(payload,t.wait);word(payload,t.deadline);auto cpu=encode(t.cpu);word(payload,uint32_t(cpu.size()));payload.insert(payload.end(),cpu.begin(),cpu.end());require(payload.size()<=maxBytes,"Process snapshot size limit");}
    Bytes out={'N','F','S','S','C','H','E','D'};word(out,1);word(out,uint32_t(payload.size()));word(out,crc(payload));out.insert(out.end(),payload.begin(),payload.end());return out;
}
inline Process decodeProcess(const Bytes& bytes,const std::string& build,const std::string& game){
    require(bytes.size()<=maxBytes+65536,"Process snapshot too large");Reader h{bytes};
    require(h.take(8)==Bytes({'N','F','S','S','C','H','E','D'})&&h.word()==1,"Unknown scheduler format");auto size=h.word(),checksum=h.word();require(size==bytes.size()-h.offset,"Scheduler snapshot size mismatch");auto payload=h.take(size);require(crc(payload)==checksum,"Scheduler checksum mismatch");Reader r{payload};Process p;p.build=build;p.game=game;
    p.now=r.word();p.cursor=r.word();p.nextHandle=r.word();auto count=r.word();require(count<=32,"Too many events");
    for(unsigned i=0;i<count;++i){Event e;e.id=r.word();e.manual=r.word();e.signaled=r.word();e.signals=r.word();p.events.push_back(e);}
    count=r.word();require(count<=32,"Too many timers");for(unsigned i=0;i<count;++i){Timer t;t.id=r.word();t.event=r.word();t.due=r.word();t.period=r.word();t.active=r.word();p.timers.push_back(t);}
    count=r.word();require(count>0&&count<=8,"Too many threads");
    for(unsigned i=0;i<count;++i){Thread t;t.id=r.word();t.status=r.word();t.wait=r.word();t.deadline=r.word();auto size=r.word();require(size<=maxBytes,"CPU snapshot too large");t.cpu=decode(r.take(size),build,game);p.threads.push_back(std::move(t));}
    require(r.offset==payload.size(),"Trailing scheduler state");validateGraph(p);
    validateShared(p);
    return p;
}
}
