#pragma once
#include "frame_metrics.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <array>
#include <algorithm>
#include <string>

// A race-like frame on the October 2026 phone log had more than 30,000 D3D
// calls. Menus were quieter. This is a heuristic, not a scene detector.
inline constexpr unsigned kBenchmarkRaceCalls=30000;
inline constexpr unsigned kBenchmarkBusyStreak=3;
inline constexpr unsigned kBenchmarkWaitSeconds=180;

struct BenchmarkView {
    double p50=0,p95=0,p99=0,maximum=0,jitter=0;
    double averageFps=0,lowFps=0; // mean rate, and the rate of the slowest 1% of frames
    double stalled=0;             // seconds the car was nearly stopped while recording
    unsigned late=0,count=0;
    unsigned limit=0;    // rate the samples were paced at; 0 is uncapped
    unsigned selected=0; // cap stored in the launcher; may be higher than limit
    unsigned refresh=0;
    bool steady=false;
    const char* resolution="";
};

struct BenchmarkAdvice {
    const char* action="inconclusive";
    const char* resolution="";
    unsigned frameRate=0;
    const char* reason="";
};

inline int benchmarkResolutionIndex(const char* resolution){
    static const char* ladder[]={"100","85","75","60","50"};
    for(int i=0;i<5;++i)if(resolution&&std::strcmp(resolution,ladder[i])==0)return i;
    return -1;
}
inline const char* benchmarkResolutionAt(int index){
    static const char* ladder[]={"100","85","75","60","50"};
    if(index<0||index>4)return nullptr;
    return ladder[index];
}
// Next lower cap on the launcher ladder. 0 means there is no lower step.
inline unsigned benchmarkLowerFrameRate(unsigned selected){
    static const unsigned ladder[]={120,90,60,40,30};
    int index=-1;
    for(int i=0;i<5;++i)if(ladder[i]==selected)index=i;
    if(index>=0)return index<4?ladder[index+1]:0u;
    unsigned best=0;for(unsigned rate:ladder)if(rate<selected&&rate>best)best=rate;
    return best;
}

inline BenchmarkAdvice adviseBenchmark(const BenchmarkView& view){
    const char* resolution=view.resolution?view.resolution:"";
    unsigned setting=view.selected?view.selected:view.limit;
    BenchmarkAdvice keep;keep.action="keep";keep.resolution=resolution;keep.frameRate=setting;
    if(view.count<30){
        keep.action="inconclusive";keep.reason="Fewer than 30 busy frames. Start a race and leave the benchmark running.";
        return keep;
    }
    // Uncapped runs are judged against the panel: it cannot show more frames than its refresh.
    unsigned goal=view.limit?view.limit:view.refresh;
    if(!goal){
        keep.action="inconclusive";keep.reason="Uncapped mode with an unknown refresh rate has no frame target. Choose a frame cap and run the benchmark again.";
        return keep;
    }
    double target=1000.0/goal;
    bool slow=view.p95>target*1.5||view.late*10>view.count;
    bool room=view.late==0&&view.p95<target*1.15&&view.jitter<=target*0.25;
    if(slow){
        int index=benchmarkResolutionIndex(resolution);
        if(index>=0&&index<4){
            BenchmarkAdvice advice;advice.action="lower-resolution";advice.resolution=benchmarkResolutionAt(index+1);
            advice.frameRate=setting;advice.reason="The frame cap was missed. One step down in resolution is the next thing to try.";
            return advice;
        }
        if(unsigned lower=benchmarkLowerFrameRate(setting?setting:goal)){
            BenchmarkAdvice advice;advice.action="lower-frame-rate";advice.resolution=resolution;
            advice.frameRate=lower;advice.reason="The frame cap was missed at this resolution. A lower cap is the next step.";
            return advice;
        }
        keep.reason="The frame cap was missed at the lowest resolution and 30 FPS.";
        return keep;
    }
    if(room){
        int index=benchmarkResolutionIndex(resolution);
        if(index>0){
            BenchmarkAdvice advice;advice.action="raise-resolution";advice.resolution=benchmarkResolutionAt(index-1);
            advice.frameRate=setting;advice.reason="The cap held with little jitter. One step up in resolution is the next thing to try.";
            return advice;
        }
        if(index==0){keep.reason="The cap held at native resolution. No change suggested.";return keep;}
        keep.reason="The cap held. This mode is not a percent scale, so the benchmark will not change it.";
        return keep;
    }
    keep.reason="The cap held. No change suggested.";
    return keep;
}

