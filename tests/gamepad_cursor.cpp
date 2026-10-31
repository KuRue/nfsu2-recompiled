#include "../android/app/src/main/cpp/gamepad_cursor.h"
#include <cassert>
#include <vector>
#include <limits>
struct Event{float x,y;int phase;};
int main(){
    GamepadCursor cursor;std::vector<Event> events;
    auto send=[&](float x,float y,int phase){events.push_back({x,y,phase});};
    cursor.update(false,.02f,.1f,-.1f,false,send);assert(events.empty());
    for(int i=0;i<100;i++)cursor.update(false,.02f,1,1,false,send);
    assert(events.back().x==1&&events.back().y==1);
    cursor.update(false,.02f,0,0,true,send);assert(events.back().phase==0);
    auto size=events.size();cursor.update(false,.02f,0,0,true,send);assert(events.size()==size);
    cursor.update(true,.02f,1,1,true,send);assert(events.back().phase==2);
    size=events.size();cursor.update(true,.02f,-1,-1,false,send);assert(events.size()==size);
    // Disconnect/pause cancels a held click exactly once.
    cursor.update(false,.02f,0,0,true,send);cursor.reset(send);assert(events.back().phase==2);
    size=events.size();cursor.reset(send);assert(events.size()==size);
    cursor.update(false,.02f,std::numeric_limits<float>::quiet_NaN(),0,false,send);assert(events.size()==size);
    // A long pause must not warp the cursor across the whole view.
    cursor.update(false,10,-1,0,false,send);assert(events.back().x>.9f);
}
