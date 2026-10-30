package com.nfsu2.androidevolved;

import android.content.*;
import android.os.*;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;

/**
 * Launcher side of the live visual preview. One preview game runs in an isolated :preview
 * process. Requests are single-flight: a setting changed while the game is still booting is
 * remembered and applied once the first still arrives, never by starting a second game over
 * the first. Base settings that the game only reads at startup require a stop, a settle wait
 * and a restart. The previous still and a persistent last still are shown throughout, so the
 * pane never blanks while a new render is prepared.
 */
final class VisualPreview {
    interface Listener {void changed(String path,String status);}
    private static final String SCENE="showcase-v1";
    private static final long START_DEBOUNCE=450, APPLY_DEBOUNCE=150, STOP_SETTLE=1200, WARM_TIMEOUT=60000;
    private final Context context;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final Listener listener;
    private final File stableStill;
    private String[] names,values;
    private Intent startRequest;
    private String wantedKey,wantedBase,runningBase,lastPath,lastStatus="Preparing…";
    private boolean visible,starting,ready,stopping,failed;
    private long sentAt;
    private final Runnable dispatch=this::dispatch,timeout=this::timedOut,settle=this::settle,warmStop=this::warmStop;
    private final ResultReceiver results;

    VisualPreview(Context context,Listener listener){
        this.context=context;this.listener=listener;
        stableStill=new File(context.getCacheDir(),"visual-preview-last.jpg");
        if(stableStill.isFile())lastPath=stableStill.getAbsolutePath();
        results=new ResultReceiver(main){@Override protected void onReceiveResult(int code,Bundle data){received(code,data);}};
    }

    static File still(Context context,String key){return new File(context.getCacheDir(),"visual-preview-"+key+".jpg");}

    /** Recompute the current settings and schedule one start/apply/restart. */
    void request(){
        visible=true;main.removeCallbacks(warmStop);main.removeCallbacks(dispatch);main.removeCallbacks(timeout);
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
        String base=Arrays.toString(size)+"|"+language+"|"+wide+"|"+cpu+"|"+Arrays.toString(driver)
                +"|"+GameOptions.get(context,"hd_packs")+"|"+GameOptions.get(context,"filtering")+"|"+GameOptions.get(context,"reflection_quality")
                +"|"+GameOptions.displayOverrides(context);
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
        if(cache.isFile()){show(cache.getAbsolutePath(),"Preview");return;}
        if(lastPath==null&&stableStill.isFile())lastPath=stableStill.getAbsolutePath();
        show(lastPath,lastPath==null?"Preparing…":"Updating…");
        if(failed||starting||stopping)return;
        if(ready){
            if(!wantedBase.equals(runningBase)){stop();return;}
            main.postDelayed(dispatch,APPLY_DEBOUNCE);
            return;
        }
        main.postDelayed(dispatch,START_DEBOUNCE);
    }

    private void show(String path,String status){lastPath=path;lastStatus=status;listener.changed(path,status);}

    private void dispatch(){
        if(!visible||wantedKey==null||failed||starting||stopping)return;
        if(ready){
            if(!wantedBase.equals(runningBase)){stop();return;}
            try{
                context.startService(new Intent(context,VisualPreviewService.class).setAction(VisualPreviewService.APPLY)
                        .putExtra("names",names).putExtra("values",values).putExtra("key",wantedKey).putExtra("receiver",results));
                arm(15000);
            }catch(RuntimeException error){fail(error);}
            return;
        }
        try{
            android.app.ActivityManager.MemoryInfo memory=new android.app.ActivityManager.MemoryInfo();
            context.getSystemService(android.app.ActivityManager.class).getMemoryInfo(memory);
            if(memory.lowMemory||memory.availMem<1536L*1024*1024){show(lastPath,"Low memory");return;}
            trimCache();
            starting=true;runningBase=wantedBase;
            context.startService(new Intent(startRequest).putExtra("names",names).putExtra("values",values)
                    .putExtra("key",wantedKey).putExtra("receiver",results));
            arm(45000);
        }catch(RuntimeException error){starting=false;fail(error);}
    }

    private void received(int code,Bundle data){
        if(code!=0){
            android.util.Log.w("NFSPreview",data.getString("error","Unknown preview failure"));
            main.removeCallbacks(timeout);starting=false;ready=false;stopping=false;runningBase=null;failed=true;
            show(lastPath,"Preview unavailable");return;
        }
        String key=data.getString("key"),path=data.getString("path");
        android.util.Log.i("NFSPreview","still for "+(key==null?"?":key.substring(0,8))+" after "+(SystemClock.uptimeMillis()-sentAt)+" ms");
        if(path==null)return;
        main.removeCallbacks(timeout);
        boolean first=starting;
        if(first){starting=false;ready=true;}
        if(key!=null&&key.equals(wantedKey)){show(path,"Preview");copyStable(path);return;}
        if(first){
            // Settings changed while the preview was booting: show the real still, then update once.
            show(path,"Updating…");
            if(wantedKey!=null&&!wantedKey.equals(key)){
                main.removeCallbacks(dispatch);
                if(wantedBase.equals(runningBase))main.postDelayed(dispatch,300);
                else stop();
            }
        }
    }

    private void fail(RuntimeException error){
        android.util.Log.w("NFSPreview","preview service call failed",error);
        starting=false;ready=false;stopping=false;runningBase=null;failed=true;
        show(lastPath,"Preview unavailable");
    }

    private void arm(long millis){sentAt=SystemClock.uptimeMillis();main.removeCallbacks(timeout);main.postDelayed(timeout,millis);}

    private void timedOut(){
        android.util.Log.w("NFSPreview","preview timed out after "+(SystemClock.uptimeMillis()-sentAt)+" ms");
        stop();failed=true;show(lastPath,"Preview unavailable");
    }

    private void stop(){
        main.removeCallbacks(dispatch);main.removeCallbacks(timeout);
        boolean active=starting||ready;
        if(active)context.stopService(new Intent(context,VisualPreviewService.class));
        starting=false;ready=false;runningBase=null;
        if(active||stopping){stopping=true;main.removeCallbacks(settle);main.postDelayed(settle,STOP_SETTLE);}
    }

    private void settle(){stopping=false;if(visible&&wantedKey!=null&&!failed)main.postDelayed(dispatch,50);}
    private void warmStop(){if(!visible)stop();}

    /** Leaving Visual settings keeps the paused preview warm for a minute; returning is instant. */
    void leave(){visible=false;main.removeCallbacks(dispatch);main.removeCallbacks(timeout);main.removeCallbacks(warmStop);main.postDelayed(warmStop,WARM_TIMEOUT);}

    void hide(){visible=false;main.removeCallbacks(dispatch);main.removeCallbacks(timeout);main.removeCallbacks(warmStop);stop();}

    void retry(){
        failed=false;still(context,wantedKey).delete();stableStill.delete();lastPath=null;show(null,"Preparing…");stop();request();
    }

    private void copyStable(String path){
        try{
            File source=new File(path);
            if(!source.getAbsolutePath().equals(stableStill.getAbsolutePath()))
                java.nio.file.Files.copy(source.toPath(),stableStill.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        }catch(Exception ignored){}
    }

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
