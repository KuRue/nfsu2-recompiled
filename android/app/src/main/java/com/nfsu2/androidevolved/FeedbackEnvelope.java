package com.nfsu2.androidevolved;

/** Output envelopes; physics determines the event, this only shapes the motor cue. */
final class FeedbackEnvelope {
    static int amplitude(int level,int channel,int strength){
        // Grip loss needs a clearer cue than road texture, without amplifying hits.
        int gain=channel==1?150:100;
        return Math.min(255,Math.max(0,level)*Math.max(0,strength)*gain/10000);
    }
    static final int IMPACT_WINDOW_MS=360;
    static long[] timings(int channel){
        if(channel==2)return new long[]{30,85,245};
        if(channel==1)return new long[]{110}; // Sustained light-motor texture, separate from isolated hits.
        return new long[]{12,24,34}; // Short, tapered curb/road pulse.
    }
    static int[] amplitudes(int channel,int amplitude,int previous){
        if(amplitude<=0)return new int[timings(channel).length];
        if(channel==2)return new int[]{0,amplitude,0};
        if(channel==1)return new int[]{amplitude};
        return new int[]{Math.min(amplitude,Math.max(1,previous)),amplitude,Math.max(1,amplitude/3)};
    }
    // Android evdev channel0 is strong rumble, channel1 weak rumble.
    // Keep curb texture restrained; give slip its own finer chatter balance.
    static int motorAmplitude(int channel,int motor,int amplitude){
        int gain=channel==1?(motor==0?0:100):channel==2?(motor==0?100:0):(motor==0?65:20);
        return amplitude*gain/100;
    }
    // Binary motors cannot represent low amplitude. Encode intensity as short
    // on-time instead of giving a tiny curb the same pulse as a strong slide.
    static long[] binaryTimings(int channel,int amplitude){
        if(channel==2){int on=Math.max(8,85*amplitude/255);return new long[]{30,on,IMPACT_WINDOW_MS-30-on};}
        int on=Math.max(4,(channel==1?110:24)*amplitude/255);
        return new long[]{0,on,Math.max(0,(channel==1?120:80)-on)};
    }
}
