#include "../android/app/src/main/cpp/performance_hint.h"
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    require(framePeriodNanos(0,120)==0,"uncapped has no period");
    require(framePeriodNanos(60,0)==16666666ull,"60 FPS without a refresh is 1e9/60");
    require(framePeriodNanos(60,120)==16666666ull,"60 FPS on 120 Hz is two vblanks");
    require(framePeriodNanos(90,120)==16666666ull,"90 FPS on 120 Hz is the 60 FPS period");
    require(framePeriodNanos(40,120)==24999999ull,"40 FPS on 120 Hz is three vblanks");
    require(framePeriodNanos(30,120)==33333332ull,"30 FPS on 120 Hz is four vblanks");
    auto early=decidePerformanceHint(8000000ull,60,120);
    require(early.capped&&early.report&&early.actualNanos==8000000ull&&early.targetNanos==16666666ull,"an early frame is reported against the paced period");
    auto edge=decidePerformanceHint(16666666ull*4,60,120);
    require(edge.report,"four periods is still a cycle");
    auto pause=decidePerformanceHint(16666666ull*4+1,60,120);
    require(pause.capped&&!pause.report,"a longer gap is a pause");
    auto idle=decidePerformanceHint(0,60,120);
    require(idle.capped&&!idle.report,"an empty sample is not reported");
    auto open=decidePerformanceHint(8000000ull,0,120);
    require(!open.capped&&!open.report&&open.targetNanos==0,"uncapped work closes the hint");
    std::puts("Performance hint passed");
}
