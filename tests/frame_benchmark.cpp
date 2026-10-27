#include <cmath>
#include "../android/app/src/main/cpp/frame_benchmark.h"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace std::chrono;
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static BenchmarkView viewAt(const char* resolution,unsigned selected,unsigned paced,double p95,unsigned late,unsigned count,double jitter){
    BenchmarkView view;view.resolution=resolution;view.selected=selected;view.limit=paced;
    view.p50=16.7;view.p95=p95;view.p99=p95;view.maximum=p95;view.jitter=jitter;
    view.late=late;view.count=count;view.refresh=120;view.steady=true;return view;
}
int main(){
    require(std::strcmp(adviseBenchmark(viewAt("75",60,60,17,0,10,0.4)).action,"inconclusive")==0,"short run");
    {
        auto view=viewAt("75",0,0,17,0,100,0.4);view.refresh=0;
        require(std::strcmp(adviseBenchmark(view).action,"inconclusive")==0,"uncapped with no known refresh");
        view=viewAt("75",0,0,30,40,100,1);view.refresh=120;
        auto advice=adviseBenchmark(view);
        require(std::strcmp(advice.action,"lower-resolution")==0&&advice.frameRate==0,"uncapped below the panel steps resolution down");
        view=viewAt("50",0,0,30,40,100,1);view.refresh=120;advice=adviseBenchmark(view);
        require(std::strcmp(advice.action,"lower-frame-rate")==0&&advice.frameRate==90,"uncapped at the lowest resolution suggests a cap");
        view=viewAt("75",0,0,8.5,0,100,0.3);view.refresh=120;advice=adviseBenchmark(view);
        require(std::strcmp(advice.action,"raise-resolution")==0&&advice.frameRate==0,"uncapped at the panel rate has room");
    }
    {
        auto advice=adviseBenchmark(viewAt("75",90,60,30,20,100,1));
        require(std::strcmp(advice.action,"lower-resolution")==0&&std::strcmp(advice.resolution,"60")==0&&advice.frameRate==90,"slow 75% steps to 60% and keeps the selected cap");
    }
    {
        auto advice=adviseBenchmark(viewAt("75",60,60,17,11,100,0.2));
        require(std::strcmp(advice.action,"lower-resolution")==0,"more than 10% late frames step resolution down");
    }
    {
        auto advice=adviseBenchmark(viewAt("50",60,60,30,20,100,1));
        require(std::strcmp(advice.action,"lower-frame-rate")==0&&advice.frameRate==40,"50% steps the cap down before 720");
    }
    {
        auto advice=adviseBenchmark(viewAt("720",60,60,30,20,100,1));
        require(std::strcmp(advice.action,"lower-frame-rate")==0&&std::strcmp(advice.resolution,"720")==0&&advice.frameRate==40,"fixed 720 mode only steps the cap");
    }
    {
        auto advice=adviseBenchmark(viewAt("50",30,30,40,20,100,1));
        require(std::strcmp(advice.action,"keep")==0&&advice.frameRate==30,"lowest settings stay put");
    }
    {
        auto advice=adviseBenchmark(viewAt("75",60,60,17.2,0,120,0.4));
        require(std::strcmp(advice.action,"raise-resolution")==0&&std::strcmp(advice.resolution,"85")==0&&advice.frameRate==60,"headroom steps to 85%");
        BenchmarkView view=viewAt("75",60,60,17.2,0,120,0.4);view.p50=16.70;view.p99=18;view.maximum=19.5;
        auto text=formatBenchmarkReport(view,advice);
        const char* expected=
            "nfs-benchmark 1\n"
            "count 120\n"
            "p50 16.70\n"
            "p95 17.20\n"
            "p99 18.00\n"
            "max 19.50\n"
            "average-fps 0.00\n"
            "low-fps 0.00\n"
            "late 0\n"
            "jitter 0.40\n"
            "limit 60\n"
            "selected 60\n"
            "refresh 120\n"
            "pace steady\n"
            "resolution 75\n"
            "action raise-resolution\n"
            "next-resolution 85\n"
            "next-fps 60\n"
            "reason The cap held with little jitter. One step up in resolution is the next thing to try.\n";
        if(text!=expected){std::fprintf(stderr,"REPORT:\n%s",text.c_str());require(false,"report text");}
    }
    {
        auto advice=adviseBenchmark(viewAt("100",60,60,17,0,120,0.4));
        require(std::strcmp(advice.action,"keep")==0,"native resolution with headroom stays");
    }
    {
        auto advice=adviseBenchmark(viewAt("75",60,60,17,0,120,10));
        require(std::strcmp(advice.action,"keep")==0,"jitter blocks a resolution increase");
    }
    {
        BenchmarkRun run;auto start=BenchmarkRun::Clock::time_point(seconds(50));
        run.arm(20);
        require(!run.present(start,100,10),"menu frames do not start the sample");
        require(!run.present(start+seconds(1),50,kBenchmarkRaceCalls),"one busy frame does not start");
        require(!run.present(start+seconds(1)+milliseconds(16),16,40000),"two busy frames do not start");
        auto gate=start+seconds(1)+milliseconds(32);
        require(!run.present(gate,16,40000),"the frame that starts recording is not a sample");
        require(!run.present(gate+milliseconds(16),16,40000),"recording continues");
        require(run.present(gate+seconds(20),16,40000),"the window closes on time");
        require(run.samples().take(60).count==2,"two intervals inside the window");
        require(!run.present(start+seconds(22),16,40000),"a finished run does not record again");
    }
    {
        BenchmarkRun run;auto start=BenchmarkRun::Clock::time_point(seconds(10));
        run.arm(20);
        require(!run.present(start,5,100),"wait origin");
        require(!run.present(start+seconds(179),5,100),"still waiting");
        require(run.present(start+seconds(180),5,100),"give up when no busy frame arrives");
        require(run.samples().take(60).count==0,"a timeout has no samples");
    }
    {
        BenchmarkRun run;auto start=BenchmarkRun::Clock::time_point(seconds(5));
        run.arm(60);
        require(!run.live(start).recording,"no panel before recording");
        for(unsigned i=0;i<kBenchmarkBusyStreak;++i)run.present(start,16,40000);
        // 198 frames at 10 ms and 2 at 50 ms: the 1% low is the two slow frames.
        auto at=start;for(unsigned i=0;i<200;++i){at+=milliseconds(10);run.present(at,i<198?10:50,40000,1.5);}
        auto live=run.live(at);
        require(live.recording&&live.total==60,"panel records");
        require(std::fabs(live.averageFps-1000.0*200/2080)<0.01,"average rate");
        require(std::fabs(live.lowFps-20)<0.01&&live.worstMs==50,"1% low is the slowest frames");
        // Frames land 10 ms apart up to 2.00 s: slots 0..7 are closed, slot 8 is still open.
        require(run.timelineCount()==8,"quarter-second timeline");
        require(std::fabs(run.timeline()[0].fps-100)<0.01&&run.timeline()[0].calls==40000&&std::fabs(run.timeline()[0].renderMs-1.5)<0.01,"slot averages");
        require(live.minFps>0&&live.maxFps>=live.minFps,"min and max");
        auto text=formatBenchmarkTimeline(run);
        require(text.rfind("100.0:100.0:100.0:1.50:40000,",0)==0,"timeline text");
        require(std::fabs(run.onePercentLowFps()-20)<0.01,"report 1% low");
    }
    {
        BenchmarkRun run;auto start=BenchmarkRun::Clock::time_point(seconds(5));
        run.arm(60);
        require(!run.present(start,16,20000),"a quiet race frame does not start recording");
        run.begin(start+milliseconds(16));
        require(run.live(start+milliseconds(16)).recording,"autopilot starts recording");
        require(!run.present(start+milliseconds(32),16,20000),"recording a quiet race");
        require(run.present(start+milliseconds(16)+seconds(60),16,20000),"window closes after begin");
    }
    std::puts("Frame benchmark passed");
}
