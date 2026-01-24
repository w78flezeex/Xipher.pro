package com.xipher.wrapper.ui;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.data.repo.ChatRepository;
import com.xipher.wrapper.databinding.ActivityMapBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.util.LocationUtils;
import com.yandex.mapkit.MapKitFactory;
import com.yandex.mapkit.geometry.Point;
import com.yandex.mapkit.map.CameraPosition;
import com.yandex.mapkit.map.PlacemarkMapObject;

import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MapActivity extends AppCompatActivity {

    public static final String EXTRA_CHAT_ID = "extra_chat_id";
    public static final String EXTRA_IS_GROUP = "extra_is_group";
    public static final String EXTRA_IS_CHANNEL = "extra_is_channel";
    public static final String EXTRA_LAT = "extra_lat";
    public static final String EXTRA_LON = "extra_lon";
    public static final String EXTRA_LIVE_ID = "extra_live_id";
    public static final String EXTRA_EXPIRES_AT = "extra_expires_at";

    private ActivityMapBinding binding;
    private PlacemarkMapObject placemark;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private AuthRepository authRepo;

    private String chatId;
    private boolean isGroup;
    private boolean isChannel;
    private String liveId;
    private long expiresAt;
    private double lat;
    private double lon;

    private final Runnable pollRunnable = new Runnable() {
        @Override
        public void run() {
            if (liveId == null || liveId.isEmpty()) return;
            pollLatestLiveLocation();
            handler.postDelayed(this, 8000L);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (BuildConfig.MAPKIT_API_KEY == null || BuildConfig.MAPKIT_API_KEY.isEmpty()) {
            Toast.makeText(this, R.string.location_mapkit_missing, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        binding = ActivityMapBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);
        chatId = getIntent() != null ? getIntent().getStringExtra(EXTRA_CHAT_ID) : null;
        isGroup = getIntent() != null && getIntent().getBooleanExtra(EXTRA_IS_GROUP, false);
        isChannel = getIntent() != null && getIntent().getBooleanExtra(EXTRA_IS_CHANNEL, false);
        liveId = getIntent() != null ? getIntent().getStringExtra(EXTRA_LIVE_ID) : null;
        expiresAt = getIntent() != null ? getIntent().getLongExtra(EXTRA_EXPIRES_AT, 0L) : 0L;
        lat = getIntent() != null ? getIntent().getDoubleExtra(EXTRA_LAT, 0d) : 0d;
        lon = getIntent() != null ? getIntent().getDoubleExtra(EXTRA_LON, 0d) : 0d;

        binding.btnBack.setOnClickListener(v -> finish());
        binding.mapTitle.setText(liveId != null && !liveId.isEmpty()
                ? getString(R.string.location_live_label)
                : getString(R.string.location_label));

        updateMarker(lat, lon, true);
        if (liveId != null && !liveId.isEmpty()) {
            handler.postDelayed(pollRunnable, 3000L);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        MapKitFactory.getInstance().onStart();
        binding.mapView.onStart();
    }

    @Override
    protected void onStop() {
        binding.mapView.onStop();
        MapKitFactory.getInstance().onStop();
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacksAndMessages(null);
        io.shutdownNow();
        super.onDestroy();
    }

    private void updateMarker(double latitude, double longitude, boolean moveCamera) {
        Point point = new Point(latitude, longitude);
        if (placemark == null) {
            placemark = binding.mapView.getMap().getMapObjects().addPlacemark(point);
        } else {
            placemark.setGeometry(point);
        }
        if (moveCamera) {
            binding.mapView.getMap().move(new CameraPosition(point, 16f, 0f, 0f));
        }
    }

    private void pollLatestLiveLocation() {
        if (chatId == null) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        io.execute(() -> {
            try {
                ChatRepository repo = new ChatRepository(getApplicationContext(), token);
                List<MessageEntity> list;
                if (isGroup) {
                    list = repo.syncGroupMessages(token, chatId);
                } else if (isChannel) {
                    list = repo.syncChannelMessages(token, chatId);
                } else {
                    list = repo.syncMessages(token, chatId);
                }
                MessageEntity latest = null;
                for (MessageEntity m : list) {
                    if (m == null || m.content == null || m.content.isEmpty()) continue;
                    if (!"live_location".equals(m.messageType)) continue;
                    LocationUtils.LocationPayload payload = LocationUtils.parse(m.content);
                    if (payload == null || payload.liveId == null) continue;
                    if (!payload.liveId.equals(liveId)) continue;
                    if (latest == null || m.createdAtMillis >= latest.createdAtMillis) {
                        latest = m;
                    }
                }
                if (latest == null) return;
                LocationUtils.LocationPayload payload = LocationUtils.parse(latest.content);
                if (payload == null) return;
                lat = payload.lat;
                lon = payload.lon;
                runOnUiThread(() -> updateMarker(lat, lon, false));
                if (expiresAt > 0L && System.currentTimeMillis() > expiresAt) {
                    runOnUiThread(() -> binding.mapTitle.setText(getString(R.string.location_label)));
                    liveId = null;
                }
            } catch (Exception ignored) {
            }
        });
    }
}