inline std::string formatBenchmarkReport(const BenchmarkView& view,const BenchmarkAdvice& advice,const std::string& timeline=""){
    char line[512];
    std::string out="nfs-benchmark 1\n";
    auto add=[&](const char* key,const char* value){std::snprintf(line,sizeof line,"%s %s\n",key,value?value:"");out+=line;};
    auto addU=[&](const char* key,unsigned value){std::snprintf(line,sizeof line,"%s %u\n",key,value);out+=line;};
    auto addD=[&](const char* key,double value){std::snprintf(line,sizeof line,"%s %.2f\n",key,value);out+=line;};
    addU("count",view.count);addD("p50",view.p50);addD("p95",view.p95);addD("p99",view.p99);addD("max",view.maximum);
    addD("average-fps",view.averageFps);addD("low-fps",view.lowFps);addD("stalled",view.stalled);
    addU("late",view.late);addD("jitter",view.jitter);addU("limit",view.limit);addU("selected",view.selected);
    addU("refresh",view.refresh);add("pace",view.steady?"steady":"exact");add("resolution",view.resolution);
    add("action",advice.action);add("next-resolution",advice.resolution);addU("next-fps",advice.frameRate);
    add("reason",advice.reason);
    if(!timeline.empty())out+="timeline "+timeline+"\n";
    return out;
}

// Waits for a busy frame, then keeps the intervals for a fixed stretch.
// The frame that crosses the busy threshold is not a sample: its interval
// still includes the menu. Later frames are, including hitch frames.
class BenchmarkRun;
inline std::string formatBenchmarkTimeline(const BenchmarkRun& run);
class BenchmarkRun {
public:
    using Clock=std::chrono::steady_clock;
    // One point of the run's timeline: a quarter second of frames.
    static constexpr unsigned kSlotMs=250;
    static constexpr unsigned kMaxSlots=4*120+8;
    struct Slot {float fps=0,minFps=0,maxFps=0,calls=0,renderMs=0;};
    void arm(unsigned seconds){
        seconds_=seconds?seconds:1;phase_=Phase::Waiting;armedAt_={};started_={};busyStreak_=0;samples_=FrameMetrics<8192>{};
        totalMs_=0;totalFrames_=0;slotCount_=0;open_={};lastMs_=0;stalledMs_=0;lastSpeedAt_={};
    }
    // Car speed in m/s while recording. Time below kStallSpeed counts as stalled: the AI
    // stuck on a wall drags the figures down, so the report flags it. The course itself slows
    // the car under 3 m/s for about 3.3 s every run (phone, 2026-10-27), so only a near stop
    // counts. Recording starts once the car passes kMovingSpeed after the countdown.
    static constexpr float kStallSpeed=1.f;
    static constexpr float kMovingSpeed=3.f;
    static constexpr double kStallLimitMs=3000;
    void speed(Clock::time_point now,float metresPerSecond){
        if(phase_!=Phase::Recording){lastSpeedAt_={};return;}
        if(lastSpeedAt_!=Clock::time_point{}&&std::isfinite(metresPerSecond)&&metresPerSecond<kStallSpeed)
            stalledMs_+=std::chrono::duration<double,std::milli>(now-lastSpeedAt_).count();
        lastSpeedAt_=now;
    }
    double stalledSeconds() const {return stalledMs_/1000.0;}
    bool stalled() const {return stalledMs_>kStallLimitMs;}
    // renderMs: CPU time the frame spent translating Direct3D calls (render thread).
    bool present(Clock::time_point now,double intervalMs,unsigned calls,double renderMs=0){
        if(phase_==Phase::Idle||phase_==Phase::Finished)return false;
        if(phase_==Phase::Waiting){
            if(armedAt_==Clock::time_point{})armedAt_=now;
            if(now-armedAt_>=std::chrono::seconds(kBenchmarkWaitSeconds)){phase_=Phase::Finished;return true;}
            if(calls<kBenchmarkRaceCalls){busyStreak_=0;return false;}
            // One heavy frame is often a load. Wait until several in a row.
            if(++busyStreak_<kBenchmarkBusyStreak)return false;
            phase_=Phase::Recording;started_=now;return false;
        }
        if(std::isfinite(intervalMs)&&intervalMs>0){
            samples_.add(intervalMs);totalMs_+=intervalMs;++totalFrames_;lastMs_=intervalMs;lastCalls_=calls;lastRender_=renderMs;
            unsigned slot=unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(now-started_).count()/kSlotMs);
            if(slot!=openIndex_&&open_.frames)closeSlot();
            openIndex_=slot;
            open_.frames++;open_.ms+=intervalMs;open_.slowest=std::max(open_.slowest,intervalMs);
            open_.fastest=open_.fastest>0?std::min(open_.fastest,intervalMs):intervalMs;open_.calls+=calls;open_.render+=renderMs;
        }
        if(now-started_>=std::chrono::seconds(seconds_)){if(open_.frames)closeSlot();phase_=Phase::Finished;return true;}
        return false;
    }
    struct Live {bool recording=false;double fps=0,frameMs=0,averageFps=0,lowFps=0,worstMs=0,elapsed=0,minFps=0,maxFps=0;unsigned total=0,calls=0;double renderMs=0;};
    Live live(Clock::time_point now) const {
        Live out;out.recording=phase_==Phase::Recording;out.total=seconds_;
        if(!out.recording)return out;
        out.elapsed=std::chrono::duration<double>(now-started_).count();
        if(slotCount_){const Slot& last=slots_[slotCount_-1];out.fps=last.fps;out.calls=unsigned(last.calls);out.renderMs=last.renderMs;}
        else if(lastMs_>0){out.fps=1000.0/lastMs_;out.calls=lastCalls_;out.renderMs=lastRender_;}
        out.frameMs=out.fps>0?1000.0/out.fps:0;
        out.averageFps=averageFps();
        auto worst=samples_.slowest((totalFrames_+99)/100);
        out.worstMs=worst.maximum;out.lowFps=worst.average>0?1000.0/worst.average:0;
        for(unsigned i=0;i<slotCount_;++i){
            if(!out.minFps||slots_[i].minFps<out.minFps)out.minFps=slots_[i].minFps;
            out.maxFps=std::max(out.maxFps,double(slots_[i].maxFps));
        }
        return out;
    }
    double averageFps() const {return totalFrames_&&totalMs_>0?1000.0*totalFrames_/totalMs_:0;}
    double onePercentLowFps() const {auto w=samples_.slowest((totalFrames_+99)/100);return w.average>0?1000.0/w.average:0;}
    const Slot* timeline() const {return slots_.data();}
    unsigned timelineCount() const {return slotCount_;}
    // The autopilot has taken the car: record from here. Busy frames stay a fallback.
    void begin(Clock::time_point now){if(phase_==Phase::Waiting){phase_=Phase::Recording;started_=now;openIndex_=0;}}
    bool finished() const {return phase_==Phase::Finished;}
    FrameMetrics<8192>& samples(){return samples_;}
