#include "../android/app/src/main/cpp/physics_haptics.h"
#include <stdexcept>
#include <iostream>
using namespace nfs_physics;
void check(bool v){if(!v)throw std::runtime_error("Physics haptics regression");}
Snapshot car(float v=20){Snapshot s;s.valid=true;s.anchor=1;s.race=2;s.forward={1,0,0};s.left={0,1,0};s.up={0,0,1};s.velocity={v,0,0};s.wheels.fill(v/.30f);s.rpm=2500;return s;}
void advance(Haptics& h,Snapshot& s,uint64_t& now){s.clock+=80;for(unsigned i=0;i<3;++i)s.position[i]+=s.velocity[i]*.02f;now+=20;h.update(s,now);}
int main(){
    Haptics h;auto s=car();uint64_t t=1000;h.update(s,t);
    for(unsigned i=0;i<60;++i){advance(h,s,t);}check(h.channels.valid&&h.channels.grip==0&&h.channels.surface==0&&h.packet(t)==0);
    s=car(0);h.stop();h.update(s,t);for(unsigned i=0;i<30;++i){advance(h,s,t);}check(h.packet(t)==0); // No steering input can fake slip.
    s=car();h.stop();h.update(s,t);for(unsigned i=0;i<60;++i){s.velocity[1]=8;advance(h,s,t);}check(h.channels.grip>100&&h.channels.slipAngle>20);
    s=car();h.stop();h.update(s,t);s.wheels.fill(0);s.brake=1;for(unsigned i=0;i<60;++i){advance(h,s,t);}check(h.channels.grip>150);
    s=car(1);s.throttle=1;s.wheels.fill(60);h.stop();h.update(s,t);for(unsigned i=0;i<60;++i){advance(h,s,t);}check(h.channels.grip>150);
    s=car();h.stop();h.update(s,t);for(unsigned i=0;i<10;++i){s.suspension.fill(i%2?.05f:0.f);advance(h,s,t);}check(h.channels.surface>20&&h.channels.surface<=45&&(h.packet(t)>>8&255)!=2);
    // Smooth hills/weight transfer should not buzz; sharp curbs still do above.
    s=car();h.stop();h.update(s,t);
    for(unsigned i=0;i<300;++i){
        float phase=i*.02f*6.2831853f;
        s.suspension={.04f*std::sin(phase),.04f*std::sin(phase),-.04f*std::sin(phase),-.04f*std::sin(phase)};
        advance(h,s,t);check(h.channels.grip==0&&h.channels.surface==0);
    }
    // Normal rolling through a bend has centripetal acceleration but no slip.
    s=car();h.stop();h.update(s,t);
    for(unsigned i=0;i<100;++i){float angle=(i+1)*.006f;
        s.forward={std::cos(angle),std::sin(angle),0};s.left={-std::sin(angle),std::cos(angle),0};
        s.velocity={20*s.forward[0],20*s.forward[1],0};advance(h,s,t);
        check(h.channels.grip==0&&(h.packet(t)>>8&255)!=2);
    }
    // Ordinary braking with rolling wheels must not resemble a wall impulse.
    s=car();h.stop();h.update(s,t);s.brake=1;
    for(unsigned i=0;i<50;++i){s.velocity[0]-=.2f;s.wheels.fill(s.velocity[0]/.3f);advance(h,s,t);check(h.packet(t)==0);}
    // Coasting calibration follows a coherent radius without trusting wheelspin.
    s=car();s.wheels.fill(20/.296f);h.stop();h.update(s,t);
    for(unsigned i=0;i<100;++i){advance(h,s,t);}check(std::fabs(h.channels.radius-.296f)<.001f);
    // A wall impulse dominates road/slip and emits one stable event id.
    s.velocity[0]=5;advance(h,s,t);auto hit=h.packet(t);check((hit>>8&255)==2&&(hit&255)>150);
    advance(h,s,t);check((h.packet(t)>>16)==(hit>>16));
    // Frozen clock, race transitions, invalid values and teleports do not buzz.
    check(h.packet(t+200)==0);h.stop();check(h.packet(t)==0);
    s=car();h.update(s,t);s.position[0]+=500;s.clock+=80;t+=20;h.update(s,t);check(h.packet(t)==0);
    s.anchor=3;advance(h,s,t);check(h.packet(t)==0);s.valid=false;h.update(s,t);check(h.packet(t)==0);
    std::cout<<"Physics haptics passed: coasting/stopped silence, drift, wheel lock/spin, surface vs impact, event priority, frozen data and resets\n";
}
