#include "../android/app/src/main/cpp/force_feedback.h"
#include <stdexcept>
#include <iostream>
using namespace nfs_ff;
void check(bool value){if(!value)throw std::runtime_error("Force feedback regression");}
int main(){
    Mixer m;m.distinctImpacts=false;auto& e=m.effects[1];e.owner=10;e.type=0x13541c23;e.durationUs=100000;e.magnitude=10000;
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
    // A 5 ms impact must survive the slower UI poll and even object release.
    m.stopAll();e.attackUs=e.fadeUs=0;e.durationUs=5000;e.magnitude=8000;e.gain=10000;
    m.start(e,1,8000);e.playing=false;check(m.sample(8040)==204);check(m.sample(8085)==0);
    m.start(e,1,9000);m.stopAll();check(m.sample(9040)==0);
    // Gain mute and lifecycle StopAll must also flush queued transients.
    m.start(e,1,10000);m.gain=0;check(m.sample(10040)==0);m.gain=10000;m.stopAll();
    e.type=0x13541c27;e.durationUs=0xffffffff;e.conditionForce=8000;e.conditionSeen=false;
    m.start(e,1,11000);check(m.sample(11000,0)==0);check(m.sample(11000,.8f)>=70);
    check(m.sample(11000,.1f)==0);check(m.sample(11000,1.f)>m.sample(11000,.5f));
    e.conditionForce=3000;m.parametersChanged(e,11020);check(m.sample(11040,0)>0);
    check(m.sample(11105,0)==0);m.stopAll();check(m.sample(11106,.8f)==0);
    for(unsigned i=0;i<100;++i){m.cue(10,12000,100);}
    check(m.cues.size()==32);m.stopAll();
    auto& transient=m.effects[2];transient.owner=10;transient.type=0x13541c20;
    transient.magnitude=10000;transient.durationUs=5000;m.start(transient,1,13000);
    m.effects.erase(2);check(m.sample(13040)==255);m.stopAll();check(m.sample(13041)==0);
    Mixer impact;auto& road=impact.effects[1];road.type=0x13541c23;road.durationUs=0xffffffff;road.magnitude=1500;
    impact.start(road,1,14000);impact.cue(10,14000,240);
    check(impact.sample(14010)==0);check(impact.sample(14050)==240);
    check(impact.sample(14130)==0);check(impact.sample(14200)==240);check(impact.sample(14245)==0);
    check(impact.sample(14260)>0); // Road hum returns after the pattern.
    impact.cue(10,15000,200);impact.cue(10,15020,240);check(impact.cues.size()==1);
    check(impact.sample(15050)==240); // Update strengthens without resetting onset.
    impact.stopAll();check(impact.sample(15190)==0);
    std::cout<<"Force feedback checks passed: timing/gain/envelopes/bounds, short impacts, object release, grip steering/change, StopAll and queue cap\n";
}