private:
    struct Open {unsigned frames=0;double ms=0,slowest=0,fastest=0,calls=0,render=0;};
    void closeSlot(){
        if(slotCount_<kMaxSlots){
            Slot& slot=slots_[slotCount_++];
            slot.fps=float(1000.0*open_.frames/open_.ms);slot.minFps=float(1000.0/open_.slowest);slot.maxFps=float(1000.0/open_.fastest);
            slot.calls=float(open_.calls/open_.frames);slot.renderMs=float(open_.render/open_.frames);
        }
        open_={};
    }
    enum class Phase {Idle,Waiting,Recording,Finished};
    Phase phase_=Phase::Idle;
    unsigned seconds_=0;
    Clock::time_point armedAt_{},started_{};
    unsigned busyStreak_=0;
    FrameMetrics<8192> samples_{};
    double totalMs_=0;unsigned totalFrames_=0;
    std::array<Slot,kMaxSlots> slots_{};unsigned slotCount_=0,openIndex_=0;
    Open open_{};double lastMs_=0,lastRender_=0;unsigned lastCalls_=0;
    double stalledMs_=0;Clock::time_point lastSpeedAt_{};
};

// Report line: one "fps:min:max:render-ms:calls" point per quarter second, comma separated.
inline std::string formatBenchmarkTimeline(const BenchmarkRun& run){
    std::string out;char point[64];
    for(unsigned i=0;i<run.timelineCount();++i){
        const auto& slot=run.timeline()[i];
        std::snprintf(point,sizeof point,"%s%.1f:%.1f:%.1f:%.2f:%.0f",i?",":"",slot.fps,slot.minFps,slot.maxFps,slot.renderMs,slot.calls);
        out+=point;
    }
    return out;
}
