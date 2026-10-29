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
    std::cout<<"Preview isolation passed\n";
}
