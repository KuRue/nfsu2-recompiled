package com.nfsu2.androidevolved;

/** Output envelopes; physics determines the event, this only shapes the motor cue. */
final class FeedbackEnvelope {
    static int amplitude(int level,int channel,int strength){
        // Grip loss needs a clearer cue than road texture, without amplifying hits.
        int gain=channel==1?150:100;
        return Math.min(255,Math.max(0,level)*Math.max(0,strength)*gain/10000);
    }
    static long[] timings(int channel){
        if(channel==2)return new long[]{15,65,45,35};
        if(channel==1)return new long[]{45,35}; // Grip chatter, not an unbroken buzz.
        return new long[]{12,24,34}; // Short, tapered curb/road pulse.
    }
    static int[] amplitudes(int channel,int amplitude,int previous){
        if(channel==2)return new int[]{0,amplitude,0,Math.max(1,amplitude*2/3)};
        if(channel==1)return new int[]{amplitude,0};
        return new int[]{Math.min(amplitude,Math.max(1,previous)),amplitude,Math.max(1,amplitude/3)};
    }
    // Android evdev channel0 is strong rumble, channel1 weak rumble.
    // Keep curb texture restrained; give slip its own finer chatter balance.
    static int motorAmplitude(int channel,int motor,int amplitude){
        int gain=channel==1?(motor==0?65:100):channel==2?(motor==0?100:70):(motor==0?65:20);
        return amplitude*gain/100;
    }
    // Binary motors cannot represent low amplitude. Encode intensity as short
    // on-time instead of giving a tiny curb the same pulse as a strong slide.
    static long[] binaryTimings(int channel,int amplitude){
        if(channel==2)return new long[]{15,Math.max(8,65*amplitude/255),45,Math.max(8,35*amplitude/255)};
        int on=Math.max(4,(channel==1?45:24)*amplitude/255);
        return new long[]{0,on,80-on};
    }
}
