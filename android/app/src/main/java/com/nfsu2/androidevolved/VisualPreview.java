package com.nfsu2.androidevolved;

import android.content.*;
import android.os.*;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;

/**
 * Launcher side of the live visual preview. While the visual settings are open, one preview game
 * keeps running in its own :preview process with the car under the game's showcase camera. A
 * change is sent to it and applied live; it replies with a still once the change is on screen.
 * Settings the game reads only at startup (resolution, HD packs, filtering, reflections, language,
 * widescreen, driver, CPU textures) restart it. Every still is cached by its complete settings.
 */
final class VisualPreview {
    interface Listener {void changed(String path,String status);}
    private static final String SCENE="showcase-v1";
    private final Context context;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final Listener listener;
    private String[] names,values;
    private Intent startRequest;
    private String wantedKey,wantedBase,runningBase,lastPath,lastStatus="Preparing…";
    private boolean visible,running;
    private long sentAt;
    private final Runnable dispatch=this::dispatch,timeout=this::timedOut;
    private final ResultReceiver results;

    VisualPreview(Context context,Listener listener){
        this.context=context;this.listener=listener;
        results=new ResultReceiver(main){@Override protected void onReceiveResult(int code,Bundle data){received(code,data);}};
    }

    static File still(Context context,String key){return new File(context.getCacheDir(),"visual-preview-"+key+".jpg");}

    void request(){
        visible=true;
        List<String> n=new ArrayList<>(),v=new ArrayList<>();
        String[][] mapping={{"hd_packs","NFSU2_NATIVE_HD"},{"filtering","NFSU2_NATIVE_AF"},
            {"sharpen","NFSU2_NATIVE_SHARPNESS"},{"lighting_look","NFSU2_NATIVE_LOOK"},
            {"reflection_quality","NFSU2_NATIVE_REFLECTIONS"},{"bloom","NFSU2_NATIVE_BLOOM"},
            {"night_effects","NFSU2_NATIVE_EFFECTS"},{"atmosphere","NFSU2_NATIVE_ATMOSPHERE"},
            {"treatment","NFSU2_NATIVE_TREATMENT"}};
        for(String[] entry:mapping){n.add(entry[1]);String value=GameOptions.get(context,entry[0]);
            v.add(value.equals("on")?"1":value.equals("off")?"0":value);}
        n.add("NFSU2_NATIVE_TEXROOT");v.add("/storage/emulated/0/nfsu2/TEXPACKS");
        n.add("NFSU2_NATIVE_LOCK_DISPLAY");v.add("1");
        n.add("NFSU2_DISPLAY_OVERRIDES");v.add(GameOptions.displayOverrides(context));
        n.add("DXVK_STATE_CACHE");v.add("0");
        names=n.toArray(new String[0]);values=v.toArray(new String[0]);
        int[] size=GameOptions.renderSize(context,960,540);
        String language=GameOptions.language(context);
        boolean wide=GameOptions.get(context,GameOptions.WIDESCREEN).equals("on");
        String[] driver=GpuDrivers.loaderArguments(context);
        boolean cpu=GameOptions.get(context,GameOptions.TEXTURES).equals("cpu");
        // What the running game cannot change: a difference here restarts it.
        String base=Arrays.toString(size)+"|"+language+"|"+wide+"|"+cpu+"|"+Arrays.toString(driver)
                +"|"+GameOptions.get(context,"hd_packs")+"|"+GameOptions.get(context,"filtering")+"|"+GameOptions.get(context,"reflection_quality");
        String identity=SCENE+"|"+BuildConfig.VERSION_CODE+"|"+BuildConfig.VERSION_NAME+"|"+Arrays.toString(names)+Arrays.toString(values)+"|"+base;
        for(String name:new String[]{"SPEED2.EXE","TEXPACKS","GLOBAL","CARS","TRACKS"}){
            File asset=new File("/storage/emulated/0/nfsu2",name);identity+="|"+asset.lastModified()+":"+asset.length();}
        try{identity+="|"+context.getPackageManager().getPackageInfo(context.getPackageName(),0).lastUpdateTime;}
        catch(android.content.pm.PackageManager.NameNotFoundException impossible){throw new IllegalStateException(impossible);}
        wantedKey=hash(identity);wantedBase=base;
        startRequest=new Intent(context,VisualPreviewService.class).setAction(VisualPreviewService.START)
                .putExtra("width",size[0]).putExtra("height",size[1]).putExtra("language",language)
                .putExtra("wide",wide).putExtra("cpuTextures",cpu).putExtra("driver",driver);
        File cache=still(context,wantedKey);
        main.removeCallbacks(dispatch);
        if(cache.isFile()){lastPath=cache.getAbsolutePath();lastStatus="Preview";listener.changed(lastPath,lastStatus);return;}
        // Keep showing the last still while the next one is prepared.
        if(lastPath==null){lastStatus="Preparing…";listener.changed(null,lastStatus);}
        main.postDelayed(dispatch,running&&wantedBase.equals(runningBase)?150:400);
    }

