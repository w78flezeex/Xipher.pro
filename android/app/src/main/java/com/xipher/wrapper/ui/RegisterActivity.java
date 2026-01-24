package com.xipher.wrapper.ui;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.model.BasicResponse;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityRegisterBinding;
import com.xipher.wrapper.di.AppServices;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class RegisterActivity extends AppCompatActivity {

    private static final int USERNAME_MIN = 3;
    private static final int USERNAME_MAX = 50;
    private static final int PASSWORD_MIN = 6;
    private static final long USERNAME_CHECK_DELAY_MS = 500;

    private ActivityRegisterBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private Runnable pendingUsernameCheck;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityRegisterBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        binding.btnRegister.setOnClickListener(v -> doRegister());
        binding.tvLoginLink.setOnClickListener(v -> finish());

        binding.etUsername.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                scheduleUsernameCheck(s != null ? s.toString() : "");
            }
        });
    }

    private void scheduleUsernameCheck(String raw) {
        String username = raw.trim();
        if (pendingUsernameCheck != null) {
            handler.removeCallbacks(pendingUsernameCheck);
        }

        binding.tvUsernameStatus.setVisibility(View.GONE);

        if (!isUsernameFormatOk(username)) {
            if (!username.isEmpty()) {
                binding.etUsername.setError(getString(R.string.username_format_error));
            }
            return;
        }

        binding.etUsername.setError(null);
        pendingUsernameCheck = () -> io.execute(() -> {
            try {
                BasicResponse res = authRepo.checkUsername(username);
                runOnUiThread(() -> {
                    String current = binding.etUsername.getText().toString().trim();
                    if (!current.equals(username)) return;
                    updateUsernameStatus(res);
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    String current = binding.etUsername.getText().toString().trim();
                    if (!current.equals(username)) return;
                    binding.tvUsernameStatus.setVisibility(View.VISIBLE);
                    binding.tvUsernameStatus.setText(getString(R.string.username_check_failed));
                });
            }
        });
        handler.postDelayed(pendingUsernameCheck, USERNAME_CHECK_DELAY_MS);
    }

    private void updateUsernameStatus(BasicResponse res) {
        if (res == null) return;
        binding.tvUsernameStatus.setVisibility(View.VISIBLE);
        if (res.success) {
            binding.tvUsernameStatus.setTextColor(getColor(com.xipher.wrapper.R.color.x_accent_end));
            binding.tvUsernameStatus.setText(getString(R.string.username_available));
        } else {
            binding.tvUsernameStatus.setTextColor(getColor(com.xipher.wrapper.R.color.x_accent_pink));
            binding.tvUsernameStatus.setText(res.message != null ? res.message : getString(R.string.username_taken));
        }
    }

    private void doRegister() {
        String username = binding.etUsername.getText().toString().trim();
        String password = binding.etPassword.getText().toString();
        String confirm = binding.etConfirmPassword.getText().toString();

        if (!isUsernameFormatOk(username)) {
            binding.etUsername.setError(getString(R.string.username_format_error));
            return;
        }
        if (password.length() < PASSWORD_MIN) {
            binding.etPassword.setError(getString(R.string.password_min_error));
            return;
        }
        if (!password.equals(confirm)) {
            binding.etConfirmPassword.setError(getString(R.string.password_mismatch));
            return;
        }

        setLoading(true);
        io.execute(() -> {
            try {
                BasicResponse res = authRepo.register(username, password);
                runOnUiThread(() -> {
                    setLoading(false);
                    if (res != null && res.success) {
                        Toast.makeText(this, R.string.register_success, Toast.LENGTH_SHORT).show();
                        finish();
                    } else {
                        binding.etUsername.setError(res != null ? res.message : getString(R.string.register_error));
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    setLoading(false);
                    Toast.makeText(this, R.string.network_unavailable, Toast.LENGTH_LONG).show();
                });
            }
        });
    }

    private void setLoading(boolean loading) {
        binding.progress.setVisibility(loading ? View.VISIBLE : View.GONE);
        binding.btnRegister.setEnabled(!loading);
        binding.etUsername.setEnabled(!loading);
        binding.etPassword.setEnabled(!loading);
        binding.etConfirmPassword.setEnabled(!loading);
    }

    private boolean isUsernameFormatOk(String username) {
        if (username.length() < USERNAME_MIN || username.length() > USERNAME_MAX) {
            return false;
        }
        return username.matches("^[a-zA-Z0-9_]+$");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (pendingUsernameCheck != null) {
            handler.removeCallbacks(pendingUsernameCheck);
        }
    }
}
