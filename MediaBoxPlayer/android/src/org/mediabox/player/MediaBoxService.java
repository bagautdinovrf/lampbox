package org.mediabox.player;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;

import org.qtproject.qt.android.bindings.QtService;

/** Hosts the native player without an Activity or any playback controls. */
public final class MediaBoxService extends QtService {
    private static final String CHANNEL_ID = "mediabox_playback";
    private static final int NOTIFICATION_ID = 1;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static volatile MediaBoxService instance;

    private PowerManager.WakeLock playbackWakeLock;
    private boolean playing;

    @Override
    public void onCreate() {
        // QtService.onCreate() loads Qt and starts C++ main(). Promote first so
        // native library loading cannot miss Android's foreground deadline.
        NotificationManager notifications = getSystemService(NotificationManager.class);
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "MediaBoxPlayer", NotificationManager.IMPORTANCE_LOW);
        channel.setDescription("Фоновое воспроизведение MediaBoxPlayer");
        notifications.createNotificationChannel(channel);

        PowerManager power = getSystemService(PowerManager.class);
        playbackWakeLock = power.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK, "MediaBoxPlayer:playback");
        playbackWakeLock.setReferenceCounted(false);
        instance = this;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification(),
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        } else {
            startForeground(NOTIFICATION_ID, notification());
        }
        super.onCreate();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        super.onStartCommand(intent, flags, startId);
        // Starting this component only starts the host. Intents never carry
        // playback commands, file names, credentials, or listen addresses.
        return START_NOT_STICKY;
    }

    private Notification notification() {
        return new Notification.Builder(this, CHANNEL_ID)
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setContentTitle("MediaBoxPlayer")
                .setContentText(playing
                        ? "Воспроизведение аудио"
                        : "Ожидание команд MediaBoxManager")
                .setCategory(Notification.CATEGORY_SERVICE)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .build();
    }

    /** Called by the native backend on playback state changes. */
    public static void setPlaying(boolean active) {
        MAIN.post(() -> {
            MediaBoxService service = instance;
            if (service == null || service.playing == active)
                return;
            service.playing = active;
            if (active) {
                service.playbackWakeLock.acquire();
            } else if (service.playbackWakeLock.isHeld()) {
                service.playbackWakeLock.release();
            }
            service.getSystemService(NotificationManager.class).notify(
                    NOTIFICATION_ID, service.notification());
        });
    }

    /** Called when the native event loop exits, including startup failures. */
    public static void stop() {
        MAIN.post(() -> {
            MediaBoxService service = instance;
            if (service != null)
                service.stopSelf();
        });
    }

    @Override
    public void onDestroy() {
        instance = null;
        if (playbackWakeLock != null && playbackWakeLock.isHeld())
            playbackWakeLock.release();
        stopForeground(STOP_FOREGROUND_REMOVE);
        // QtService terminates Qt's native thread and the service process.
        super.onDestroy();
    }
}
