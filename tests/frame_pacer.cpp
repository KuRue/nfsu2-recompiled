#include "../android/app/src/main/cpp/frame_pacer.h"
#include <cstdio>
#include <cstdlib>
using namespace std::chrono;
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    using Clock=FramePacer::Clock;
    auto origin=Clock::time_point(seconds(100));
    for(unsigned limit:{30u,60u,90u,120u}){
        FramePacer pacer;auto time=origin;auto period=nanoseconds(1000000000ull/limit);
        require(pacer.deadline(time,limit)==time,"first frame must not sleep");
        for(unsigned frame=1;frame<=1000;++frame){
            auto ready=time+period/3;auto target=pacer.deadline(ready,limit);
            require(target==time+period,"fast frames obey the selected limit");time=target;
        }
        for(unsigned frame=0;frame<1000;++frame){
            time+=period+milliseconds(7);
            require(pacer.deadline(time,limit)==time,"CPU-limited frames must not gain periodic extra sleeps");
        }
        time+=seconds(5);
        require(pacer.deadline(time,limit)==time,"resume must not add a full frame of delay");
        require(pacer.deadline(time+milliseconds(1),0)==time+milliseconds(1),"uncapped mode never sleeps");
        require(pacer.deadline(time+milliseconds(2),limit)==time+milliseconds(2),"enabling a cap starts a fresh schedule");
        require(pacer.deadline(time+milliseconds(3),limit==30?60:30)==time+milliseconds(3),"changing caps starts a fresh schedule");
    }
    {
        FramePacer pacer;auto period=nanoseconds(1000000000ull/60);
        require(pacer.deadline(origin,60)==origin,"grid origin");
        auto late=origin+period+milliseconds(1);
        require(pacer.deadline(late,60)==late,"a slightly late frame does not sleep");
        auto early=origin+period*2-milliseconds(5);
        require(pacer.deadline(early,60)==origin+period*2,"the next deadline stays on the original grid");
    }
    {
        FramePacer pacer;auto period=nanoseconds(1000000000ull/60);
        require(pacer.deadline(origin,60)==origin,"resync origin");
        auto veryLate=origin+period*2+milliseconds(7);
        require(pacer.deadline(veryLate,60)==veryLate,"a frame a full period late does not sleep");
        require(pacer.deadline(veryLate+period/3,60)==veryLate+period,"resync does not burst the following frame");
    }
    {
        FramePacer pacer;auto period=nanoseconds(1000000000ull/60);
        require(pacer.deadline(origin,60)==origin,"slack origin");
        auto almost=origin+period-milliseconds(1);
        require(pacer.deadline(almost,60,0,milliseconds(2))==almost,"a wait under the slack is skipped");
        require(pacer.deadline(origin+period*2-milliseconds(5),60)==origin+period*2,"skipped slack still keeps the grid");
        FramePacer early;require(early.deadline(origin,60)==origin,"long-wait origin");
        require(early.deadline(origin+period/3,60,0,milliseconds(2))==origin+period,"a long wait is not skipped");
    }
    require(steadyFrameRate(90,120)==60,"90 FPS on 120 Hz paces at 60");
    require(steadyFrameRate(60,120)==60,"60 FPS on 120 Hz stays 60");
    require(steadyFrameRate(120,120)==120,"120 FPS on 120 Hz stays 120");
    require(steadyFrameRate(30,120)==30,"30 FPS on 120 Hz stays 30");
    require(steadyFrameRate(40,120)==40,"40 FPS on 120 Hz stays 40");
    require(steadyFrameRate(90,0)==90,"no refresh rate keeps the selected cap");
    {
        const unsigned modes[]={24u,60u,120u,144u};
        require(displayRefreshForContent(0,modes,4)==120,"uncapped uses the fastest mode at or under 120");
        require(displayRefreshForContent(30,modes,4)==60,"30 FPS uses 60 Hz");
        require(displayRefreshForContent(40,modes,4)==120,"40 FPS stays on 120 Hz");
        require(displayRefreshForContent(60,modes,4)==60,"60 FPS uses 60 Hz");
        require(displayRefreshForContent(90,modes,4)==120,"90 FPS has no even mode, so the panel stays at 120");
        require(displayRefreshForContent(120,modes,4)==120,"120 FPS uses 120 Hz");
        require(displayRefreshForContent(steadyFrameRate(90,120),modes,4)==60,"steady 90 on 120 Hz asks the panel for 60");
        const unsigned only120[]={120u};
        require(displayRefreshForContent(60,only120,1)==120,"a 120 Hz panel shows 60 FPS at 120");
        const unsigned with30[]={30u,60u,120u};
        require(displayRefreshForContent(30,with30,3)==30,"an exact 30 Hz mode is used");
        require(displayRefreshForContent(60,nullptr,0)==0,"no modes leaves the choice to the system");
    }
    {
        FramePacer pacer;auto vblank=nanoseconds(1000000000ull/120);auto step=vblank*2;
        require(pacer.deadline(origin,90,120)==origin,"refresh lock origin");
        require(pacer.deadline(origin+step/3,90,120)==origin+step,"90 FPS on 120 Hz sleeps on a 60 FPS grid");
    }
    std::puts("Frame pacing passed: grid lock, slack, refresh quantize, 30/60/90/120");
}
