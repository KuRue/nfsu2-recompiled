#include "../android/app/src/main/cpp/physics_probe.h"
#include <vector>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <iostream>
void check(bool yes){if(!yes)throw std::runtime_error("Physics probe validation regression");}
int main(){
    std::vector<uint8_t> data(0x920000);
    auto put=[&](uint32_t address,auto value){std::memcpy(data.data()+address,&value,sizeof value);};
    put(0x8900d8,0x8b0000u);put(0x832e94,0x8c0000u);put(0x8c0028,0x8d0000u);
    put(0x8d0090,1.f);put(0x8d00a4,1.f);put(0x8d00b8,1.f);put(0x8d0070,20.f);
    put(0x8d0330,65.f);put(0x8d0440,3000.f);put(0x8d0438,.5f);
    auto read=[&](uint32_t at,void* out,size_t size){if(uint64_t(at)+size>data.size())return false;std::memcpy(out,data.data()+at,size);return true;};
    auto s=nfs_physics::readSnapshot(read);check(s.valid&&s.anchor==0x8d0000&&s.velocity[0]==20&&s.wheels[0]==65);
    put(0x8d0070,std::numeric_limits<float>::quiet_NaN());check(!nfs_physics::readSnapshot(read).valid);put(0x8d0070,20.f);
    put(0x8d00a4,0.f);put(0x8d00a0,1.f);check(!nfs_physics::readSnapshot(read).valid);put(0x8d00a0,0.f);put(0x8d00a4,1.f);
    put(0x8d0330,100000.f);check(!nfs_physics::readSnapshot(read).valid);put(0x8d0330,65.f);
    put(0x832e94,0xfffffff0u);check(!nfs_physics::readSnapshot(read).valid);put(0x832e94,0x8c0000u);
    put(0x8900d8,0u);check(!nfs_physics::readSnapshot(read).valid);
    std::cout<<"Physics reader checks passed: pointer bounds/overflow, race gating, finite values, orthogonal body axes and wheel ranges\n";
}
