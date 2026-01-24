package com.xipher.wrapper.ui;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.bumptech.glide.Glide;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.databinding.ActivitySettingsBinding;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.R;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class SettingsActivity extends AppCompatActivity {
    private ActivitySettingsBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivitySettingsBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        setupProfileCard();
        setupClickListeners();
        loadProfileFromApi();
        
        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_settings);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Refresh profile data when returning from ProfileActivity
        setupProfileCard();
    }

    private void loadProfileFromApi() {
        String token = authRepo.getToken();
        String userId = authRepo.getUserId();
        if (token == null || userId == null) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getUserProfile(token, userId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                    JsonObject user = res.getAsJsonObject("user");
                    String firstName = user.has("first_name") ? user.get("first_name").getAsString() : "";
                    String lastName = user.has("last_name") ? user.get("last_name").getAsString() : "";
                    String phone = user.has("phone") ? user.get("phone").getAsString() : null;
                    String avatarUrl = user.has("avatar_url") ? user.get("avatar_url").getAsString() : null;
                    
                    authRepo.saveProfile(firstName, lastName, phone, avatarUrl);
                    runOnUiThread(this::setupProfileCard);
                }
            } catch (Exception ignored) {}
        });
    }

    private void setupProfileCard() {
        // Check if activity is still valid
        if (isDestroyed() || isFinishing()) {
            return;
        }
        
        // Load user data
        String firstName = authRepo.getFirstName();
        String lastName = authRepo.getLastName();
        String phone = authRepo.getPhone();
        String avatarUrl = authRepo.getAvatarUrl();

        // Build full avatar URL
        String fullAvatarUrl = null;
        if (!TextUtils.isEmpty(avatarUrl)) {
            fullAvatarUrl = avatarUrl.startsWith("http") ? avatarUrl : BuildConfig.API_BASE + avatarUrl;
        }

        // Set name
        String fullName = "";
        if (!TextUtils.isEmpty(firstName)) {
            fullName = firstName;
            if (!TextUtils.isEmpty(lastName)) {
                fullName += " " + lastName;
            }
        }
        binding.settingsName.setText(fullName.isEmpty() ? getString(R.string.profile_name_placeholder) : fullName);

        // Set phone
        binding.settingsPhone.setText(!TextUtils.isEmpty(phone) ? phone : "");

        // Set avatar
        if (!TextUtils.isEmpty(fullAvatarUrl)) {
            binding.settingsAvatarImage.setVisibility(View.VISIBLE);
            binding.settingsAvatarLetter.setVisibility(View.GONE);
            Glide.with(getApplicationContext())
                .load(fullAvatarUrl)
                .circleCrop()
                .into(binding.settingsAvatarImage);
        } else {
            binding.settingsAvatarImage.setVisibility(View.GONE);
            binding.settingsAvatarLetter.setVisibility(View.VISIBLE);
            String letter = !TextUtils.isEmpty(firstName) ? firstName.substring(0, 1).toUpperCase() : "?";
            binding.settingsAvatarLetter.setText(letter);
        }

        // Edit profile button
        binding.btnEditProfile.setOnClickListener(v -> {
            Intent intent = new Intent(this, ProfileActivity.class);
            if (authRepo.getUserId() != null) {
                intent.putExtra(ProfileActivity.EXTRA_USER_ID, authRepo.getUserId());
            }
            startActivity(intent);
        });

        // Profile card click
        binding.profileCard.setOnClickListener(v -> {
            Intent intent = new Intent(this, ProfileActivity.class);
            if (authRepo.getUserId() != null) {
                intent.putExtra(ProfileActivity.EXTRA_USER_ID, authRepo.getUserId());
            }
            startActivity(intent);
        });
    }

    private void setupClickListeners() {
        binding.rowAccount.setOnClickListener(v -> {
            Intent intent = new Intent(this, ProfileActivity.class);
            if (authRepo.getUserId() != null) {
                intent.putExtra(ProfileActivity.EXTRA_USER_ID, authRepo.getUserId());
            }
            startActivity(intent);
        });

        binding.rowNotifications.setOnClickListener(v -> startActivity(new Intent(this, NotificationsSettingsActivity.class)));
        binding.rowSecurity.setOnClickListener(v -> startActivity(new Intent(this, PrivacySecurityActivity.class)));
        binding.rowLanguage.setOnClickListener(v -> startActivity(new Intent(this, LanguageActivity.class)));
        binding.rowChatTheme.setOnClickListener(v -> startActivity(new Intent(this, ChatThemeActivity.class)));
        binding.rowPremium.setOnClickListener(v -> startActivity(new Intent(this, PremiumActivity.class)));
        binding.rowFaq.setOnClickListener(v -> startActivity(new Intent(this, FaqActivity.class)));
    }
}
