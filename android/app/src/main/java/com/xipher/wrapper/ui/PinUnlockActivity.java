package com.xipher.wrapper.ui;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.DatabaseManager;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityPinUnlockBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.notifications.NotificationHelper;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.security.DbKeyStore;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.PinPrefs;
import com.xipher.wrapper.security.SecurityContext;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class PinUnlockActivity extends AppCompatActivity {

    private ActivityPinUnlockBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityPinUnlockBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            Intent authIntent = new Intent(this, AuthActivity.class);
            attachChatExtras(getIntent(), authIntent);
            startActivity(authIntent);
            finish();
            return;
        }
        SecurityContext.get().setCurrentSessionToken(token);

        if (PanicModePrefs.isEnabled(this)) {
            binding.tvUnlockSubtitle.setText(R.string.pin_unlock_subtitle_panic);
        }

        binding.btnUnlock.setOnClickListener(v -> unlock());
        binding.tvSwitchAccount.setOnClickListener(v -> switchAccount());
    }

    private void unlock() {
        String pin = binding.etPin.getText().toString().trim();
        if (!isValidPin(pin)) {
            binding.etPin.setError(getString(R.string.pin_invalid));
            return;
        }

        setLoading(true);
        io.execute(() -> {
            try {
                DatabaseManager.getInstance().init(getApplicationContext(), pin);
                boolean panic = DatabaseManager.getInstance().isPanicMode();
                if (!panic) {
                    PinPrefs.setMainPinSet(this, true);
                    DbKeyStore.clearMainKey(this);
                }
                runOnUiThread(() -> {
                    setLoading(false);
                    openMain();
                });
            } catch (DatabaseManager.InvalidPasswordException e) {
                runOnUiThread(() -> {
                    setLoading(false);
                    binding.etPin.setError(getString(R.string.pin_wrong));
                    Toast.makeText(this, R.string.pin_wrong, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void openMain() {
        if (!SecurityContext.get().isPanicMode()) {
            PushTokenManager.syncIfPossible(this);
        }
        Intent intent = new Intent(this, ChatActivity.class);
        attachChatExtras(getIntent(), intent);
        startActivity(intent);
        finish();
    }

    private void switchAccount() {
        authRepo.clear();
        SecurityContext.get().clearSession();
        DatabaseManager.getInstance().clearSensitiveState();
        startActivity(new Intent(this, AuthActivity.class));
        finish();
    }

    private void setLoading(boolean loading) {
        binding.progress.setVisibility(loading ? View.VISIBLE : View.GONE);
        binding.btnUnlock.setEnabled(!loading);
        binding.etPin.setEnabled(!loading);
    }

    private boolean isValidPin(String pin) {
        if (pin == null) return false;
        int len = pin.length();
        if (len < 4 || len > 6) return false;
        for (int i = 0; i < len; i++) {
            char c = pin.charAt(i);
            if (c < '0' || c > '9') return false;
        }
        return true;
    }

    private void attachChatExtras(Intent source, Intent target) {
        if (source == null || target == null) return;
        String chatId = firstNonEmpty(
                source.getStringExtra(NotificationHelper.EXTRA_CHAT_ID),
                source.getStringExtra("chat_id")
        );
        if (chatId == null || chatId.isEmpty()) return;
        String chatTitle = firstNonEmpty(
                source.getStringExtra(NotificationHelper.EXTRA_CHAT_TITLE),
                source.getStringExtra("chat_title")
        );
        String chatType = firstNonEmpty(
                source.getStringExtra(NotificationHelper.EXTRA_CHAT_TYPE),
                source.getStringExtra("chat_type")
        );
        String userName = firstNonEmpty(
                source.getStringExtra(NotificationHelper.EXTRA_USER_NAME),
                source.getStringExtra("user_name"),
                source.getStringExtra("sender_name")
        );
        target.putExtra(NotificationHelper.EXTRA_CHAT_ID, chatId);
        if (chatTitle != null && !chatTitle.isEmpty()) {
            target.putExtra(NotificationHelper.EXTRA_CHAT_TITLE, chatTitle);
        }
        if (chatType != null && !chatType.isEmpty()) {
            target.putExtra(NotificationHelper.EXTRA_CHAT_TYPE, chatType);
        }
        if (userName != null && !userName.isEmpty()) {
            target.putExtra(NotificationHelper.EXTRA_USER_NAME, userName);
        }
    }

    private String firstNonEmpty(String... values) {
        if (values == null) return null;
        for (String v : values) {
            if (v != null && !v.isEmpty()) return v;
        }
        return null;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
