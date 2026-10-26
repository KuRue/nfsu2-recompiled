package com.nfsu2.androidevolved;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/**
 * Keeps the :game process (and the paused guest machine in it) alive after the
 * player leaves the app, so Resume returns to the same live session. Dies with the
 * process; a fresh start from the launcher kills both deliberately.
 */
public final class KeepAliveService extends Service {
    private static final String CHANNEL = "session";

    @Override public void onCreate() {
        super.onCreate();
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel(CHANNEL,
                "Kept game session", NotificationManager.IMPORTANCE_LOW));
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if("end".equals(intent==null?null:intent.getAction())){
            stopForeground(STOP_FOREGROUND_REMOVE);stopSelf();
            android.os.Process.killProcess(android.os.Process.myPid());return START_NOT_STICKY;
        }
        Intent open = new Intent(this, GameActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        PendingIntent tap = PendingIntent.getActivity(this, 0, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification notice = new Notification.Builder(this, CHANNEL)
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setContentTitle("NFSU2 session kept")
                .setContentText("Paused session in memory. Tap to return.")
                .addAction(new Notification.Action.Builder(null,"End session",PendingIntent.getService(this,1,
                    new Intent(this,KeepAliveService.class).setAction("end"),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT)).build())
                .setContentIntent(tap)
                .setOngoing(true)
                .build();
        startForeground(1, notice);
        return START_NOT_STICKY;
    }

    @Override public IBinder onBind(Intent intent) { return null; }
}
