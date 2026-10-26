#include "../android/app/src/main/cpp/frame_metrics.h"
#include <cassert>
#include <limits>
int main(){
    FrameMetrics m;assert(m.take(60).count==0);
    for(unsigned i=1;i<=120;++i)m.add(i);
    m.add(999);auto s=m.take(60);
    assert(s.count==120&&s.p50==60&&s.p95==114&&s.p99==119&&s.maximum==120&&s.late==95);
    m.add(0);m.add(-1);m.add(std::numeric_limits<double>::quiet_NaN());assert(m.take(60).count==0);
    m.add(16.7);s=m.take(0);assert(s.count==1&&s.p99==16.7&&s.late==0);
}
