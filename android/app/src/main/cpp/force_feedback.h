#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>
#include <vector>
#include <cmath>

// Scalar vibration approximation of DirectInput forces. No wheel torque or
// synthetic collision/engine signals. All timing is monotonic milliseconds.
namespace nfs_ff {
struct Effect {
    uint32_t owner=0,type=0,gain=10000,durationUs=0,startDelayUs=0;
    int32_t magnitude=0,offset=0;
    uint32_t attackUs=0,fadeUs=0,attackLevel=0,fadeLevel=0;
    uint64_t start=0,end=0;
    int32_t conditionForce=0,lastConditionForce=0;
    unsigned lastCaptured=0;bool conditionSeen=false;
    bool playing=false;
};
class Mixer {
public:
    std::map<uint32_t,Effect> effects;
    struct Cue {uint32_t owner;uint64_t start,end;unsigned level;};
    std::vector<Cue> cues;
    bool distinctImpacts=true;
    void cue(uint32_t owner,uint64_t when,unsigned level){
        if(!level||!actuators||paused)return;
        bool impact=distinctImpacts&&level>=160;
        // Don't restart a strong pattern on every force update while scraping.
        if(impact)for(auto& p:cues)if(p.owner==owner&&p.level>=160&&when>=p.start&&when<p.end+80){p.level=std::max(p.level,std::min(level,255u));return;}
        if(cues.size()>=32)cues.erase(cues.begin());
        cues.push_back({owner,when,when+(impact?250u:85u),std::min(level,255u)});
    }
    unsigned scaled(const Effect& e,uint64_t force)const{return unsigned(std::min<uint64_t>(force,10000)*std::min(e.gain,10000u)*std::min(gain,10000u)*255/1000000000000ull);}
    void parametersChanged(Effect& e,uint64_t now){
        if(e.type>=0x13541c27u){
            int64_t delta=int64_t(e.conditionForce)-e.lastConditionForce;if(delta<0)delta=-delta;
            if(e.playing&&e.conditionSeen&&delta>=300)cue(e.owner,now,scaled(e,std::min<int64_t>(4500,delta*2)));
            e.lastConditionForce=e.conditionForce;e.conditionSeen=true;return;
        }
        int64_t m=e.magnitude,o=e.offset;unsigned level=scaled(e,uint64_t(m<0?-m:m)+uint64_t(o<0?-o:o));
        if(e.playing&&level>e.lastCaptured&&!e.attackUs)cue(e.owner,std::max(now,e.start),level);
        e.lastCaptured=level;
    }
    uint32_t gain=10000;bool actuators=true,paused=false;
    void start(Effect& e,uint32_t iterations,uint64_t now){
        if(!iterations||!e.durationUs){e.playing=false;return;}
        e.start=now+(uint64_t(e.startDelayUs)+999)/1000;
        uint64_t duration=(uint64_t(e.durationUs)+999)/1000;
        bool infinite=e.durationUs==0xffffffffu||iterations==0xffffffffu;
        e.end=infinite?std::numeric_limits<uint64_t>::max():e.start+duration*iterations;
        e.playing=true;e.lastCaptured=0;parametersChanged(e,now);
    }
    void stopAll(uint32_t owner=0){for(auto& [id,e]:effects)if(!owner||e.owner==owner)e.playing=false;
        cues.erase(std::remove_if(cues.begin(),cues.end(),[&](const Cue& p){return !owner||p.owner==owner;}),cues.end());}
    unsigned sample(uint64_t now,float steering=0.f){
        unsigned maximum=0;
        if(!actuators||paused||!gain)return 0;
        cues.erase(std::remove_if(cues.begin(),cues.end(),[&](const Cue& p){return p.end<=now;}),cues.end());
        const Cue* impact=nullptr;
        for(const auto& p:cues)if(now>=p.start&&distinctImpacts&&p.level>=160&&(!impact||p.level>impact->level))impact=&p;
        if(impact){
            uint64_t age=now-impact->start;
            // Two pronounced taps with a quiet gap, overriding ongoing road hum.
            return (age>=30&&age<105)||(age>=165&&age<240)?impact->level:0;
        }
        for(const auto& cue:cues)if(now>=cue.start)maximum=std::max(maximum,cue.level);
        for(auto& [id,e]:effects){
            if(!e.playing)continue;
            if(now>=e.end){e.playing=false;continue;}
            if(now<e.start)continue;
            // A light grip cue approximates wheel resistance during cornering.
            // This is not a tire-slip telemetry measurement.
            if(e.type>=0x13541c27u){
                float turn=std::min(1.f,std::fabs(steering));
                uint64_t grip=turn>.12f?uint64_t(std::abs(int64_t(e.conditionForce))*.45f*std::sqrt((turn-.12f)/.88f)):0;
                maximum=std::max(maximum,scaled(e,grip));continue;
            }
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
