#include "../android/app/src/main/cpp/preview_mode.h"
#include <cassert>
#include <iostream>
int main(){
    unsetenv("NFSU2_LAUNCHER_PREVIEW");
    assert(launcherPreviewAllowsOpen(0xc0000000u,2)); // Normal game's saves still work.
    setenv("NFSU2_LAUNCHER_PREVIEW","1",1);
    assert(launcherPreviewAllowsOpen(0x80000000u,3)); // Existing assets, read only.
    for(unsigned disposition=1;disposition<=5;++disposition){
        assert(!launcherPreviewAllowsOpen(0x40000000u,disposition));
        assert(!launcherPreviewAllowsOpen(0xc0000000u,disposition));
        if(disposition!=3)assert(!launcherPreviewAllowsOpen(0x80000000u,disposition));
    }
    PreviewFrameGate gate;
    for(unsigned i=0;i<200;++i){gate.observe(false,true,30000);gate.observe(true,false,30000);gate.observe(true,true,600);}
    assert(!gate.ready()); // Loading/HUD-only frames must not count.
    for(unsigned i=1;i<PreviewFrameGate::warmFrames;++i)gate.observe(true,true,24000);
    assert(!gate.ready());gate.observe(true,true,24000);assert(gate.ready());
    std::cout<<"Preview isolation and scene gating passed\n";
}
