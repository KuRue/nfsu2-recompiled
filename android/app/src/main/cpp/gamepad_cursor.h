#pragma once
#include <algorithm>
#include <cmath>

// Menu-only pointer fallback for PC controls outside the D-pad focus order.
class GamepadCursor {
    float x_=.5f,y_=.5f,elapsed_=0;
    bool pressed_=false;
    static float axis(float value){
        float magnitude=std::fabs(value);
        if(!std::isfinite(value)||magnitude<=.18f)return 0;
        float normalized=std::min(1.f,(magnitude-.18f)/.82f);
        return std::copysign(normalized*normalized,value);
    }
public:
    template<class Pointer> void update(bool driving,float seconds,float horizontal,float vertical,bool down,Pointer&& pointer){
        if(driving){reset(pointer);return;}
        elapsed_+=std::clamp(seconds,0.f,.05f);
        if(elapsed_<1.f/120&&down==pressed_)return;
        float dt=std::min(elapsed_,.05f);elapsed_=0;
        float x=std::clamp(x_+axis(horizontal)*dt*.7f,0.f,1.f);
        float y=std::clamp(y_+axis(vertical)*dt*.7f,0.f,1.f);
        if(x!=x_||y!=y_){x_=x;y_=y;pointer(x_,y_,1);}
        if(down!=pressed_){pressed_=down;pointer(x_,y_,down?0:2);}
    }
    template<class Pointer> void reset(Pointer&& pointer){
        if(pressed_)pointer(x_,y_,2);
        pressed_=false;elapsed_=0;
    }
};
