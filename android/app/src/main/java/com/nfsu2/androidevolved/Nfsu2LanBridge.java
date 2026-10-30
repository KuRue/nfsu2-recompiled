package com.nfsu2.androidevolved;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Runs the nfsu2.xyz /lan client for the game session and connects it to the
 * native virtual LAN. The native Winsock HLE queues switch frames; a pump
 * thread hands them to {@link SwitchSocket}; received frames go straight back
 * through JNI. Started only when the launcher's network provider is nfsu2.
 */
final class Nfsu2LanBridge {
    private static final String TAG = "NFSU2Switch";
    private static final String URL = "wss://nfsu2.xyz/lan";

    private static SwitchSocket socket;
    private static Thread pump;
    private static volatile boolean running;

    private Nfsu2LanBridge() {}

    static synchronized void start(Context context) {
        if (running) return;
        running = true;
        Context app = context.getApplicationContext();
        Handler main = new Handler(Looper.getMainLooper());
        AtomicBoolean saidWaiting = new AtomicBoolean();
        SwitchSocket client = new SwitchSocket(URL, "NFSU2 Android", new SwitchSocket.Listener() {
            @Override public void onWelcome(int vip, int mask, String vipText, String token) {
                Log.i(TAG, "on the nfsu2.xyz switch as " + vipText + " (mask " + SwitchSocket.ipText(mask) + ")");
                nativeSwitchWelcome(vip, mask);
                main.post(() -> NeonToast.show(app, "nfsu2.xyz: on the virtual LAN as " + vipText));
            }

            @Override public void onFrame(byte[] frame) {
                nativeSwitchFrame(frame);
            }

            @Override public void onStatus(String text) {
                Log.i(TAG, text);
                if (text.startsWith("The switch is full") && saidWaiting.compareAndSet(false, true))
                    main.post(() -> NeonToast.show(app, text));
            }

            @Override public void onClosed(String reason) {
                Log.i(TAG, "switch client stopped: " + reason);
                nativeSwitchClosed(reason);
            }
        });
        socket = client;
        client.start();
        Thread thread = new Thread(Nfsu2LanBridge::pumpOut, "nfsu2-switch-out");
        pump = thread;
        thread.start();
    }

    static synchronized void stop() {
        if (!running) return;
        running = false;
        SwitchSocket client = socket;
        socket = null;
        if (client != null) client.stop();
        Thread thread = pump;
        pump = null;
        if (thread != null) thread.interrupt();
        nativeSwitchClosed("stopped");
    }

    private static void pumpOut() {
        while (running) {
            byte[] frame = nativeSwitchPoll(500);
            if (frame == null) continue;
            SwitchSocket client = socket;
            if (client != null) client.send(frame);
        }
    }

    private static native void nativeSwitchWelcome(int vip, int mask);
    private static native void nativeSwitchFrame(byte[] frame);
    private static native void nativeSwitchClosed(String reason);
    private static native byte[] nativeSwitchPoll(int timeoutMs);
}
