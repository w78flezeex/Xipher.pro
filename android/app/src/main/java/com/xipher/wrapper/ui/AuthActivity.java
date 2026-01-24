package com.xipher.wrapper.ui;

import android.content.Intent;
import android.os.Bundle;
import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityAuthBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.notifications.NotificationHelper;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.data.db.DatabaseManager;
import com.xipher.wrapper.security.DbKeyStore;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.SecurityContext;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class AuthActivity extends AppCompatActivity {

    private ActivityAuthBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        try {
            authRepo = AppServices.authRepository(this);
            SecurityContext.get().setCurrentSessionToken(authRepo.getToken());
        } catch (Throwable t) {
            android.util.Log.e("AuthActivity", "Init failed", t);
        }
        
        requestNotificationPermission();

        if (authRepo != null && authRepo.getToken() != null && !authRepo.getToken().isEmpty()) {
            openAfterAuth();
            return;
        }

        binding = ActivityAuthBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        if (authRepo != null) {
            String savedUsername = authRepo.getUsername();
            if (savedUsername != null && !savedUsername.isEmpty()) {
                binding.etUsername.setText(savedUsername);
            }
        }

        binding.btnLogin.setOnClickListener(v -> doLogin());
        binding.tvRegister.setOnClickListener(v -> openRegister());
    }

    private void doLogin() {
        final String username = binding.etUsername.getText().toString().trim();
        final String password = binding.etPassword.getText().toString();

        if (username.isEmpty()) {
            binding.etUsername.setError(getString(R.string.login_required));
            return;
        }
        if (password.isEmpty()) {
            binding.etPassword.setError(getString(R.string.password_required));
            return;
        }

        setLoading(true);
        io.execute(() -> {
            try {
                var res = authRepo.login(username, password);
                runOnUiThread(() -> {
                    setLoading(false);
                    if (res != null && res.success) {
                        Toast.makeText(this, R.string.login_success, Toast.LENGTH_SHORT).show();
                        SecurityContext.get().setCurrentSessionToken(authRepo.getToken());
                        openAfterAuth();
                    } else {
                        Toast.makeText(this, res != null ? res.message : getString(R.string.login_failed), Toast.LENGTH_LONG).show();
                    }
                });
            } catch (Exception e) {
                android.util.Log.e("AuthActivity", "Login failed", e);
                runOnUiThread(() -> {
                    setLoading(false);
                    String errorMsg = e.getMessage();
                    if (errorMsg != null && errorMsg.length() > 100) {
                        errorMsg = errorMsg.substring(0, 100);
                    }
                    Toast.makeText(this, "Ошибка: " + errorMsg, Toast.LENGTH_LONG).show();
                });
            }
        });
    }

    private void setLoading(boolean loading) {
        if (binding == null) return;
        binding.progress.setVisibility(loading ? View.VISIBLE : View.GONE);
        binding.btnLogin.setEnabled(!loading);
        binding.etUsername.setEnabled(!loading);
        binding.etPassword.setEnabled(!loading);
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

    private void openAfterAuth() {
        // Don't call setLoading here - binding may be null
        io.execute(() -> {
            try {
                if (PanicModePrefs.isEnabled(this)) {
                    runOnUiThread(() -> {
                        Intent pinIntent = new Intent(this, PinUnlockActivity.class);
                        attachChatExtras(getIntent(), pinIntent);
                        startActivity(pinIntent);
                        finish();
                    });
                    return;
                }
                String key = DbKeyStore.getOrCreateMainKey(this);
                DatabaseManager.getInstance().init(getApplicationContext(), key);
                if (authRepo != null) {
                    SecurityContext.get().setCurrentSessionToken(authRepo.getToken());
                }
                runOnUiThread(this::openMain);
            } catch (DatabaseManager.InvalidPasswordException e) {
                runOnUiThread(() -> {
                    Intent pinIntent = new Intent(this, PinUnlockActivity.class);
                    attachChatExtras(getIntent(), pinIntent);
                    startActivity(pinIntent);
                    finish();
                });
            } catch (Throwable t) {
                android.util.Log.e("AuthActivity", "openAfterAuth failed", t);
                runOnUiThread(() -> {
                    // Show error and let user try again from login screen
                    String errorMsg = t.getMessage();
                    if (errorMsg != null && errorMsg.contains("Secure key storage")) {
                        Toast.makeText(this, "Ошибка безопасного хранилища. Попробуйте очистить данные приложения.", Toast.LENGTH_LONG).show();
                    } else {
                        Toast.makeText(this, "Ошибка запуска: " + errorMsg, Toast.LENGTH_LONG).show();
                    }
                    // Clear token and show login
                    if (authRepo != null) {
                        authRepo.clear();
                    }
                    binding = ActivityAuthBinding.inflate(getLayoutInflater());
                    setContentView(binding.getRoot());
                    binding.btnLogin.setOnClickListener(v -> doLogin());
                    binding.tvRegister.setOnClickListener(v -> openRegister());
                });
            }
        });
    }

    private void openRegister() {
        startActivity(new Intent(this, RegisterActivity.class));
    }

    private void requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.POST_NOTIFICATIONS}, 200);
            }
        }
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

}
