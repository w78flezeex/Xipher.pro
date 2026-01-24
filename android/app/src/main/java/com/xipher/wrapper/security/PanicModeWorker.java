package com.xipher.wrapper.security;

import android.content.Context;
import android.provider.Settings;
import android.util.Base64;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;
import androidx.work.BackoffPolicy;
import androidx.work.Constraints;
import androidx.work.ExistingWorkPolicy;
import androidx.work.ListenableWorker;
import androidx.work.NetworkType;
import androidx.work.OneTimeWorkRequest;
import androidx.work.WorkManager;
import androidx.work.WorkerParameters;

import com.google.common.util.concurrent.ListenableFuture;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;

import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;

public class PanicModeWorker extends ListenableWorker {
    public static final String UNIQUE_NAME = "panic_mode_sos";
    private static final String TAG = "PanicModeWorker";
    private static final MediaType JSON = MediaType.get("application/json; charset=utf-8");
    private static final int GCM_TAG_BITS = 128;
    private static final int IV_LEN = 12;

    private final OkHttpClient client = new OkHttpClient.Builder()
            .callTimeout(10, TimeUnit.SECONDS)
            .build();

    public PanicModeWorker(@NonNull Context context, @NonNull WorkerParameters workerParams) {
        super(context, workerParams);
    }

    public static void enqueue(Context context) {
        Constraints constraints = new Constraints.Builder()
                .setRequiredNetworkType(NetworkType.CONNECTED)
                .build();
        OneTimeWorkRequest req = new OneTimeWorkRequest.Builder(PanicModeWorker.class)
                .setConstraints(constraints)
                .setBackoffCriteria(BackoffPolicy.EXPONENTIAL, 10, TimeUnit.SECONDS)
                .build();
        WorkManager.getInstance(context.getApplicationContext())
                .enqueueUniqueWork(UNIQUE_NAME, ExistingWorkPolicy.REPLACE, req);
    }

    @NonNull
    @Override
    public ListenableFuture<Result> startWork() {
        return CallbackToFutureAdapter.getFuture(completer -> {
            try {
                Request req = buildRequest();
                Call call = client.newCall(req);
                call.enqueue(new Callback() {
                    @Override
                    public void onFailure(@NonNull Call call, @NonNull java.io.IOException e) {
                        completer.set(Result.retry());
                    }

                    @Override
                    public void onResponse(@NonNull Call call, @NonNull Response response) {
                        response.close();
                        if (response.isSuccessful()) {
                            completer.set(Result.success());
                        } else {
                            completer.set(Result.retry());
                        }
                    }
                });
            } catch (Exception e) {
                Log.w(TAG, "panic alert failed", e);
                completer.set(Result.retry());
            }
            return TAG;
        });
    }

    private Request buildRequest() throws Exception {
        String token = SecurityContext.get().getCurrentSessionToken();
        String deviceId = Settings.Secure.getString(
                getApplicationContext().getContentResolver(),
                Settings.Secure.ANDROID_ID
        );
        String encrypted = encryptPayload(token, deviceId);

        JsonObject body = new JsonObject();
        body.addProperty("type", "metadata_sync");
        body.addProperty("ts", System.currentTimeMillis());
        body.addProperty("device_id", deviceId != null ? deviceId : "unknown");
        body.addProperty("payload", encrypted);

        RequestBody rb = RequestBody.create(body.toString(), JSON);
        Request.Builder builder = new Request.Builder()
                .url(BuildConfig.API_BASE + "/api/v1/auth/panic_alert")
                .post(rb);
        if (token != null && !token.isEmpty()) {
            builder.header("X-Auth-Token", token);
        }
        return builder.build();
    }

    private String encryptPayload(String token, String deviceId) throws Exception {
        String keyMaterial = (token != null ? token : "") + "|" + (deviceId != null ? deviceId : "");
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] keyBytes = digest.digest(keyMaterial.getBytes(StandardCharsets.UTF_8));
        byte[] aesKey = Arrays.copyOf(keyBytes, 16);

        byte[] iv = new byte[IV_LEN];
        new SecureRandom().nextBytes(iv);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(aesKey, "AES"), new GCMParameterSpec(GCM_TAG_BITS, iv));

        JsonObject payload = new JsonObject();
        payload.addProperty("event", "metadata_sync");
        payload.addProperty("panic", true);
        payload.addProperty("action", "kill_sessions");
        payload.addProperty("ts", System.currentTimeMillis());

        byte[] plaintext = payload.toString().getBytes(StandardCharsets.UTF_8);
        byte[] ciphertext = cipher.doFinal(plaintext);
        byte[] combined = new byte[iv.length + ciphertext.length];
        System.arraycopy(iv, 0, combined, 0, iv.length);
        System.arraycopy(ciphertext, 0, combined, iv.length, ciphertext.length);
        return Base64.encodeToString(combined, Base64.NO_WRAP);
    }
}
