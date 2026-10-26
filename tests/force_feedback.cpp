#include "../android/app/src/main/cpp/force_feedback.h"
#include <stdexcept>
#include <iostream>
using namespace nfs_ff;
void check(bool value){if(!value)throw std::runtime_error("Force feedback regression");}
int main(){
    Mixer m;auto& e=m.effects[1];e.owner=10;e.type=0x13541c23;e.durationUs=100000;e.magnitude=10000;
    m.start(e,1,1000);check(m.sample(1000)==255);check(m.sample(1100)==0);
    e.startDelayUs=20000;e.gain=5000;m.start(e,2,2000);check(m.sample(2000)==0);check(m.sample(2020)==127);check(m.sample(2219)==127);check(m.sample(2220)==0);
    e.startDelayUs=0;e.durationUs=0xffffffff;m.start(e,1,3000);check(m.sample(100000)==127);
    m.paused=true;check(m.sample(3001)==0);m.paused=false;m.actuators=false;check(m.sample(3001)==0);m.actuators=true;
    m.stopAll(11);check(m.sample(3001)==127);m.stopAll(10);check(m.sample(3001)==0);
    e.type=0x13541c27;m.start(e,1,4000);check(m.sample(4000)==0);
    e.type=0x13541c20;e.magnitude=INT32_MIN;m.start(e,1,5000);check(m.sample(5000)==127);
    e.durationUs=100000;e.gain=10000;e.magnitude=10000;e.attackUs=20000;e.attackLevel=0;e.fadeUs=20000;e.fadeLevel=0;
    m.start(e,1,6000);check(m.sample(6000)==0);check(m.sample(6010)==127);check(m.sample(6050)==255);check(m.sample(6090)==127);check(m.sample(6100)==0);
    m.start(e,0,7000);check(m.sample(7000)==0);
    std::cout<<"Force feedback checks passed: gain, duration, delay, repeat, infinite, stop, pause, actuator mute, signed bounds and envelopes\n";
}
