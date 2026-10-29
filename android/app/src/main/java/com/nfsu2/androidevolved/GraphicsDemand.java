package com.nfsu2.androidevolved;

import java.util.Map;

/** Relative configuration estimate, not measured utilization or a device performance prediction.
 * Weights describe recurring frame work; loading-only texture decoding is excluded.
 * Explicit display switches override legacy effect bundles, so those are not counted twice.
 */
final class GraphicsDemand {
    final int cpuLevel, gpuLevel;
    final double targetFps, cpuScore, gpuScore;
    private GraphicsDemand(double fps, double cpu, double gpu) {
        targetFps=fps; cpuScore=cpu; gpuScore=gpu;
        cpuLevel=tier(cpu); gpuLevel=tier(gpu);
    }
    private static int tier(double score) { return score<.8?1:score<1.5?2:score<2.5?3:4; }
    private static int value(Map<String,String> s,String key,int fallback) {
        try { return Integer.parseInt(s.get(key)); } catch (RuntimeException e) { return fallback; }
    }
    static GraphicsDemand estimate(Map<String,String> s,int width,int height,double refresh) {
        refresh=Math.max(30,refresh);
        double fps=value(s,"fps",120);
        if(fps<=0) fps=refresh;
        if("steady".equals(s.get("frame_pace"))) fps=refresh/Math.ceil(refresh/Math.min(fps,refresh));
        double scale=value(s,"resolution",75);
        double pixels=scale==720?1280.0*720/Math.max(1.0,(double)width*height):scale*scale/10000;
        double world=value(s,"world_detail",2), car=value(s,"car_detail",1);
        double crowd=value(s,"crowd",1), particles=value(s,"particles",1);
        double cpu=(.4+.15*world+.08*car+.08*crowd+.08*particles)*fps/60;
        double detail=.55+.12*world+.08*car+.06*value(s,"car_shadow",2)
                +.08*value(s,"road_reflect_detail",2)+.07*particles+.04*crowd;
        int env=value(s,"car_env_detail",1);
        if(env>0) detail+=.08*env*(1+value(s,"car_env_update",1))*(1+value(s,"reflection_quality",0));
        for(String key:new String[]{"car_headlight","light_trails","light_glow","motion_blur","fog","dof","overbright","bleach","rain"})
            detail+=.025*value(s,key,0);
        detail+=.05*value(s,"bloom",0)+.002*value(s,"sharpen",0)+.003*value(s,"filtering",0);
        if("on".equals(s.get("hd_packs")))detail+=.08;
        return new GraphicsDemand(fps,cpu,detail*pixels*fps/60);
    }
}
