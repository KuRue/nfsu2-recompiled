#pragma once
#include "frame_metrics.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

// A race-like frame on the October 2026 phone log had more than 30,000 D3D
// calls. Menus were quieter. This is a heuristic, not a scene detector.
inline constexpr unsigned kBenchmarkRaceCalls=30000;
inline constexpr unsigned kBenchmarkBusyStreak=3;
inline constexpr unsigned kBenchmarkWaitSeconds=180;

struct BenchmarkView {
    double p50=0,p95=0,p99=0,maximum=0,jitter=0;
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
    if(!view.limit){
        keep.action="inconclusive";keep.reason="Uncapped mode has no frame target. Choose a frame cap and run the benchmark again.";
        return keep;
    }
    double target=1000.0/view.limit;
    bool slow=view.p95>target*1.5||view.late*10>view.count;
    bool room=view.late==0&&view.p95<target*1.15&&view.jitter<=target*0.25;
    if(slow){
        int index=benchmarkResolutionIndex(resolution);
        if(index>=0&&index<4){
            BenchmarkAdvice advice;advice.action="lower-resolution";advice.resolution=benchmarkResolutionAt(index+1);
            advice.frameRate=setting;advice.reason="The frame cap was missed. One step down in resolution is the next thing to try.";
            return advice;
        }
        if(unsigned lower=benchmarkLowerFrameRate(setting)){
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

inline std::string formatBenchmarkReport(const BenchmarkView& view,const BenchmarkAdvice& advice){
    char line[512];
    std::string out="nfs-benchmark 1\n";
    auto add=[&](const char* key,const char* value){std::snprintf(line,sizeof line,"%s %s\n",key,value?value:"");out+=line;};
    auto addU=[&](const char* key,unsigned value){std::snprintf(line,sizeof line,"%s %u\n",key,value);out+=line;};
    auto addD=[&](const char* key,double value){std::snprintf(line,sizeof line,"%s %.2f\n",key,value);out+=line;};
    addU("count",view.count);addD("p50",view.p50);addD("p95",view.p95);addD("p99",view.p99);addD("max",view.maximum);
    addU("late",view.late);addD("jitter",view.jitter);addU("limit",view.limit);addU("selected",view.selected);
    addU("refresh",view.refresh);add("pace",view.steady?"steady":"exact");add("resolution",view.resolution);
    add("action",advice.action);add("next-resolution",advice.resolution);addU("next-fps",advice.frameRate);
    add("reason",advice.reason);
    return out;
}

// Waits for a busy frame, then keeps the intervals for a fixed stretch.
// The frame that crosses the busy threshold is not a sample: its interval
// still includes the menu. Later frames are, including hitch frames.
class BenchmarkRun {
public:
    using Clock=std::chrono::steady_clock;
    void arm(unsigned seconds){seconds_=seconds?seconds:1;phase_=Phase::Waiting;armedAt_={};started_={};busyStreak_=0;samples_=FrameMetrics<4096>{};}
    bool present(Clock::time_point now,double intervalMs,unsigned calls){
        if(phase_==Phase::Idle||phase_==Phase::Finished)return false;
        if(phase_==Phase::Waiting){
            if(armedAt_==Clock::time_point{})armedAt_=now;
            if(now-armedAt_>=std::chrono::seconds(kBenchmarkWaitSeconds)){phase_=Phase::Finished;return true;}
            if(calls<kBenchmarkRaceCalls){busyStreak_=0;return false;}
            // One heavy frame is often a load. Wait until several in a row.
            if(++busyStreak_<kBenchmarkBusyStreak)return false;
            phase_=Phase::Recording;started_=now;return false;
        }
        if(std::isfinite(intervalMs)&&intervalMs>0)samples_.add(intervalMs);
        if(now-started_>=std::chrono::seconds(seconds_)){phase_=Phase::Finished;return true;}
        return false;
    }
    const char* label() const {
        if(phase_==Phase::Waiting)return "Benchmark: drive. Recording starts after a few busy frames.";
        if(phase_==Phase::Recording)return "Benchmark: recording this stretch.";
        return nullptr;
    }
    bool finished() const {return phase_==Phase::Finished;}
    FrameMetrics<4096>& samples(){return samples_;}
private:
    enum class Phase {Idle,Waiting,Recording,Finished};
    Phase phase_=Phase::Idle;
    unsigned seconds_=0;
    Clock::time_point armedAt_{},started_{};
    unsigned busyStreak_=0;
    FrameMetrics<4096> samples_{};
};
