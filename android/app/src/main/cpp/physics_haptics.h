#pragma once
#include "physics_probe.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace nfs_physics {
struct Channels {
    bool valid=false;
    float forwardSpeed=0,lateralSpeed=0,slipAngle=0,wheelSlip=0,suspensionRate=0,planarAcceleration=0,radius=.30f;
    unsigned grip=0,surface=0;
};
// Derived slip/impact estimates from checked kinematics, not measured tire forces.
// Runs only while the guest machine is owned; UI accesses it under inputMutex.
class Haptics {
    Snapshot previous{};bool havePrevious=false;
    uint64_t lastUpdate=0,impactUntil=0,lastImpact=0;
    uint32_t impactSequence=0;unsigned impactStrength=0;
    float radius=.30f,gripFiltered=0,surfaceFiltered=0;
    std::array<float,4> suspensionTrend{};
    static float clamp01(float n){return std::clamp(n,0.f,1.f);}
    static float length(const std::array<float,3>& v){return std::sqrt(dot(v,v));}
    void clearState(){havePrevious=false;impactUntil=0;gripFiltered=surfaceFiltered=0;suspensionTrend={};channels={};}
public:
    Channels channels;
    void stop(){clearState();lastUpdate=0;}
    void update(const Snapshot& s,uint64_t now){
        if(!s.valid){stop();return;}
        if(!havePrevious||s.anchor!=previous.anchor||s.race!=previous.race){
            clearState();radius=.30f;previous=s;havePrevious=true;lastUpdate=now;return;
        }
        uint32_t ticks=s.clock-previous.clock;
        if(!ticks){if(now-lastUpdate>150){channels={};gripFiltered=surfaceFiltered=0;impactUntil=0;}return;}
        float dt=ticks/4000.f;
        if(dt<.003f)return;
        if(dt>.12f||now-lastUpdate>250){clearState();previous=s;havePrevious=true;lastUpdate=now;return;}
        std::array<float,3> dv{},error{};
        for(unsigned i=0;i<3;++i){dv[i]=s.velocity[i]-previous.velocity[i];error[i]=s.position[i]-previous.position[i]-previous.velocity[i]*dt;}
        if(length(error)>std::max(2.f,length(previous.velocity)*dt*.8f)){
            clearState();previous=s;havePrevious=true;lastUpdate=now;return;
        }
        Channels c;c.valid=true;
        c.forwardSpeed=dot(s.velocity,s.forward);c.lateralSpeed=dot(s.velocity,s.left);
        float speed=length(s.velocity),vf=std::fabs(c.forwardSpeed),vertical=std::fabs(dot(s.velocity,s.up));
        // Robust rolling-radius calibration: coast straight, avoid braking/spin.
        if(vf>6&&std::fabs(c.lateralSpeed)<.5f&&s.throttle<.2f&&s.brake<.1f){
            std::array<float,4> omega{};for(unsigned i=0;i<4;++i)omega[i]=std::fabs(s.wheels[i]);
            std::sort(omega.begin(),omega.end());float middle=(omega[1]+omega[2])*.5f;
            if(middle>10&&omega[3]-omega[0]<middle*.15f){float r=vf/middle;
                if(r>.22f&&r<.45f)radius+=(r-radius)*std::min(1.f,dt*1.5f);}
        }
        c.radius=radius;
        if(vf>3.5f)c.slipAngle=std::atan2(std::fabs(c.lateralSpeed),vf)*57.29578f;
        // Axle averages reduce inside/outside wheel-speed differences in turns.
        float front=std::fabs((s.wheels[0]+s.wheels[1])*.5f)*radius;
        float rear=std::fabs((s.wheels[2]+s.wheels[3])*.5f)*radius;
        float longitudinal=0;
        if(vf>4){
            for(float surface:{front,rear})longitudinal=std::max(longitudinal,std::fabs(surface-vf)/std::max({vf,surface,3.f}));
        }else if(s.throttle>.5f&&std::max(front,rear)>4){longitudinal=clamp01((std::max(front,rear)-vf)/std::max(front,rear));}
        c.wheelSlip=longitudinal;
        float drift=clamp01((c.slipAngle-6.f)/18.f),spinLock=clamp01((longitudinal-.30f)/.5f);
        float grip=vertical<1.5f?std::max(drift,spinLock):0;
        if(speed<1&&std::max(front,rear)<4)grip=0; // Parked steering/revving is silent.
        float response=std::min(1.f,dt*(grip>gripFiltered?12.f:6.f));gripFiltered+=(grip-gripFiltered)*response;
        c.grip=unsigned(std::clamp(gripFiltered*210.f,0.f,210.f));
        float rate=0,sharpRate=0;
        for(unsigned i=0;i<4;++i){
            float derivative=(s.suspension[i]-previous.suspension[i])/dt;
            rate+=std::fabs(derivative);
            // Remove slow heave/pitch/weight transfer before generating road texture.
            // A smooth hill is not evidence that a tire has lost grip.
            suspensionTrend[i]+=(derivative-suspensionTrend[i])*(dt/(.12f+dt));
            sharpRate+=std::fabs(derivative-suspensionTrend[i]);
        }
        c.suspensionRate=rate*.25f;
        float roughness=speed>2?clamp01((sharpRate*.25f-.35f)/1.2f):0;
        surfaceFiltered+=(roughness-surfaceFiltered)*(dt/((roughness>surfaceFiltered?.10f:.18f)+dt));
        c.surface=unsigned(std::clamp(surfaceFiltered*45.f,0.f,45.f));
        float horizontalDV=std::hypot(dv[0],dv[1]);c.planarAcceleration=horizontalDV/dt;
        // Sudden planar impulses distinguish hits from ordinary braking/cornering.
        // Predominantly vertical curb/landing shocks remain surface feedback.
        if(length(previous.velocity)>2&&horizontalDV>.65f&&c.planarAcceleration>38.f&&
           horizontalDV>std::fabs(dv[2])*.65f&&now-lastImpact>280){
            impactStrength=unsigned(150+105*clamp01((c.planarAcceleration-38.f)/180.f));
            lastImpact=now;impactUntil=now+210;impactSequence=impactSequence%127+1;
        }
        channels=c;previous=s;lastUpdate=now;
    }
    // Bits0..7 strength, bits8..15 channel (0 surface,1 slip,2 impact), bits16..22 event id.
    uint32_t packet(uint64_t now)const{
        if(!channels.valid||now-lastUpdate>150)return 0;
        if(now<impactUntil)return impactStrength|(2u<<8)|(impactSequence<<16);
        // Chatter is tied to slip, not steering angle. Surface stays low underneath.
        unsigned slip=channels.grip>8&&now%150<90?channels.grip:0;
        return slip>channels.surface?slip|(1u<<8):channels.surface;
    }
};
}
