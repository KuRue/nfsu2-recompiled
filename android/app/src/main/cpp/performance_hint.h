#pragma once
#include "frame_pacer.h"
#include <cstdint>

// Whether one cycle of guest work should be told to the performance hint.
// The target is the paced present period. The sleep that fills the rest of
// the frame is not part of busyNanos. A gap longer than four periods is a
// pause or a load, not a cycle the hint should learn from. Uncapped work
// leaves capped false so the session can close.
struct PerformanceHintDecision {
    bool report=false;
    bool capped=false;
    uint64_t targetNanos=0;
    uint64_t actualNanos=0;
};
inline PerformanceHintDecision decidePerformanceHint(uint64_t busyNanos,unsigned limit,unsigned refreshHz){
    PerformanceHintDecision decision;
    decision.targetNanos=framePeriodNanos(limit,refreshHz);
    decision.capped=decision.targetNanos!=0;
    if(!decision.capped||busyNanos==0||busyNanos>decision.targetNanos*4)return decision;
    decision.actualNanos=busyNanos;
    decision.report=true;
    return decision;
}

// Android-only. Safe to call on the presenting guest thread, including while
// the machine lock is released. Other platforms do not link this.
void reportGuestPerformanceHint(uint64_t busyNanos,unsigned limit,unsigned refreshHz);
