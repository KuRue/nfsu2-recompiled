#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>

// Scalar vibration approximation of DirectInput forces. No wheel torque or
// synthetic collision/engine signals. All timing is monotonic milliseconds.
namespace nfs_ff {
struct Effect {
    uint32_t owner=0,type=0,gain=10000,durationUs=0,startDelayUs=0;
    int32_t magnitude=0,offset=0;
    uint32_t attackUs=0,fadeUs=0,attackLevel=0,fadeLevel=0;
    uint64_t start=0,end=0;
    bool playing=false;
};
class Mixer {
public:
    std::map<uint32_t,Effect> effects;
    uint32_t gain=10000;bool actuators=true,paused=false;
    void start(Effect& e,uint32_t iterations,uint64_t now){
        if(!iterations||!e.durationUs){e.playing=false;return;}
        e.start=now+(uint64_t(e.startDelayUs)+999)/1000;
        uint64_t duration=(uint64_t(e.durationUs)+999)/1000;
        bool infinite=e.durationUs==0xffffffffu||iterations==0xffffffffu;
        e.end=infinite?std::numeric_limits<uint64_t>::max():e.start+duration*iterations;
        e.playing=true;
    }
    void stopAll(uint32_t owner=0){for(auto& [id,e]:effects)if(!owner||e.owner==owner)e.playing=false;}
    unsigned sample(uint64_t now){
        unsigned maximum=0;
        if(!actuators||paused)return 0;
        for(auto& [id,e]:effects){
            if(!e.playing)continue;
            if(now>=e.end){e.playing=false;continue;}
            if(now<e.start)continue;
            // Condition forces describe a wheel's resistance, not a vibration.
            if(e.type>=0x13541c27u)continue;
            int64_t signedMagnitude=e.magnitude;
            uint64_t force=uint64_t(signedMagnitude<0?-signedMagnitude:signedMagnitude);
            int64_t off=e.offset;force+=uint64_t(off<0?-off:off);
            force=std::min<uint64_t>(force,10000);
            uint64_t age=now-e.start,remaining=e.end-now;
            if(e.attackUs&&age<(uint64_t(e.attackUs)+999)/1000){
                uint64_t span=(uint64_t(e.attackUs)+999)/1000;
                force=std::min<uint64_t>(force,e.attackLevel+(10000-std::min(e.attackLevel,10000u))*age/span);
            }
            if(e.fadeUs&&remaining<(uint64_t(e.fadeUs)+999)/1000){
                uint64_t span=(uint64_t(e.fadeUs)+999)/1000;
                force=std::min<uint64_t>(force,e.fadeLevel+(10000-std::min(e.fadeLevel,10000u))*remaining/span);
            }
            unsigned amplitude=unsigned(force*std::min(e.gain,10000u)*std::min(gain,10000u)*255/1000000000000ull);
            maximum=std::max(maximum,amplitude);
        }
        return maximum;
    }
};
}
