#pragma once
#include <chrono>
#include <algorithm>

// How many display refreshes fit under the cap. 0 means the cap is not locked
// to a refresh rate, and the period is the truncated 1e9/limit nanoseconds.
inline unsigned steadyFrameInterval(unsigned limit,unsigned refreshHz){
    if(!limit||refreshHz<30||refreshHz>240)return 0;
    unsigned interval=(refreshHz+limit-1)/limit;
    return interval<1?1u:interval;
}
// Nanoseconds the pacer waits between presents. 0 when there is no cap.
// The division matches FramePacer: a 120 Hz vblank is 8333333 ns, so 90 FPS
// on that panel is two vblanks, 16666666 ns.
inline unsigned long long framePeriodNanos(unsigned limit,unsigned refreshHz){
    if(!limit)return 0;
    unsigned interval=steadyFrameInterval(limit,refreshHz);
    if(!interval)return std::max(1ull,1000000000ull/std::max(1u,limit));
    return std::max(1ull,1000000000ull/refreshHz)*interval;
}
// Highest rate at or under the cap that lands on whole refreshes.
// 90 on a 120 Hz panel is 60, because 90 does not divide 120.
inline unsigned steadyFrameRate(unsigned limit,unsigned refreshHz){
    unsigned interval=steadyFrameInterval(limit,refreshHz);
    if(!interval)return limit;
    unsigned rate=refreshHz/interval;
    return rate?rate:1u;
}
// Slowest mode that shows this content rate for an equal number of refreshes.
// Modes outside 30..120 Hz are ignored. Content 0 means uncapped and picks the
// fastest remaining mode. 40 fps on a 60/120 Hz panel is 120, because 40 does
// not divide 60. A rate with no even multiple uses the slowest mode that is
// still at least that fast. No usable mode returns 0.
inline unsigned displayRefreshForContent(unsigned contentFps,const unsigned* modes,unsigned count){
    unsigned fastest=0,even=0,covering=0;
    for(unsigned index=0;index<count;++index){
        unsigned hz=modes?modes[index]:0;
        if(hz<30||hz>120)continue;
        if(hz>fastest)fastest=hz;
        if(!contentFps||hz<contentFps)continue;
        if(hz%contentFps==0&&(!even||hz<even))even=hz;
        if(!covering||hz<covering)covering=hz;
    }
    if(!contentFps)return fastest;
    if(even)return even;
    return covering?covering:fastest;
}
// October 26 phone log: the idle frontend sat near 611 D3D calls and still
// presented at the cap. The cheap 3D frontend stayed under 2,800. Loading
// was about 9,000 and the lightest race window about 24,000. 8,000 is between
// those. Eight quiet frames avoid a one-frame dip; one busier frame restores
// the cap on the next present. 30 FPS is the floor, including while uncapped.
inline constexpr unsigned kMenuQuietCalls=8000;
inline constexpr unsigned kMenuQuietFrames=8;
inline constexpr unsigned kMenuFloorFps=30;
inline unsigned menuPresentLimit(unsigned paceLimit,unsigned calls,unsigned& quietFrames){
    unsigned cap=paceLimit?paceLimit:120u;
    if(calls>=kMenuQuietCalls||cap<=kMenuFloorFps){quietFrames=0;return paceLimit;}
    if(quietFrames<kMenuQuietFrames)++quietFrames;
    if(quietFrames<kMenuQuietFrames)return paceLimit;
    return kMenuFloorFps;
}

// A frame that is late by less than one period is presented now, and the next
// deadline stays on the original grid. Shifting the grid to "now" on every
// late frame is what walks the picture against the display.
// A frame a full period late, or later, starts a new grid. Replaying the old
// one would burst uncapped frames and then stall for a full period.
// slack skips a wait that is too short to trust: sleeping past the slot costs
// a whole refresh. The grid still advances from the scheduled point.
class FramePacer {
public:
    using Clock=std::chrono::steady_clock;
    Clock::time_point deadline(Clock::time_point now,unsigned limit,unsigned refreshHz=0,
            Clock::duration slack=Clock::duration::zero()){
        if(!limit){limit_=0;refresh_=0;next_={};return now;}
        unsigned refresh=refreshHz>=30&&refreshHz<=240?refreshHz:0;
        if(limit!=limit_||refresh!=refresh_){limit_=limit;refresh_=refresh;next_=now+period();return now;}
        auto step=period();
        if(slack<Clock::duration::zero())slack=Clock::duration::zero();
        if(slack>step/2)slack=step/2;
        auto scheduled=next_;
        if(now+slack<scheduled){next_=scheduled+step;return scheduled;}
        if(now<scheduled+step){next_=scheduled+step;return now;}
        next_=now+step;return now;
    }
private:
    Clock::duration period() const {
        unsigned interval=steadyFrameInterval(limit_,refresh_);
        if(!interval)return std::chrono::nanoseconds(std::max(1ull,1000000000ull/std::max(1u,limit_)));
        auto vblank=std::chrono::nanoseconds(std::max(1ull,1000000000ull/refresh_));
        return vblank*interval;
    }
    unsigned limit_=0,refresh_=0;
    Clock::time_point next_{};
};
