package com.nfsu2.androidevolved;

import android.content.Context;

/**
 * Support > Test nfsu2.xyz: opens the real /lan client for a few seconds and
 * reports the virtual address and how many relayed LAN frames arrived. It uses
 * the same SwitchSocket as the game session, so a success here means the
 * transport works for LAN play too.
 */
final class Nfsu2SwitchProbe {
    interface Listener { void onStatus(String text); }

    private static final String URL = "wss://nfsu2.xyz/lan";
    private static SwitchSocket client;
    private static long frames;
    private static String address = "";

    private Nfsu2SwitchProbe() {}

    static synchronized boolean isRunning() { return client != null; }

    static synchronized void start(Context context, Listener listener) {
        if (client != null) {
            listener.onStatus("Already connected. Close first.");
            return;
        }
        frames = 0;
        address = "";
        final long[] lastReport = {System.currentTimeMillis()};
        SwitchSocket socket = new SwitchSocket(URL, "NFSU2 Android probe", new SwitchSocket.Listener() {
            @Override public void onWelcome(int vip, int mask, String vipText, String token) {
                address = vipText;
                listener.onStatus("nfsu2.xyz " + vipText + " / " + SwitchSocket.ipText(mask)
                        + " • waiting for LAN frames…");
            }

            @Override public void onFrame(byte[] frame) {
                ++frames;
                long now = System.currentTimeMillis();
                if (frames == 1 || now - lastReport[0] > 2000) {
                    lastReport[0] = now;
                    listener.onStatus("nfsu2.xyz " + (address.isEmpty() ? "" : address + " • ")
                            + frames + " LAN frame(s) received.");
                }
            }

            @Override public void onStatus(String text) {
                listener.onStatus(text);
            }

            @Override public void onClosed(String reason) {
                listener.onStatus("Switch client stopped (" + reason + ").");
            }
        });
        client = socket;
        socket.start();
    }

    static synchronized void stop() {
        SwitchSocket socket = client;
        client = null;
        if (socket != null) socket.stop();
    }
}
