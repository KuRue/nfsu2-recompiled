package com.nfsu2.androidevolved;
import java.util.HashMap;
import java.util.Map;
public final class GraphicsDemandTest {
    private static GraphicsDemand demand(Map<String,String> s) {return GraphicsDemand.estimate(s,3120,1440,120);}
    private static void check(boolean value,String message) {if(!value)throw new AssertionError(message);}
    public static void main(String[] args) {
        Map<String,String> s=new HashMap<>();s.put("fps","60");s.put("resolution","50");
        GraphicsDemand low=demand(s);s.put("resolution","100");GraphicsDemand high=demand(s);
        check(high.gpuSegments>low.gpuSegments,"More segments for higher demand");
        check(high.gpuScore>low.gpuScore,"Resolution must increase GPU work");
        check(high.cpuScore==low.cpuScore,"Resolution must not inflate CPU work");
        s.put("fps","120");GraphicsDemand fast=demand(s);
        check(fast.cpuScore>high.cpuScore && fast.gpuScore>high.gpuScore,"Frame cap must affect both");
        s.put("fps","90");s.put("frame_pace","steady");check(demand(s).targetFps==60,"Steady 90 on 120 Hz");
        s.put("frame_pace","exact");check(demand(s).targetFps==90,"Exact cap");
        s.put("fps","0");check(demand(s).targetFps==120,"Uncapped estimate uses panel maximum");
        s.put("world_detail","0");GraphicsDemand sparse=demand(s);s.put("world_detail","2");
        check(demand(s).cpuScore>sparse.cpuScore && demand(s).gpuScore>sparse.gpuScore,"World detail");
        s.put("car_env_detail","0");GraphicsDemand noReflections=demand(s);s.put("reflection_quality","1");
        check(demand(s).gpuScore==noReflections.gpuScore,"Disabled car reflections ignore map multiplier");
        s.put("car_env_detail","2");check(demand(s).gpuScore>noReflections.gpuScore,"Enabled reflections cost work");
        double original=demand(s).gpuScore;s.put("night_effects","2");s.put("atmosphere","2");s.put("treatment","2");
        check(demand(s).gpuScore==original,"Do not double count overridden legacy bundles");
        s.put("resolution","720");check(Double.isFinite(demand(s).gpuScore),"720p estimate");
        System.out.println("GraphicsDemand tests passed");
    }
}
