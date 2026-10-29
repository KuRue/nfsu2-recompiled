package com.nfsu2.androidevolved;

import android.content.*;
import android.os.*;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;

/** Serializes expensive one-shot renders; stale results never replace current settings. */
final class VisualPreview {
    interface Listener {void changed(String path,String status);}
    private final Context context;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final Listener listener;
    private Intent wanted;
    private String wantedKey,activeKey,lastPath,lastStatus="Preparing…";
    private boolean visible;
    private final Runnable launch=this::launch;
    VisualPreview(Context context,Listener listener){this.context=context;this.listener=listener;}
    void request(){
        visible=true;
        Intent request=new Intent(context,VisualPreviewService.class);
        List<String> names=new ArrayList<>(),values=new ArrayList<>();
        String[][] mapping={{"hd_packs","NFSU2_NATIVE_HD"},{"filtering","NFSU2_NATIVE_AF"},
            {"sharpen","NFSU2_NATIVE_SHARPNESS"},{"lighting_look","NFSU2_NATIVE_LOOK"},
            {"reflection_quality","NFSU2_NATIVE_REFLECTIONS"},{"bloom","NFSU2_NATIVE_BLOOM"},
            {"night_effects","NFSU2_NATIVE_EFFECTS"},{"atmosphere","NFSU2_NATIVE_ATMOSPHERE"},
            {"treatment","NFSU2_NATIVE_TREATMENT"}};
        for(String[] entry:mapping){names.add(entry[1]);String value=GameOptions.get(context,entry[0]);
            values.add(value.equals("on")?"1":value.equals("off")?"0":value);}
        names.add("NFSU2_NATIVE_TEXROOT");values.add("/storage/emulated/0/nfsu2/TEXPACKS");
        names.add("NFSU2_NATIVE_LOCK_DISPLAY");values.add("1");
        names.add("NFSU2_DISPLAY_OVERRIDES");values.add(GameOptions.displayOverrides(context));
        names.add("DXVK_STATE_CACHE");values.add("0");
        int[] size=GameOptions.renderSize(context,960,540);
        String language=GameOptions.language(context);
        boolean wide=GameOptions.get(context,GameOptions.WIDESCREEN).equals("on");
        String[] driver=GpuDrivers.loaderArguments(context);
        boolean cpu=GameOptions.get(context,GameOptions.TEXTURES).equals("cpu");
        // Include renderer build and asset metadata. No bundled/copyrighted game images.
        String identity="still-v3|"+BuildConfig.VERSION_CODE+"|"+BuildConfig.VERSION_NAME+"|"+names+values
                +"|"+Arrays.toString(size)+"|"+language+"|"+wide+"|"+cpu+"|"+Arrays.toString(driver);
        for(String name:new String[]{"SPEED2.EXE","TEXPACKS","GLOBAL","CARS","TRACKS"}){
            File asset=new File("/storage/emulated/0/nfsu2",name);identity+="|"+asset.lastModified()+":"+asset.length();}
        try{identity+="|"+context.getPackageManager().getPackageInfo(context.getPackageName(),0).lastUpdateTime;}
        catch(android.content.pm.PackageManager.NameNotFoundException impossible){throw new IllegalStateException(impossible);}
        String key=hash(identity);
        wanted=request.putExtra("names",names.toArray(new String[0])).putExtra("values",values.toArray(new String[0]))
                .putExtra("width",size[0]).putExtra("height",size[1]).putExtra("language",language)
                .putExtra("wide",wide).putExtra("cpuTextures",cpu).putExtra("driver",driver).putExtra("key",key);
        if(!key.equals(wantedKey)){wantedKey=key;lastPath=null;lastStatus="Preparing…";}
        File cache=new File(context.getCacheDir(),"visual-preview-"+key+".png");
        if(cache.isFile()){lastPath=cache.getAbsolutePath();lastStatus="Preview";}
        listener.changed(lastPath,lastStatus);
        main.removeCallbacks(launch);
        if(lastPath==null&&activeKey==null)main.postDelayed(launch,600);
    }
    private void launch(){
        if(!visible||activeKey!=null||wanted==null)return;
        android.app.ActivityManager.MemoryInfo memory=new android.app.ActivityManager.MemoryInfo();
        context.getSystemService(android.app.ActivityManager.class).getMemoryInfo(memory);
        if(memory.lowMemory||memory.availMem<1536L*1024*1024){lastStatus="Low memory";listener.changed(null,lastStatus);return;}
        // Bound this feature's own disposable disk cache, including prior app installs.
        File[] cached=context.getCacheDir().listFiles((dir,name)->name.matches("visual-preview-[a-f0-9]{64}\\.png"));
        if(cached!=null){Arrays.sort(cached,Comparator.comparingLong(File::lastModified).reversed());
            for(int i=15;i<cached.length;i++)cached[i].delete();}
        activeKey=wantedKey;
        final String key=activeKey;
        wanted.putExtra("receiver",new ResultReceiver(main){@Override protected void onReceiveResult(int code,Bundle data){
            if(!key.equals(activeKey))return;
            main.removeCallbacks(timeout);activeKey=null;
            if(key.equals(wantedKey)){
                lastPath=data.getString("path");lastStatus=lastPath==null?"Preview unavailable":"Preview";
                if(code!=0)android.util.Log.w("NFSPreview",data.getString("error","Unknown preview failure"));
                if(visible)listener.changed(lastPath,lastStatus);
            }
            // The prior isolated renderer exits before the next request starts.
            if(visible&&lastPath==null&&!key.equals(wantedKey))main.postDelayed(launch,500);
        }});
        try{context.startService(wanted);main.postDelayed(timeout,47000);}
        catch(RuntimeException error){activeKey=null;lastStatus="Preview unavailable";listener.changed(null,lastStatus);}
    }
    void retry(){
        if(wantedKey!=null)new File(context.getCacheDir(),"visual-preview-"+wantedKey+".png").delete();
        lastPath=null;request();
    }
    private final Runnable timeout=this::timedOut;
    private void timedOut(){
        if(activeKey==null)return;
        String failed=activeKey;
        context.stopService(new Intent(context,VisualPreviewService.class));activeKey=null;
        if(!failed.equals(wantedKey)){if(visible)main.postDelayed(launch,500);return;}
        lastStatus="Preview unavailable";if(visible)listener.changed(null,lastStatus);
    }
    void hide(){visible=false;main.removeCallbacksAndMessages(null);
        if(activeKey!=null)context.stopService(new Intent(context,VisualPreviewService.class));activeKey=null;}
    private static String hash(String value){try{
        byte[] bytes=MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
        StringBuilder result=new StringBuilder();for(byte b:bytes)result.append(String.format(Locale.ROOT,"%02x",b&255));return result.toString();
    }catch(Exception error){throw new IllegalStateException(error);}}
}
