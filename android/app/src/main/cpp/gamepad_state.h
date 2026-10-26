#pragma once
#include <array>
#include <cmath>
#include <cstdint>

struct GuestPadState {
    float leftX=0,leftY=0,rightX=0,rightY=0,leftTrigger=0,rightTrigger=0;
    uint32_t buttons=0;
    int hatX=0,hatY=0;
};

class GamepadState {
public:
    enum Button : unsigned {A,B,X,Y,L1,R1,L2,R2,Start,Select,ThumbL,ThumbR,Up,Down,Left,Right,ButtonCount};
    // The game switched between menus and driving, or started/stopped reading
    // the pad as a joystick: remap whatever is held.
    template<class SetKey> bool refresh(bool driving,SetKey&& setKey){
        if(driving==driving_&&emulate_==sentEmulation_)return false;
        apply(driving,setKey);return true;
    }
    template<class SetKey> void releaseAll(SetKey&& setKey){
        buttons_.fill(false);stickX_=stickY_=rightX_=rightY_=hatX_=hatY_=leftTrigger_=rightTrigger_=0;apply(driving_,setKey);
    }
    // false while the game reads the pad as a DirectInput joystick.
    void setTriggerFallback(bool enabled){triggerFallback_=enabled;}
    void setKeyEmulation(bool emulate){emulate_=emulate;}
    GuestPadState padState()const{
        GuestPadState state;
        state.leftX=stickX_;state.leftY=stickY_;state.rightX=rightX_;state.rightY=rightY_;
        state.leftTrigger=std::fmax(leftTrigger_,buttons_[L2]?1.f:0.f);
        state.rightTrigger=std::fmax(rightTrigger_,buttons_[R2]?1.f:0.f);
        static constexpr Button order[]={A,B,X,Y,L1,R1,Select,Start,ThumbL,ThumbR};
        for(unsigned i=0;i<10;++i){
            // These driving actions use explicit keyboard bindings below. Do not
            // also deliver the game's conflicting default joystick actions.
            if(driving_&&(order[i]==X||order[i]==Y||order[i]==ThumbR))continue;
            if(buttons_[order[i]])state.buttons|=1u<<i;
        }
        state.hatX=buttons_[Left]||hatX_<-.5f?-1:buttons_[Right]||hatX_>.5f?1:0;
        state.hatY=buttons_[Up]||hatY_<-.5f?-1:buttons_[Down]||hatY_>.5f?1:0;
        return state;
    }
protected:
    std::array<bool,ButtonCount> buttons_{};
    float stickX_=0,stickY_=0,rightX_=0,rightY_=0,hatX_=0,hatY_=0,leftTrigger_=0,rightTrigger_=0;
    std::array<bool,256> sent_{};
    bool triggerFallback_=true;
    bool driving_=false,emulate_=true,sentEmulation_=true;

    template<class SetKey> void apply(bool driving,SetKey& setKey){
        std::array<bool,256> want{};
        // Hysteresis keeps a stick resting near the threshold from chattering.
        auto past=[&](float value,unsigned scan){return value>(sent_[scan]?.25f:.4f);};
        bool left=buttons_[Left]||hatX_<-.5f||past(-stickX_,0xcb),right=buttons_[Right]||hatX_>.5f||past(stickX_,0xcd);
        bool up=buttons_[Up]||hatY_<-.5f,down=buttons_[Down]||hatY_>.5f;
        want[0xcb]=left;want[0xcd]=right;
        if(driving){
            // Xbox-style racing: RT gas, LT brake/reverse, A handbrake, B nitrous,
            // X camera, Y accept/free-roam action, R3 look back; RB/LB shift, Start pause.
            want[0xc8]=up||buttons_[R2]||past(rightTrigger_,0xc8);
            want[0xd0]=down||buttons_[L2]||past(leftTrigger_,0xd0);
            want[0x39]=buttons_[A];want[0x38]=buttons_[B];want[0x2e]=buttons_[X];
            want[0x1c]=buttons_[Y];want[0x30]=buttons_[ThumbR];
            want[0x2a]=buttons_[R1];want[0x1d]=buttons_[L1];
            want[0x01]=buttons_[Start]||buttons_[Select];
        }else{
            want[0xc8]=up||past(-stickY_,0xc8);want[0xd0]=down||past(stickY_,0xd0);
            want[0x1c]=buttons_[A]||buttons_[Start];want[0x01]=buttons_[B]||buttons_[Select];
            want[0x39]=buttons_[X];want[0x2a]=buttons_[Y];
        }
        driving_=driving;sentEmulation_=emulate_;
        if(!emulate_){
            want.fill(false);
            // The game may acquire DirectInput with button-only pedal bindings.
            // Keep RT/LT working through its keyboard pedal actions in this mode.
            if(driving){
                want[0x2e]=buttons_[X];want[0x1c]=buttons_[Y];want[0x30]=buttons_[ThumbR];
            }
            if(driving&&triggerFallback_){
                want[0xc8]=buttons_[R2]||past(rightTrigger_,0xc8);
                want[0xd0]=buttons_[L2]||past(leftTrigger_,0xd0);
            }
        }
        for(unsigned scan=0;scan<256;++scan)if(want[scan]!=sent_[scan]){sent_[scan]=want[scan];setKey(scan,want[scan]);}
    }
};
