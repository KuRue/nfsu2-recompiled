#pragma once
#include "gamepad_state.h"
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>

class GamepadInput : public GamepadState {
public:
    template<class SetKey>
    bool key(const AInputEvent* event,bool driving,SetKey&& setKey){
        int32_t code=AKeyEvent_getKeyCode(event),action=AKeyEvent_getAction(event);
        int button=buttonFor(code);
        if(button<0){
            if(isGamepad(event)&&action==AKEY_EVENT_ACTION_DOWN&&AKeyEvent_getRepeatCount(event)==0)
                __android_log_print(ANDROID_LOG_INFO,"NFSU2","Gamepad key %d not mapped (device %d)",code,AInputEvent_getDeviceId(event));
            return false;
        }
        if(action!=AKEY_EVENT_ACTION_DOWN&&action!=AKEY_EVENT_ACTION_UP)return true;
        buttons_[button]=action==AKEY_EVENT_ACTION_DOWN;
        apply(driving,setKey);return true;
    }
    template<class SetKey>
    bool motion(const AInputEvent* event,bool driving,SetKey&& setKey){
        if(!(AInputEvent_getSource(event)&AINPUT_SOURCE_CLASS_JOYSTICK))return false;
        auto axis=[&](int32_t which){return AMotionEvent_getAxisValue(event,which,0);};
        stickX_=axis(AMOTION_EVENT_AXIS_X);stickY_=axis(AMOTION_EVENT_AXIS_Y);
        hatX_=axis(AMOTION_EVENT_AXIS_HAT_X);hatY_=axis(AMOTION_EVENT_AXIS_HAT_Y);
        // Android's standard right stick is Z/RZ.
        rightX_=axis(AMOTION_EVENT_AXIS_Z);rightY_=axis(AMOTION_EVENT_AXIS_RZ);
        // Pads report triggers as LTRIGGER/RTRIGGER, BRAKE/GAS or both.
        leftTrigger_=std::fmax(axis(AMOTION_EVENT_AXIS_LTRIGGER),axis(AMOTION_EVENT_AXIS_BRAKE));
        rightTrigger_=std::fmax(axis(AMOTION_EVENT_AXIS_RTRIGGER),axis(AMOTION_EVENT_AXIS_GAS));
        apply(driving,setKey);return true;
    }
    static bool isGamepad(const AInputEvent* event){
        int32_t source=AInputEvent_getSource(event);
        return (source&AINPUT_SOURCE_GAMEPAD)==AINPUT_SOURCE_GAMEPAD||(source&AINPUT_SOURCE_JOYSTICK)==AINPUT_SOURCE_JOYSTICK;
    }
private:
    static int buttonFor(int32_t code){
        switch(code){
        case AKEYCODE_BUTTON_A:case AKEYCODE_BUTTON_1:return A;
        case AKEYCODE_BUTTON_B:case AKEYCODE_BUTTON_2:return B;
        case AKEYCODE_BUTTON_X:case AKEYCODE_BUTTON_3:return X;
        case AKEYCODE_BUTTON_Y:case AKEYCODE_BUTTON_4:return Y;
        case AKEYCODE_BUTTON_L1:case AKEYCODE_BUTTON_5:return L1;
        case AKEYCODE_BUTTON_R1:case AKEYCODE_BUTTON_6:return R1;
        case AKEYCODE_BUTTON_L2:case AKEYCODE_BUTTON_7:return L2;
        case AKEYCODE_BUTTON_R2:case AKEYCODE_BUTTON_8:return R2;
        case AKEYCODE_BUTTON_SELECT:case AKEYCODE_BUTTON_9:case AKEYCODE_BACK:return Select;
        case AKEYCODE_BUTTON_START:case AKEYCODE_BUTTON_10:case AKEYCODE_MENU:return Start;
        case AKEYCODE_BUTTON_THUMBL:case AKEYCODE_BUTTON_11:return ThumbL;
        case AKEYCODE_BUTTON_THUMBR:case AKEYCODE_BUTTON_12:return ThumbR;
        case AKEYCODE_DPAD_UP:return Up;case AKEYCODE_DPAD_DOWN:return Down;
        case AKEYCODE_DPAD_LEFT:return Left;case AKEYCODE_DPAD_RIGHT:return Right;
        case AKEYCODE_DPAD_CENTER:return A;
        default:return -1;
        }
    }
};
