package com.nfsu2.androidevolved;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

/** Shared input/feedback filter: exclude built-in sensors and ordinary keyboards. */
final class GamepadDevices {
    static boolean isPhysicalGamepad(InputDevice device){
        if(device==null||device.isVirtual()||!device.isExternal())return false;
        int sources=device.getSources();
        boolean buttons=(sources&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD;
        boolean stick=(sources&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK
                &&device.getMotionRange(MotionEvent.AXIS_X)!=null&&device.getMotionRange(MotionEvent.AXIS_Y)!=null;
        boolean keys=(sources&InputDevice.SOURCE_KEYBOARD)==InputDevice.SOURCE_KEYBOARD
                &&device.getKeyboardType()!=InputDevice.KEYBOARD_TYPE_ALPHABETIC
                &&device.hasKeys(KeyEvent.KEYCODE_BUTTON_A)[0];
        return buttons||stick||keys;
    }
}
