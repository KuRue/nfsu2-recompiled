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

/** One-shot offscreen game renderer. Manifest isolates native globals from the user's :game. */
public final class VisualPreviewService extends Service {
    static { System.loadLibrary("nfsu2_android"); }
    private static native String nativeStart(android.view.Surface surface,String[] names,String[] values,
            String profile,int width,int height,String language,boolean widescreen);
    private static native boolean nativeReady();
    private final Handler main=new Handler(Looper.getMainLooper());
    private HandlerThread thread;
    private ImageReader reader;
    private ResultReceiver receiver;
    private volatile boolean finished;
    private File output;
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public int onStartCommand(Intent intent,int flags,int startId){
        if(thread!=null||intent==null)return START_NOT_STICKY;
        receiver=intent.getParcelableExtra("receiver");
        String key=intent.getStringExtra("key");
        if(key==null||!key.matches("[a-f0-9]{64}")){finish("Invalid preview request");return START_NOT_STICKY;}
        output=new File(getCacheDir(),"visual-preview-"+key+".png");
        thread=new HandlerThread("Still preview");thread.start();
        Handler worker=new Handler(thread.getLooper());
        main.postDelayed(()->finish("Preview timed out"),45000);
        worker.post(()->{
            try{
                GameActivity.nativeCpuTextures(intent.getBooleanExtra("cpuTextures",false));
                String[] driver=intent.getStringArrayExtra("driver");
                if(driver!=null&&driver.length==4)GameActivity.nativeGpuDriver(driver[0],driver[1],driver[2],driver[3]);
                reader=ImageReader.newInstance(960,540,PixelFormat.RGBA_8888,3);
                reader.setOnImageAvailableListener(source->{
                    try(Image image=source.acquireLatestImage()){
                        if(image==null||finished||!nativeReady())return;
                        Image.Plane plane=image.getPlanes()[0];
                        int stride=plane.getRowStride(),pixel=plane.getPixelStride();
                        if(pixel!=4)throw new IOException("Unsupported preview pixel format");
                        Bitmap bitmap=Bitmap.createBitmap(image.getWidth(),image.getHeight(),Bitmap.Config.ARGB_8888);
                        ByteBuffer buffer=plane.getBuffer(),packed=ByteBuffer.allocate(image.getWidth()*image.getHeight()*4);
                        for(int row=0;row<image.getHeight();row++){
                            ByteBuffer line=buffer.duplicate();line.position(row*stride);line.limit(row*stride+image.getWidth()*4);
                            packed.put(line);
                        }
                        packed.flip();bitmap.copyPixelsFromBuffer(packed);bitmap.setHasAlpha(false);
                        File temporary=new File(output.getPath()+".tmp");
                        try(FileOutputStream out=new FileOutputStream(temporary)){
                            if(!bitmap.compress(Bitmap.CompressFormat.PNG,100,out))throw new IOException("Preview encoding failed");
                        }finally{bitmap.recycle();}
                        if(!temporary.renameTo(output))throw new IOException("Preview publication failed");
                        main.post(()->finish(null));
                    }catch(Exception error){main.post(()->finish(error.getMessage()));}
                },worker);
                String error=nativeStart(reader.getSurface(),intent.getStringArrayExtra("names"),
                        intent.getStringArrayExtra("values"),new File(getCacheDir(),"preview-profile").getAbsolutePath(),
                        intent.getIntExtra("width",960),intent.getIntExtra("height",540),
                        intent.getStringExtra("language"),intent.getBooleanExtra("wide",true));
                if(error!=null)main.post(()->finish(error));
            }catch(Throwable error){main.post(()->finish(error.toString()));}
        });
        return START_NOT_STICKY;
    }
    private void finish(String error){
        if(finished)return;finished=true;
        if(receiver!=null){Bundle result=new Bundle();
            if(error==null)result.putString("path",output.getAbsolutePath());else result.putString("error",error);
            receiver.send(error==null?0:1,result);
        }
        stopSelf();
    }
    @Override public void onDestroy(){
        main.removeCallbacksAndMessages(null);
        // This service owns :preview exclusively. Never signal the shared :game or adb process.
        // Native resources and worker threads are reclaimed together after the result is delivered.
        main.postDelayed(()->android.os.Process.killProcess(android.os.Process.myPid()),100);
        super.onDestroy();
    }
}
