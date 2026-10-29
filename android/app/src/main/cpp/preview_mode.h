#pragma once
#include <atomic>
#include <cstdlib>
inline bool launcherPreviewMode(){return std::getenv("NFSU2_LAUNCHER_PREVIEW")!=nullptr;}
// No write-capable or create/truncate opens in a preview, including OPEN_ALWAYS.
inline bool launcherPreviewAllowsOpen(unsigned access,unsigned disposition){
    return !launcherPreviewMode() || (!(access&0x40000000u)&&disposition==3);
}
