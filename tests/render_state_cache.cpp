#include "../android/app/src/main/cpp/render_state_cache.h"
#include <cassert>
#include <cstdio>
using nfs_d3d9::RenderStateCache;
int main(){
    RenderStateCache cache;int nativeCalls=0;
    auto success=[&](){++nativeCalls;return int32_t(0);};
    auto failure=[&](){++nativeCalls;return int32_t(-1);};
    assert(cache.render(7,1,success)==0);
    cache.render(7,1,success);assert(nativeCalls==1&&cache.skipped==1);
    cache.render(7,2,success);assert(nativeCalls==2);
    // Stage/state pairs must not alias each other or the render-state bank.
    cache.stage(0,7,2,success);cache.stage(1,7,2,success);
    cache.stage(0,7,2,success);assert(nativeCalls==4);
    assert(cache.render(8,3,failure)==-1);
    cache.render(8,3,success);assert(nativeCalls==6);
    // Failed replacement invalidates the old entry too: driver state is unknown.
    cache.render(8,4,failure);cache.render(8,3,success);assert(nativeCalls==8);
    cache.invalidate();cache.render(7,2,success);assert(nativeCalls==9);
    // Equal values are still recorded into state blocks, and recording never
    // updates the live-state cache. End/Apply/reset must force native setters.
    cache.setRecording(true);
    cache.render(7,2,success);cache.render(7,2,success);assert(nativeCalls==11);
    cache.invalidate();cache.render(7,2,success);assert(nativeCalls==12);
    cache.setRecording(false);cache.render(7,2,success);assert(nativeCalls==13);
    cache.reset();cache.render(7,2,success);assert(nativeCalls==14);
    // Unsupported indices always reach the driver, preserving validation/errors.
    cache.render(256,1,success);cache.render(256,1,success);
    cache.stage(8,1,1,success);cache.stage(8,1,1,success);
    cache.stage(0,64,1,success);cache.stage(0,64,1,success);assert(nativeCalls==20);
    cache.render(154,0x7fa05000,success);cache.render(154,0x7fa05000,success);
    cache.render(181,0,success);cache.render(181,0,success);assert(nativeCalls==24);
    assert(cache.calls==cache.skipped+uint64_t(nativeCalls));
    std::puts("Render state cache passed");
}
