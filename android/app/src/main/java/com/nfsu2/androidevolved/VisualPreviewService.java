package com.nfsu2.androidevolved;

import android.app.Service;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.media.Image;
import android.media.ImageReader;
import android.os.*;
import java.io.*;
import java.nio.ByteBuffer;
import java.util.TreeMap;

/**
 * Offscreen preview game in the :preview process, isolated from the user's :game. START boots it
 * with a set of settings; APPLY changes settings while it runs. Each request is a generation; the
 * first frame at or past a generation's settled point is stored as that request's still. Between
 * requests the game is paused. The process exits when the launcher stops the service.
 */
public final class VisualPreviewService extends Service {
    static final String START="start",APPLY="apply";
    static { System.loadLibrary("nfsu2_android"); }
    private static native String nativeStart(android.view.Surface surface,String[] names,String[] values,
            String profile,int width,int height,String language,boolean widescreen);
    private static native int nativeApply(String[] names,String[] values);
    private static native int nativeReadyGeneration();
    private static native void nativePause(boolean paused);
    private final Handler main=new Handler(Looper.getMainLooper());
    private HandlerThread thread;
    private Handler worker;
    private ImageReader reader;
    private ResultReceiver receiver;
    private boolean started,failed;
    /** generation -> settings key, for requests still waiting for their still (worker thread). */
    private final TreeMap<Integer,String> pending=new TreeMap<>();

    @Override public IBinder onBind(Intent intent){return null;}

    @Override public int onStartCommand(Intent intent,int flags,int startId){
        if(intent==null)return START_NOT_STICKY;
        String key=intent.getStringExtra("key");
        ResultReceiver from=intent.getParcelableExtra("receiver");
        if(key==null||!key.matches("[a-f0-9]{64}")||from==null)return START_NOT_STICKY;
        receiver=from;
        if(APPLY.equals(intent.getAction())){
            if(!started||failed){fail("Preview is not running");return START_NOT_STICKY;}
            String[] names=intent.getStringArrayExtra("names"),values=intent.getStringArrayExtra("values");
            worker.post(()->{
                int generation=nativeApply(names,values);
                pending.put(generation,key);nativePause(false);
            });
            return START_NOT_STICKY;
        }
        if(started)return START_NOT_STICKY; // one game per process; the launcher restarts it to change base settings
        started=true;
        thread=new HandlerThread("Preview stills");thread.start();worker=new Handler(thread.getLooper());
        worker.post(()->{
            try{
                GameActivity.nativeCpuTextures(intent.getBooleanExtra("cpuTextures",false));
                String[] driver=intent.getStringArrayExtra("driver");
                if(driver!=null&&driver.length==4)GameActivity.nativeGpuDriver(driver[0],driver[1],driver[2],driver[3]);
                reader=ImageReader.newInstance(960,540,PixelFormat.RGBA_8888,3);
                reader.setOnImageAvailableListener(this::frame,worker);
                pending.put(1,key);
                String error=nativeStart(reader.getSurface(),intent.getStringArrayExtra("names"),
                        intent.getStringArrayExtra("values"),new File(getCacheDir(),"preview-profile").getAbsolutePath(),
                        intent.getIntExtra("width",960),intent.getIntExtra("height",540),
                        intent.getStringExtra("language"),intent.getBooleanExtra("wide",true));
                if(error!=null)fail(error);
            }catch(Throwable error){fail(error.toString());}
        });
        return START_NOT_STICKY;
    }

    /** Worker thread: each presented frame. Store it for the newest request it satisfies. */
    private void frame(ImageReader source){
        try(Image image=source.acquireLatestImage()){
            if(image==null||failed||pending.isEmpty())return;
            int ready=nativeReadyGeneration();
            if(ready<pending.firstKey())return;
            java.util.SortedMap<Integer,String> satisfied=pending.headMap(ready,true);
            String key=satisfied.get(satisfied.lastKey());satisfied.clear();
            File output=VisualPreview.still(this,key);
            write(image,output);
            Bundle result=new Bundle();result.putString("path",output.getAbsolutePath());result.putString("key",key);
            receiver.send(0,result);
            if(pending.isEmpty())nativePause(true);
        }catch(Exception error){fail(String.valueOf(error.getMessage()));}
    }

    private static void write(Image image,File output) throws IOException {
        Image.Plane plane=image.getPlanes()[0];
        int stride=plane.getRowStride(),pixel=plane.getPixelStride();
        if(pixel!=4)throw new IOException("Unsupported preview pixel format");
        Bitmap bitmap=Bitmap.createBitmap(image.getWidth(),image.getHeight(),Bitmap.Config.ARGB_8888);
        ByteBuffer buffer=plane.getBuffer(),packed=ByteBuffer.allocate(image.getWidth()*image.getHeight()*4);
        for(int row=0;row<image.getHeight();row++){
            ByteBuffer line=buffer.duplicate();line.position(row*stride);line.limit(row*stride+image.getWidth()*4);packed.put(line);
        }
        packed.flip();bitmap.copyPixelsFromBuffer(packed);bitmap.setHasAlpha(false);
        File temporary=new File(output.getPath()+".tmp");
        try(FileOutputStream out=new FileOutputStream(temporary)){
            if(!bitmap.compress(Bitmap.CompressFormat.JPEG,92,out))throw new IOException("Preview encoding failed");
        }finally{bitmap.recycle();}
        if(!temporary.renameTo(output))throw new IOException("Preview publication failed");
    }

    private void fail(String error){
        main.post(()->{
            if(failed)return;failed=true;
            if(receiver!=null){Bundle result=new Bundle();result.putString("error",error);receiver.send(1,result);}
            stopSelf();
        });
    }

    @Override public void onDestroy(){
        main.removeCallbacksAndMessages(null);
        // This service owns :preview exclusively. Never signal the shared :game or adb process.
        main.postDelayed(()->android.os.Process.killProcess(android.os.Process.myPid()),100);
        super.onDestroy();
    }
}
