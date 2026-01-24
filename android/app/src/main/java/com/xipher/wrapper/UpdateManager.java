package com.xipher.wrapper;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import java.io.IOException;
import java.util.Locale;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * In-app update checker.
 * Fetches remote JSON: {"versionCode":2,"versionName":"1.2.3","changelog":"...","storeUrl":"https://www.rustore.ru/catalog/app/com.example"}
 */
public class UpdateManager {

    public interface Listener {
        void onNoUpdate();
        void onUpdateAvailable(JSONObject payload);
        void onError(String message);
    }

    private static final String CHANNEL_ID = "updates";
    private static final String PREFS_NAME = "updates_prefs";
    private static final String PREF_LAST_NOTIFIED = "last_notified_version";
    private static final String RU_STORE_URL_TEMPLATE = "https://www.rustore.ru/catalog/app/%s";

    private final Context context;
    private final OkHttpClient client = new OkHttpClient();
    private final String checkUrl;
    private Listener listener;

    public UpdateManager(@NonNull Context context, @NonNull String checkUrl) {
        this.context = context.getApplicationContext();
        this.checkUrl = checkUrl;
        ensureChannel();
    }

    public void setListener(@Nullable Listener listener) {
        this.listener = listener;
    }

    public void checkForUpdate() {
        Request request = new Request.Builder().url(checkUrl).build();
        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(@NonNull Call call, @NonNull IOException e) {
                notifyError(context.getString(R.string.update_check_failed, safeMessage(e)));
            }

            @Override
            public void onResponse(@NonNull Call call, @NonNull Response response) throws IOException {
                if (!response.isSuccessful()) {
                    notifyError(context.getString(R.string.update_http_error, response.code()));
                    return;
                }
                String body = response.body() != null ? response.body().string() : "";
                try {
                    JSONObject json = new JSONObject(body);
                    int remoteCode = json.optInt("versionCode", 0);
                    if (remoteCode > getCurrentVersionCode()) {
                        maybeNotifyUpdate(remoteCode, json);
                        if (listener != null) listener.onUpdateAvailable(json);
                    } else if (listener != null) {
                        listener.onNoUpdate();
                    }
                } catch (JSONException e) {
                    notifyError(context.getString(R.string.update_json_error, safeMessage(e)));
                }
            }
        });
    }

    public void promptToUpdate(@NonNull Activity activity, @NonNull JSONObject payload) {
        String changelog = payload.optString("changelog", context.getString(R.string.update_changelog_fallback));
        new AlertDialog.Builder(activity)
                .setTitle(R.string.update_dialog_title)
                .setMessage(changelog)
                .setCancelable(true)
                .setPositiveButton(R.string.update_button, (dialog, which) -> openStore(activity, payload))
                .setNegativeButton(R.string.update_later, null)
                .show();
    }

    public void openStore(@NonNull Context ctx, @Nullable JSONObject payload) {
        String storeUrl = resolveStoreUrl(payload);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(storeUrl));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            ctx.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            notifyError(context.getString(R.string.no_app_to_open));
        }
    }

    private void maybeNotifyUpdate(int remoteCode, @NonNull JSONObject payload) {
        int lastNotified = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getInt(PREF_LAST_NOTIFIED, 0);
        if (remoteCode <= lastNotified) {
            return;
        }
        showUpdateNotification(payload);
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .edit()
                .putInt(PREF_LAST_NOTIFIED, remoteCode)
                .apply();
    }

    private void showUpdateNotification(@NonNull JSONObject payload) {
        String title = context.getString(R.string.update_available);
        String body = context.getString(R.string.update_tap);
        String storeUrl = resolveStoreUrl(payload);
        Intent tapIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(storeUrl));
        tapIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        PendingIntent pendingIntent = PendingIntent.getActivity(
                context,
                0,
                tapIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        NotificationCompat.Builder builder = new NotificationCompat.Builder(context, CHANNEL_ID)
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .setContentTitle(title)
                .setContentText(body)
                .setAutoCancel(true)
                .setContentIntent(pendingIntent);

        NotificationManager nm = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) nm.notify(1001, builder.build());
    }

    private String resolveStoreUrl(@Nullable JSONObject payload) {
        String url = "";
        if (payload != null) {
            url = payload.optString("storeUrl", payload.optString("rustoreUrl", ""));
        }
        if (url == null || url.isEmpty()) {
            url = String.format(Locale.US, RU_STORE_URL_TEMPLATE, context.getPackageName());
        }
        return url;
    }

    private int getCurrentVersionCode() {
        try {
            PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                return (int) info.getLongVersionCode();
            }
            return info.versionCode;
        } catch (Exception e) {
            return 0;
        }
    }

    private void ensureChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationManager nm = context.getSystemService(NotificationManager.class);
            if (nm != null) {
                NotificationChannel ch = new NotificationChannel(CHANNEL_ID,
                        context.getString(R.string.notification_channel_updates),
                        NotificationManager.IMPORTANCE_LOW);
                nm.createNotificationChannel(ch);
            }
        }
    }

    private void notifyError(String msg) {
        if (listener != null) listener.onError(msg);
    }

    private static String safeMessage(Exception e) {
        String msg = e.getMessage();
        return msg != null ? msg : "";
    }
}
