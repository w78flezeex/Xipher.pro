package com.xipher.wrapper.ui;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.xipher.wrapper.databinding.ActivityNotificationsSettingsBinding;
import com.xipher.wrapper.notifications.NotificationHelper;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.R;

public class NotificationsSettingsActivity extends AppCompatActivity {
    private ActivityNotificationsSettingsBinding binding;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityNotificationsSettingsBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.btnBack.setOnClickListener(v -> finish());

        binding.switchPush.setChecked(PushTokenManager.isPushEnabled(this));
        binding.switchPush.setOnCheckedChangeListener((buttonView, isChecked) -> {
            PushTokenManager.setPushEnabled(this, isChecked);
            Toast.makeText(this, isChecked ? getString(R.string.push_enabled) : getString(R.string.push_disabled), Toast.LENGTH_SHORT).show();
        });

        binding.rowSystemNotifications.setOnClickListener(v -> openAppNotificationSettings());
        binding.rowMessageChannel.setOnClickListener(v -> openChannel(NotificationHelper.CHANNEL_MESSAGES));
        binding.rowCallChannel.setOnClickListener(v -> openChannel(NotificationHelper.CHANNEL_CALLS));
        binding.rowSoundSettings.setOnClickListener(v -> openSoundSettings());

        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_settings);
    }

    private void openAppNotificationSettings() {
        try {
            Intent intent = new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, getPackageName());
            startActivity(intent);
        } catch (Exception e) {
            Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            intent.setData(Uri.parse("package:" + getPackageName()));
            startActivity(intent);
        }
    }

    private void openChannel(String channelId) {
        try {
            Intent intent = new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, getPackageName());
            intent.putExtra(Settings.EXTRA_CHANNEL_ID, channelId);
            startActivity(intent);
        } catch (Exception e) {
            openAppNotificationSettings();
        }
    }

    private void openSoundSettings() {
        try {
            Intent intent = new Intent(Settings.ACTION_SOUND_SETTINGS);
            startActivity(intent);
        } catch (Exception e) {
            Toast.makeText(this, getString(R.string.feature_unavailable), Toast.LENGTH_SHORT).show();
        }
    }
}
