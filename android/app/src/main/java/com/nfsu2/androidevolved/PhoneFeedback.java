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
    private VibratorManager controllerManager;
    private int targetDevice=-1;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final IntSupplier source;
    private final Runnable stopSource;
    private final int strength;
    private boolean running,active;
    private int lastImpact=-1,surfaceAmplitude,lastChannel=-1;
    private long surfaceAt,impactQuietUntil;
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
            android.util.Log.i("NFSU2","Controller feedback: "+device.getName()+" id="+id+" motor="+candidate.hasVibrator()+" amplitude="+candidate.hasAmplitudeControl()+" motors="+(Build.VERSION.SDK_INT>=31?java.util.Arrays.toString(device.getVibratorManager().getVibratorIds()):"legacy"));
            if(candidate.hasVibrator()){next=candidate;nextDevice=id;name=device.getName();break;}
        }
        cancelOutput();controllerManager=null;
        if(Build.VERSION.SDK_INT>=31&&nextDevice>=0){
            android.view.InputDevice device=android.view.InputDevice.getDevice(nextDevice);
            if(device!=null){
                VibratorManager manager=device.getVibratorManager();int[] motors=manager.getVibratorIds();
                boolean strong=false,weak=false;for(int id:motors){strong|=id==0;weak|=id==1;}
                if(strong&&weak&&manager.getVibrator(0).hasAmplitudeControl()&&manager.getVibrator(1).hasAmplitudeControl())controllerManager=manager;
            }
        }
        vibrator=next;targetDevice=nextDevice;active=false;surfaceAmplitude=0;lastImpact=-1;lastChannel=-1;surfaceAt=0;impactQuietUntil=0;
        android.util.Log.i("NFSU2","Feedback output: "+name+" id="+targetDevice+" dual="+(controllerManager!=null));
    }
    private void cancelOutput(){
        if(controllerManager!=null)controllerManager.cancel();
        if(vibrator!=null)vibrator.cancel();
    }
    private VibrationEffect effect(int channel,int amplitude,int previous,int heldMs){
        return VibrationEffect.createWaveform(heldMs>0?new long[]{heldMs}:FeedbackEnvelope.timings(channel),
                heldMs>0?new int[]{amplitude}:FeedbackEnvelope.amplitudes(channel,amplitude,previous),-1);
    }
    private void play(int channel,int amplitude,int previous){play(channel,amplitude,previous,0);}
    private void play(int channel,int amplitude,int previous,int heldMs){
        if(Build.VERSION.SDK_INT>=31&&controllerManager!=null){
            android.os.CombinedVibration.ParallelCombination combined=android.os.CombinedVibration.startParallel();
            for(int motor=0;motor<2;motor++){
                int level=FeedbackEnvelope.motorAmplitude(channel,motor,amplitude);
                int old=FeedbackEnvelope.motorAmplitude(channel,motor,previous);
                combined.addVibrator(motor,effect(channel,level,old,heldMs));
            }
            controllerManager.vibrate(combined.combine(),new android.os.VibrationAttributes.Builder()
                    .setUsage(android.os.VibrationAttributes.USAGE_MEDIA).build());
        }else if(vibrator.hasAmplitudeControl())vibrator.vibrate(effect(channel,amplitude,previous,heldMs),attributes);
        else vibrator.vibrate(VibrationEffect.createWaveform(FeedbackEnvelope.binaryTimings(channel,amplitude),-1),attributes);
    }
    boolean available(){return vibrator!=null&&vibrator.hasVibrator();}
    boolean enabled(){return available()&&strength>0;}
    private final Runnable poll=new Runnable(){@Override public void run(){
        if(!running)return;
        if(!available()){handler.postDelayed(this,250);return;}
        int packet=source.getAsInt(),channel=(packet>>>8)&255,event=(packet>>>16)&127;
        int amplitude=FeedbackEnvelope.amplitude(packet&255,channel,strength);
        if(channel==2&&amplitude>2){
            if(event!=lastImpact||!active){
                lastImpact=event;surfaceAmplitude=0;
                try{
                    play(2,amplitude,0);
                    impactQuietUntil=android.os.SystemClock.elapsedRealtime()+FeedbackEnvelope.IMPACT_WINDOW_MS;
                    active=true;android.util.Log.i("NFSU2","Physics impact waveform event="+event+" amplitude="+amplitude);
                }catch(SecurityException|IllegalArgumentException e){stop();return;}
            }
            handler.postDelayed(this,40);return;
        }
        // Let the impact's quiet tail finish before road/slip chatter returns.
        if(android.os.SystemClock.elapsedRealtime()<impactQuietUntil){handler.postDelayed(this,40);return;}
        if(amplitude>2){
            try{
                if(!active)android.util.Log.i("NFSU2","Feedback playing device="+targetDevice+" amplitude="+amplitude);
                long now=android.os.SystemClock.elapsedRealtime();
                if(!active||lastChannel!=channel||now-surfaceAt>=80){
                    play(channel,amplitude,surfaceAmplitude);
                    surfaceAmplitude=channel==0?amplitude:0;surfaceAt=now;lastChannel=channel;
                }
                active=true;
            }
            catch(SecurityException|IllegalArgumentException e){stop();return;}
        }else if(active){cancelOutput();active=false;surfaceAmplitude=0;lastChannel=-1;}
        handler.postDelayed(this,40);
    }};
    void start(){if(enabled()&&!running){running=true;handler.post(poll);}}
    void stop(){running=false;handler.removeCallbacks(poll);cancelOutput();active=false;surfaceAmplitude=0;lastChannel=-1;lastImpact=-1;impactQuietUntil=0;stopSource.run();}
    static boolean test(Context context){return test(context,2);}
    static boolean test(Context context,int channel){
        PhoneFeedback feedback=new PhoneFeedback(context,100,()->0,()->{});
        if(!feedback.available())return false;
        try{feedback.play(channel,180,0,channel==1?600:0);return true;}
        catch(SecurityException|IllegalArgumentException e){feedback.cancelOutput();return false;}
    }
}
