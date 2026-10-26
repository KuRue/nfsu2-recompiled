#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
namespace nfs_physics {
struct Snapshot {
    bool valid=false;
    uint32_t anchor=0,race=0,clock=0;
    std::array<float,3> position{},velocity{},forward{},left{},up{};
    std::array<float,4> wheels{},suspension{};
    float rpm=0,throttle=0,brake=0,handbrake=0;
};
inline float dot(const std::array<float,3>& a,const std::array<float,3>& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline bool finiteRange(float v,float low,float high){return std::isfinite(v)&&v>=low&&v<=high;}
// Candidate PC v1.2 layout from DevCrashDay/speed2vr (MIT), see research notes.
// No guest reads occur outside the supplied checked reader. Never writes memory.
template<class Read> Snapshot readSnapshot(Read&& read){
    Snapshot s;uint32_t node=0,status=0;
    auto at=[&](uint32_t base,uint32_t off,void* out,size_t size){return uint64_t(base)+off+size<=0x100000000ull&&read(base+off,out,size);};
    if(!read(0x8900d8,&s.race,4)||!s.race||!read(0x832e94,&node,4)||node<4||node==0x832e94||
       !at(node,0x28,&s.anchor,4)||!s.anchor||(s.anchor&3))return s;
    if(!at(s.anchor,0x50,s.position.data(),12)||!at(s.anchor,0x70,s.velocity.data(),12)||
       !at(s.anchor,0x90,s.forward.data(),12)||!at(s.anchor,0xa0,s.left.data(),12)||!at(s.anchor,0xb0,s.up.data(),12)||
       !at(s.anchor,0x330,s.wheels.data(),16)||!at(s.anchor,0x380,s.suspension.data(),16)||
       !at(s.anchor,0x440,&s.rpm,4)||!at(s.anchor,0x438,&s.throttle,4))return s;
    for(auto v:s.position)if(!finiteRange(v,-100000,100000))return s;
    for(auto v:s.velocity)if(!finiteRange(v,-350,350))return s;
    for(auto* axis:{&s.forward,&s.left,&s.up}){
        for(float v:*axis)if(!finiteRange(v,-1.1f,1.1f))return s;
        if(std::fabs(dot(*axis,*axis)-1.f)>.12f)return s;
    }
    if(std::fabs(dot(s.forward,s.left))>.12f||std::fabs(dot(s.forward,s.up))>.12f||std::fabs(dot(s.left,s.up))>.12f)return s;
    for(float v:s.wheels)if(!finiteRange(v,-2500,2500))return s;
    for(float v:s.suspension)if(!finiteRange(v,-2,2))return s;
    if(!finiteRange(s.rpm,0,20000)||!finiteRange(s.throttle,0,1.1f))return s;
    uint32_t input=0;
    if(at(s.anchor,0x30,&input,4)&&input){
        if(!at(input,0x214,&s.brake,4)||!finiteRange(s.brake,0,1.1f))s.brake=0;
        if(!at(input,0x210,&s.handbrake,4)||!finiteRange(s.handbrake,0,1.1f))s.handbrake=0;
    }
    if(read(0x890118,&status,4)&&status)at(status,0x14,&s.clock,4);
    s.valid=true;return s;
}
}
