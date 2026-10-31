package com.nfsu2.androidevolved;
public final class FeedbackEnvelopeTest {
    static void check(boolean okay){if(!okay)throw new AssertionError("Feedback envelope");}
    static long sum(long[] values){long n=0;for(long v:values)n+=v;return n;}
    public static void main(String[] args){
        check(FeedbackEnvelope.amplitude(0,1,100)==0);
        check(FeedbackEnvelope.amplitude(120,1,50)==90);
        check(FeedbackEnvelope.amplitude(120,0,50)==60);
        check(FeedbackEnvelope.amplitude(210,1,100)==255);
        check(FeedbackEnvelope.amplitude(200,2,50)==100);
        check(FeedbackEnvelope.amplitude(255,1,0)==0);
        check(sum(FeedbackEnvelope.timings(0))==70);
        check(sum(FeedbackEnvelope.timings(1))==110&&FeedbackEnvelope.amplitudes(1,150,0).length==1);
        check(sum(FeedbackEnvelope.timings(2))==FeedbackEnvelope.IMPACT_WINDOW_MS);
        check(FeedbackEnvelope.amplitudes(2,200,0)[2]==0);
        check(FeedbackEnvelope.timings(2)[0]>=30&&FeedbackEnvelope.timings(2)[2]>=240);
        check(FeedbackEnvelope.motorAmplitude(1,0,255)==0);
        check(FeedbackEnvelope.motorAmplitude(2,1,255)==0);
        for(int channel=0;channel<3;channel++)for(int v:FeedbackEnvelope.amplitudes(channel,0,100))check(v==0);
        check(sum(FeedbackEnvelope.binaryTimings(2,150))==FeedbackEnvelope.IMPACT_WINDOW_MS);
        check(FeedbackEnvelope.amplitudes(0,40,200)[0]<=40);
        check(FeedbackEnvelope.amplitudes(0,40,0)[2]<40);
        check(FeedbackEnvelope.binaryTimings(0,30)[1]<FeedbackEnvelope.binaryTimings(1,150)[1]);
        check(FeedbackEnvelope.motorAmplitude(1,1,200)>FeedbackEnvelope.motorAmplitude(1,0,200));
        check(FeedbackEnvelope.motorAmplitude(2,0,200)>FeedbackEnvelope.motorAmplitude(2,1,200));
        check(FeedbackEnvelope.motorAmplitude(1,1,200)>=3*FeedbackEnvelope.motorAmplitude(1,0,200));
        check(FeedbackEnvelope.motorAmplitude(2,0,200)>=3*FeedbackEnvelope.motorAmplitude(2,1,200));
        for(int channel=0;channel<3;channel++)for(int a=3;a<=255;a++){
            long[] t=FeedbackEnvelope.timings(channel);int[] v=FeedbackEnvelope.amplitudes(channel,a,20);
            check(t.length==v.length);
            for(int value:v)check(value>=0&&value<=255);
            for(long value:FeedbackEnvelope.binaryTimings(channel,a))check(value>=0);
        }
        System.out.println("Feedback envelopes passed");
    }
}
