package com.nfsu2.androidevolved;

import android.content.Context;
import android.media.AudioAttributes;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import java.util.function.IntSupplier;

/** Physics feedback prefers a capable external gamepad, falling back to the handset. */
final class PhoneFeedback {
    private final Vibrator handset;
    private Vibrator vibrator;
    private int targetDevice=-1;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final IntSupplier source;
    private final Runnable stopSource;
    private final int strength;
    private boolean running,active;
    private int lastImpact=-1,surfaceAmplitude;
    private long surfaceAt;
    private final AudioAttributes attributes=new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build();
    static Vibrator motor(Context context) {
        if(Build.VERSION.SDK_INT>=31){VibratorManager manager=context.getSystemService(VibratorManager.class);return manager==null?null:manager.getDefaultVibrator();}
        return context.getSystemService(Vibrator.class);
    }
    PhoneFeedback(Context context,int strength,IntSupplier source,Runnable stopSource){
        this.handset=motor(context);this.vibrator=handset;this.strength=strength;this.source=source;this.stopSource=stopSource;
        updateDevices(android.view.InputDevice.getDeviceIds());
    }
    @SuppressWarnings("deprecation")
    private static Vibrator controllerMotor(android.view.InputDevice device){
        return Build.VERSION.SDK_INT>=31?device.getVibratorManager().getDefaultVibrator():device.getVibrator();
    }
    void updateDevices(int[] ids){
        Vibrator next=handset;int nextDevice=-1;String name="Phone";
        // Retain the current capable controller if another pad is connected.
        for(int pass=0;pass<2&&nextDevice<0;pass++)for(int id:ids){
            if(pass==0&&id!=targetDevice)continue;
            android.view.InputDevice device=android.view.InputDevice.getDevice(id);
            if(!GamepadDevices.isPhysicalGamepad(device))continue;
            Vibrator candidate=controllerMotor(device);
            android.util.Log.i("NFSU2","Controller feedback: "+device.getName()+" id="+id+" motor="+candidate.hasVibrator());
            if(candidate.hasVibrator()){next=candidate;nextDevice=id;name=device.getName();break;}
        }
        if(vibrator!=null)vibrator.cancel();
        vibrator=next;targetDevice=nextDevice;active=false;surfaceAmplitude=0;lastImpact=-1;surfaceAt=0;
        android.util.Log.i("NFSU2","Feedback output: "+name+" id="+targetDevice);
    }
    boolean available(){return vibrator!=null&&vibrator.hasVibrator();}
    boolean enabled(){return available()&&strength>0;}
    private final Runnable poll=new Runnable(){@Override public void run(){
        if(!running)return;
        if(!available()){handler.postDelayed(this,250);return;}
        int packet=source.getAsInt(),channel=(packet>>>8)&255,event=(packet>>>16)&127;
        int amplitude=Math.min(255,(packet&255)*strength/100);
        if(channel==2&&amplitude>2){
            if(event!=lastImpact||!active){
                lastImpact=event;surfaceAmplitude=0;
                try{
                    if(vibrator.hasAmplitudeControl())vibrator.vibrate(VibrationEffect.createWaveform(
                            new long[]{15,65,45,35},new int[]{0,amplitude,0,Math.max(1,amplitude*2/3)},-1),attributes);
                    else vibrator.vibrate(VibrationEffect.createWaveform(new long[]{15,Math.max(8,65*strength/100),45,Math.max(8,35*strength/100)},-1),attributes);
                    active=true;android.util.Log.i("NFSU2","Physics impact waveform event="+event+" amplitude="+amplitude);
                }catch(SecurityException|IllegalArgumentException e){stop();return;}
            }
            handler.postDelayed(this,40);return;
        }
        if(amplitude>2){
            try{
                boolean amplitudeControl=vibrator.hasAmplitudeControl();
                if(!active)android.util.Log.i("NFSU2","Feedback playing device="+targetDevice+" amplitude="+amplitude);
                if(channel==0&&amplitudeControl){
                    long now=android.os.SystemClock.elapsedRealtime();
                    if(!active||now-surfaceAt>=80){
                        int previous=surfaceAmplitude>0?surfaceAmplitude:1;
                        vibrator.vibrate(VibrationEffect.createWaveform(new long[]{35,50,70},
                            new int[]{previous,Math.max(1,(previous+amplitude)/2),amplitude},-1),attributes);
                        surfaceAmplitude=amplitude;surfaceAt=now;
                    }
                }else{
                    surfaceAmplitude=0;
                    vibrator.vibrate(VibrationEffect.createOneShot(amplitudeControl?65:Math.max(8,65*strength/100),
                            amplitudeControl?amplitude:VibrationEffect.DEFAULT_AMPLITUDE),attributes);
                }
                active=true;
            }
            catch(SecurityException|IllegalArgumentException e){stop();return;}
        }else if(active){vibrator.cancel();active=false;surfaceAmplitude=0;}
        handler.postDelayed(this,40);
    }};
    void start(){if(enabled()&&!running){running=true;handler.post(poll);}}
    void stop(){running=false;handler.removeCallbacks(poll);if(vibrator!=null)vibrator.cancel();active=false;surfaceAmplitude=0;lastImpact=-1;stopSource.run();}
    static boolean test(Context context){
        PhoneFeedback feedback=new PhoneFeedback(context,100,()->0,()->{});
        Vibrator motor=feedback.vibrator;if(motor==null||!motor.hasVibrator())return false;
        motor.vibrate(VibrationEffect.createOneShot(180,motor.hasAmplitudeControl()?110:VibrationEffect.DEFAULT_AMPLITUDE),
                new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).build());return true;
    }
}