    private void dispatch(){
        if(!visible||wantedKey==null)return;
        try{
            if(running&&wantedBase.equals(runningBase)){
                context.startService(new Intent(context,VisualPreviewService.class).setAction(VisualPreviewService.APPLY)
                        .putExtra("names",names).putExtra("values",values).putExtra("key",wantedKey).putExtra("receiver",results));
                arm(10000);return;
            }
            android.app.ActivityManager.MemoryInfo memory=new android.app.ActivityManager.MemoryInfo();
            context.getSystemService(android.app.ActivityManager.class).getMemoryInfo(memory);
            if(memory.lowMemory||memory.availMem<1536L*1024*1024){lastStatus="Low memory";listener.changed(lastPath,lastStatus);return;}
            trimCache();
            if(running){stop();main.postDelayed(dispatch,300);return;} // the old game exits first
            running=true;runningBase=wantedBase;
            context.startService(new Intent(startRequest).putExtra("names",names).putExtra("values",values)
                    .putExtra("key",wantedKey).putExtra("receiver",results));
            arm(45000);
        }catch(RuntimeException error){running=false;lastStatus="Preview unavailable";listener.changed(lastPath,lastStatus);}
    }

    private void arm(long millis){sentAt=SystemClock.uptimeMillis();main.removeCallbacks(timeout);main.postDelayed(timeout,millis);}

    private void received(int code,Bundle data){
        if(code!=0){
            android.util.Log.w("NFSPreview",data.getString("error","Unknown preview failure"));
            main.removeCallbacks(timeout);running=false;
            if(visible){lastStatus="Preview unavailable";listener.changed(lastPath,lastStatus);}
            return;
        }
        String key=data.getString("key"),path=data.getString("path");
        android.util.Log.i("NFSPreview","still for "+(key==null?"?":key.substring(0,8))+" after "+(SystemClock.uptimeMillis()-sentAt)+" ms");
        if(key==null||!key.equals(wantedKey)||path==null)return; // an older change; the newer one is coming
        main.removeCallbacks(timeout);
        lastPath=path;lastStatus="Preview";
        if(visible)listener.changed(lastPath,lastStatus);
    }

    void retry(){
        if(wantedKey!=null)still(context,wantedKey).delete();
        lastPath=null;stop();request();
    }

    private void timedOut(){
        stop();
        if(visible){lastStatus="Preview unavailable";listener.changed(lastPath,lastStatus);}
    }

    private void stop(){
        if(running)context.stopService(new Intent(context,VisualPreviewService.class));
        running=false;runningBase=null;main.removeCallbacks(timeout);
    }

    void hide(){visible=false;main.removeCallbacks(dispatch);stop();}

    /** Bound this feature's own disposable cache, including stills from earlier installs. */
    private void trimCache(){
        File[] cached=context.getCacheDir().listFiles((dir,name)->name.matches("visual-preview-[a-f0-9]{64}\\.(jpg|png)"));
        if(cached==null)return;
        Arrays.sort(cached,Comparator.comparingLong(File::lastModified).reversed());
        for(int i=40;i<cached.length;i++)cached[i].delete();
        for(File file:cached)if(file.getName().endsWith(".png"))file.delete();
    }

    private static String hash(String value){try{
        byte[] bytes=MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
        StringBuilder result=new StringBuilder();for(byte b:bytes)result.append(String.format(Locale.ROOT,"%02x",b&255));return result.toString();
    }catch(Exception error){throw new IllegalStateException(error);}}
}
