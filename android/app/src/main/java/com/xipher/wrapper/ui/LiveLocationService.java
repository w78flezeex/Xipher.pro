package com.xipher.wrapper.ui;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationCompat;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.data.repo.ChatRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.util.LocationUtils;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class LiveLocationService extends Service {

    public static final String EXTRA_CHAT_ID = "extra_chat_id";
    public static final String EXTRA_IS_GROUP = "extra_is_group";
    public static final String EXTRA_IS_CHANNEL = "extra_is_channel";
    public static final String EXTRA_LIVE_ID = "extra_live_id";
    public static final String EXTRA_EXPIRES_AT = "extra_expires_at";
    private static final String ACTION_STOP = "com.xipher.wrapper.action.STOP_LIVE_LOCATION";
    private static final String CHANNEL_ID = "live_location";
    private static final int NOTIF_ID = 2104;

    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private LocationManager locationManager;
    private LocationListener locationListener;
    private long expiresAt;
    private String chatId;
    private boolean isGroup;
    private boolean isChannel;
    private String liveId;
    private long lastSentAt;
    private AuthRepository authRepo;

    public static void start(Context context, String chatId, boolean isGroup, boolean isChannel,
                             String liveId, long expiresAt) {
        Intent intent = new Intent(context, LiveLocationService.class);
        intent.putExtra(EXTRA_CHAT_ID, chatId);
        intent.putExtra(EXTRA_IS_GROUP, isGroup);
        intent.putExtra(EXTRA_IS_CHANNEL, isChannel);
        intent.putExtra(EXTRA_LIVE_ID, liveId);
        intent.putExtra(EXTRA_EXPIRES_AT, expiresAt);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            context.startForegroundService(intent);
        } else {
            context.startService(intent);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_STOP.equals(intent.getAction())) {
            stopSelf();
            return START_NOT_STICKY;
        }
        chatId = intent != null ? intent.getStringExtra(EXTRA_CHAT_ID) : null;
        isGroup = intent != null && intent.getBooleanExtra(EXTRA_IS_GROUP, false);
        isChannel = intent != null && intent.getBooleanExtra(EXTRA_IS_CHANNEL, false);
        liveId = intent != null ? intent.getStringExtra(EXTRA_LIVE_ID) : null;
        expiresAt = intent != null ? intent.getLongExtra(EXTRA_EXPIRES_AT, 0L) : 0L;
        authRepo = AppServices.authRepository(this);

        if (chatId == null || liveId == null || expiresAt <= 0L) {
            stopSelf();
            return START_NOT_STICKY;
        }

        startForeground(NOTIF_ID, buildNotification());
        startLocationUpdates();
        scheduleStop();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopLocationUpdates();
        io.shutdownNow();
        handler.removeCallbacksAndMessages(null);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private Notification buildNotification() {
        ensureChannel();
        PendingIntent stopIntent = PendingIntent.getService(
                this, 0, new Intent(this, LiveLocationService.class).setAction(ACTION_STOP),
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle(getString(R.string.location_live_notification_title))
                .setContentText(getString(R.string.location_live_notification_text))
                .setSmallIcon(R.drawable.ic_location)
                .addAction(R.drawable.ic_close, getString(R.string.location_live_stop), stopIntent)
                .setOngoing(true)
                .build();
    }

    private void ensureChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;
        NotificationManager nm = getSystemService(NotificationManager.class);
        if (nm == null) return;
        if (nm.getNotificationChannel(CHANNEL_ID) != null) return;
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                getString(R.string.notification_channel_live_location),
                NotificationManager.IMPORTANCE_LOW);
        nm.createNotificationChannel(channel);
    }

    private void scheduleStop() {
        long delay = Math.max(1000L, expiresAt - System.currentTimeMillis());
        handler.postDelayed(this::stopSelf, delay);
    }

    private void startLocationUpdates() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED
                && ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            stopSelf();
            return;
        }
        locationManager = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
        if (locationManager == null) {
            stopSelf();
            return;
        }
        String provider = locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)
                ? LocationManager.GPS_PROVIDER
                : LocationManager.NETWORK_PROVIDER;
        locationListener = location -> sendLocationUpdate(location);
        try {
            locationManager.requestLocationUpdates(provider, 10000L, 10f, locationListener, Looper.getMainLooper());
        } catch (Exception e) {
            stopSelf();
            return;
        }
        Location last = null;
        try {
            last = locationManager.getLastKnownLocation(provider);
        } catch (Exception ignored) {
        }
        if (last != null) {
            sendLocationUpdate(last);
        }
    }

    private void stopLocationUpdates() {
        if (locationManager != null && locationListener != null) {
            try {
                locationManager.removeUpdates(locationListener);
            } catch (Exception ignored) {
            }
        }
        locationListener = null;
    }

    private void sendLocationUpdate(Location location) {
        if (location == null) return;
        long now = System.currentTimeMillis();
        if (now - lastSentAt < 8000L) return;
        lastSentAt = now;
        io.execute(() -> {
            try {
                String token = authRepo.getToken();
                String selfId = authRepo.getUserId();
                String selfName = authRepo.getUsername();
                if (token == null || token.isEmpty()) return;
                ChatRepository repo = new ChatRepository(this, token);
                String content = LocationUtils.buildMapUrl(
                        location.getLatitude(), location.getLongitude(), liveId, expiresAt);
                if (isGroup) {
                    repo.sendGroupMessage(token, chatId, content, "live_location", null, selfId, selfName, null, null, null);
                } else if (isChannel) {
                    repo.sendChannelMessage(token, chatId, content, "live_location", null, selfId, selfName, null, null, null);
                } else {
                    repo.sendMessage(token, chatId, content, "live_location", null, selfId);
                }
            } catch (Exception ignored) {
            }
        });
    }

}
