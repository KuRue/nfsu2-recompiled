#include "../android/app/src/main/cpp/gamepad_state.h"
#include <stdexcept>
#include <iostream>
struct Pad:GamepadState{
    template<class F>void update(bool driving,float lt,float rt,bool a,bool digitalLT,bool digitalRT,F& f){
        leftTrigger_=lt;rightTrigger_=rt;buttons_[A]=a;buttons_[L2]=digitalLT;buttons_[R2]=digitalRT;apply(driving,f);
    }
};
static void require(bool yes){if(!yes)throw std::runtime_error("Controller pedal regression");}
int main(){
    Pad pad;std::array<bool,256> keys{};auto send=[&](unsigned scan,bool down){keys[scan]=down;};
    pad.setKeyEmulation(false); // Game acquired its DirectInput joystick.
    pad.update(true,0,.7f,false,false,false,send);require(keys[0xc8]&&!keys[0xd0]);
    pad.update(true,.8f,.7f,true,false,false,send);require(keys[0xc8]&&keys[0xd0]&&!keys[0x39]);
    pad.update(true,.8f,0,false,false,false,send);require(!keys[0xc8]&&keys[0xd0]);
    pad.update(false,.8f,.7f,false,false,false,send);require(!keys[0xc8]&&!keys[0xd0]);
    pad.update(true,0,0,false,true,true,send);require(keys[0xc8]&&keys[0xd0]);
    pad.releaseAll(send);require(!keys[0xc8]&&!keys[0xd0]);
    pad.setTriggerFallback(false);pad.update(true,.8f,.8f,false,false,false,send);require(!keys[0xc8]&&!keys[0xd0]);
    auto state=pad.padState();require(state.leftTrigger==.8f&&state.rightTrigger==.8f);
    pad.setKeyEmulation(true);pad.update(true,0,.8f,true,false,false,send);require(keys[0xc8]&&keys[0x39]);
    std::cout<<"Controller checks passed: acquired joystick, independent triggers, menu isolation, button triggers, release and opt-out\n";
}
