package com.xipher.wrapper.ui;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import com.xipher.wrapper.R;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.data.repo.ChatRepository;
import com.xipher.wrapper.databinding.ActivityLocationShareBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.util.LocationUtils;
import com.yandex.mapkit.MapKitFactory;
import com.yandex.mapkit.geometry.Point;
import com.yandex.mapkit.map.CameraPosition;
import com.yandex.mapkit.map.PlacemarkMapObject;

import java.util.Locale;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class LocationShareActivity extends AppCompatActivity {

    public static final String EXTRA_CHAT_ID = "extra_chat_id";
    public static final String EXTRA_IS_GROUP = "extra_is_group";
    public static final String EXTRA_IS_CHANNEL = "extra_is_channel";

    private ActivityLocationShareBinding binding;
    private LocationManager locationManager;
    private LocationListener locationListener;
    private ActivityResultLauncher<String[]> locationPermission;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private AuthRepository authRepo;

    private String chatId;
    private boolean isGroup;
    private boolean isChannel;
    private Double currentLat;
    private Double currentLon;
    private PlacemarkMapObject placemark;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityLocationShareBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        chatId = getIntent() != null ? getIntent().getStringExtra(EXTRA_CHAT_ID) : null;
        isGroup = getIntent() != null && getIntent().getBooleanExtra(EXTRA_IS_GROUP, false);
        isChannel = getIntent() != null && getIntent().getBooleanExtra(EXTRA_IS_CHANNEL, false);
        authRepo = AppServices.authRepository(this);
        if (chatId == null || chatId.isEmpty()) {
            finish();
            return;
        }
        if (BuildConfig.MAPKIT_API_KEY == null || BuildConfig.MAPKIT_API_KEY.isEmpty()) {
            Toast.makeText(this, R.string.location_mapkit_missing, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        binding.btnBack.setOnClickListener(v -> finish());
        binding.btnSendLocation.setOnClickListener(v -> sendCurrentLocation());
        binding.btnLiveLocation.setOnClickListener(v -> chooseLiveDuration());
        binding.btnSendLocation.setEnabled(false);
        binding.btnLiveLocation.setEnabled(false);

        locationPermission = registerForActivityResult(new ActivityResultContracts.RequestMultiplePermissions(), result -> {
            boolean granted = false;
            if (result != null) {
                Boolean fine = result.get(Manifest.permission.ACCESS_FINE_LOCATION);
                Boolean coarse = result.get(Manifest.permission.ACCESS_COARSE_LOCATION);
                granted = (fine != null && fine) || (coarse != null && coarse);
            }
            if (!granted) {
                Toast.makeText(this, R.string.location_permission_required, Toast.LENGTH_SHORT).show();
                binding.locationStatus.setText(R.string.location_permission_required);
                return;
            }
            loadLocation();
        });

        binding.locationStatus.setText(R.string.location_loading);
        loadLocation();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopLocationUpdates();
        io.shutdownNow();
    }

    @Override
    protected void onStart() {
        super.onStart();
        MapKitFactory.getInstance().onStart();
        binding.locationMap.onStart();
    }

    @Override
    protected void onStop() {
        binding.locationMap.onStop();
        MapKitFactory.getInstance().onStop();
        super.onStop();
    }

    private void loadLocation() {
        boolean hasPermission = ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
                || ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED;
        if (!hasPermission) {
            locationPermission.launch(new String[]{
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_COARSE_LOCATION
            });
            return;
        }
        try {
            locationManager = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
            if (locationManager == null) {
                binding.locationStatus.setText(R.string.location_unavailable);
                return;
            }
            String provider = locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)
                    ? LocationManager.GPS_PROVIDER
                    : LocationManager.NETWORK_PROVIDER;

            Location last = locationManager.getLastKnownLocation(provider);
            if (last != null) {
                applyLocation(last);
                return;
            }
            stopLocationUpdates();
            locationListener = new LocationListener() {
                @Override
                public void onLocationChanged(@NonNull Location location) {
                    applyLocation(location);
                    stopLocationUpdates();
                }
            };
            locationManager.requestSingleUpdate(provider, locationListener, Looper.getMainLooper());
            handler.postDelayed(() -> {
                if (currentLat == null || currentLon == null) {
                    binding.locationStatus.setText(R.string.location_unavailable);
                }
            }, 12000L);
        } catch (Exception e) {
            binding.locationStatus.setText(R.string.location_unavailable);
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

    private void applyLocation(Location location) {
        currentLat = location.getLatitude();
        currentLon = location.getLongitude();
        String coords = String.format(Locale.getDefault(), "%.5f, %.5f", currentLat, currentLon);
        binding.locationStatus.setText(getString(R.string.location_ready, coords));
        binding.btnSendLocation.setEnabled(true);
        binding.btnLiveLocation.setEnabled(true);
        Point point = new Point(currentLat, currentLon);
        if (placemark == null) {
            placemark = binding.locationMap.getMap().getMapObjects().addPlacemark(point);
        } else {
            placemark.setGeometry(point);
        }
        binding.locationMap.getMap().move(new CameraPosition(point, 16f, 0f, 0f));
    }

    private void sendCurrentLocation() {
        if (chatId == null || currentLat == null || currentLon == null) {
            Toast.makeText(this, R.string.location_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        binding.btnSendLocation.setEnabled(false);
        io.execute(() -> {
            try {
                String token = authRepo.getToken();
                String selfId = authRepo.getUserId();
                String selfName = authRepo.getUsername();
                if (token == null || token.isEmpty()) {
                    throw new IllegalStateException("Missing token");
                }
                ChatRepository repo = new ChatRepository(getApplicationContext(), token);
                String content = LocationUtils.buildMapUrl(currentLat, currentLon, null, 0L);
                if (isGroup) {
                    repo.sendGroupMessage(token, chatId, content, "location", null, selfId, selfName, null, null, null);
                } else if (isChannel) {
                    repo.sendChannelMessage(token, chatId, content, "location", null, selfId, selfName, null, null, null);
                } else {
                    repo.sendMessage(token, chatId, content, "location", null, selfId);
                }
                runOnUiThread(() -> {
                    Toast.makeText(this, R.string.location_sent, Toast.LENGTH_SHORT).show();
                    setResult(RESULT_OK);
                    finish();
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    binding.btnSendLocation.setEnabled(true);
                    Toast.makeText(this, R.string.location_send_failed, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void chooseLiveDuration() {
        if (chatId == null || currentLat == null || currentLon == null) {
            Toast.makeText(this, R.string.location_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        String[] options = new String[]{
                getString(R.string.location_live_15m),
                getString(R.string.location_live_1h),
                getString(R.string.location_live_8h)
        };
        long[] durations = new long[]{
                15L * 60L * 1000L,
                60L * 60L * 1000L,
                8L * 60L * 60L * 1000L
        };
        new AlertDialog.Builder(this)
                .setTitle(R.string.location_live_choose_duration)
                .setItems(options, (dialog, which) -> startLiveSharing(durations[which]))
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void startLiveSharing(long durationMs) {
        if (chatId == null || currentLat == null || currentLon == null) {
            Toast.makeText(this, R.string.location_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        String liveId = UUID.randomUUID().toString();
        long expiresAt = System.currentTimeMillis() + durationMs;
        binding.btnLiveLocation.setEnabled(false);
        io.execute(() -> {
            try {
                String token = authRepo.getToken();
                String selfId = authRepo.getUserId();
                String selfName = authRepo.getUsername();
                if (token == null || token.isEmpty()) {
                    throw new IllegalStateException("Missing token");
                }
                ChatRepository repo = new ChatRepository(getApplicationContext(), token);
                String content = LocationUtils.buildMapUrl(currentLat, currentLon, liveId, expiresAt);
                if (isGroup) {
                    repo.sendGroupMessage(token, chatId, content, "live_location", null, selfId, selfName, null, null, null);
                } else if (isChannel) {
                    repo.sendChannelMessage(token, chatId, content, "live_location", null, selfId, selfName, null, null, null);
                } else {
                    repo.sendMessage(token, chatId, content, "live_location", null, selfId);
                }
                runOnUiThread(() -> {
                    LiveLocationService.start(this, chatId, isGroup, isChannel, liveId, expiresAt);
                    Toast.makeText(this, R.string.location_live_started, Toast.LENGTH_SHORT).show();
                    setResult(RESULT_OK);
                    finish();
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    binding.btnLiveLocation.setEnabled(true);
                    Toast.makeText(this, R.string.location_send_failed, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }
}
