#pragma once
#include <array>
#include <bitset>
#include <cstdint>

namespace nfs_d3d9 {
// Scalar values only: no resource references, shader data, or guest pointers.
// The bridge invalidates on device switches, state-block Apply, reset and restore.
class RenderStateCache {
    static constexpr unsigned count=256+8*64;
    std::array<uint32_t,count> values{};
    std::bitset<count> valid;
    bool recording=false;
public:
    uint64_t calls=0, skipped=0;
    void invalidate(){valid.reset();}
    void setRecording(bool value){recording=value;invalidate();}
    void reset(){recording=false;invalidate();}
    // Only successful native setters populate the cache. Never suppress calls
    // while recording: setting the same value still adds it to the state block.
    template<class Setter> int32_t set(unsigned index,uint32_t value,Setter&& setter){
        ++calls;
        if(!recording&&index<count&&valid[index]&&values[index]==value){++skipped;return 0;}
        int32_t status=setter();
        if(!recording&&index<count){
            if(status>=0){values[index]=value;valid.set(index);}else valid.reset(index);
        }
        return status;
    }
    template<class Setter> int32_t render(unsigned state,uint32_t value,Setter&& setter){
        // POINTSIZE (154) and adaptive tessellation (180–183) can carry
        // vendor extension commands (depth resolve / coverage / depth bounds).
        // Keep those on the native path even when their bits repeat.
        const bool scalar=state<256&&state!=154&&!(state>=180&&state<=183);
        return set(scalar?state:count,value,static_cast<Setter&&>(setter));
    }
    template<class Setter> int32_t stage(unsigned stage,unsigned state,uint32_t value,Setter&& setter){
        return set(stage<8&&state<64?256+stage*64+state:count,value,static_cast<Setter&&>(setter));
    }
};
}
