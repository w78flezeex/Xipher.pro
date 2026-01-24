package com.xipher.wrapper.ui;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityChannelInfoBinding;
import com.xipher.wrapper.di.AppServices;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ChannelInfoActivity extends AppCompatActivity {
    public static final String EXTRA_CHANNEL_ID = "channel_id";
    public static final String EXTRA_CHANNEL_TITLE = "channel_title";

    private ActivityChannelInfoBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private String channelId;
    private boolean isAdmin = false;
    private String originalName = "";
    private String originalDescription = "";
    private String originalUsername = "";
    private boolean originalPrivate = false;
    private boolean originalShowAuthor = true;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityChannelInfoBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);
        channelId = getIntent() != null ? getIntent().getStringExtra(EXTRA_CHANNEL_ID) : null;
        String title = getIntent() != null ? getIntent().getStringExtra(EXTRA_CHANNEL_TITLE) : null;
        if (!TextUtils.isEmpty(title)) {
            binding.channelTitle.setText(title);
        }

        binding.btnBack.setOnClickListener(v -> finish());
        binding.btnSave.setOnClickListener(v -> saveChanges());
        binding.btnManageMembers.setOnClickListener(v -> openManageMembers());

        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_chats);
        loadChannelInfo();
    }

    private void openManageMembers() {
        Intent intent = new Intent(this, ChannelManagementActivity.class);
        intent.putExtra(ChannelManagementActivity.EXTRA_CHANNEL_ID, channelId);
        intent.putExtra(ChannelManagementActivity.EXTRA_CHANNEL_TITLE, binding.etName.getText() != null ? binding.etName.getText().toString() : "");
        startActivity(intent);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }

    private void loadChannelInfo() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;
        io.execute(() -> {
            try {
                com.xipher.wrapper.data.api.ApiClient client = new com.xipher.wrapper.data.api.ApiClient(token);
                JsonObject res = client.getChannelInfo(token, channelId);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                if (!ok) {
                    runOnUiThread(() -> Toast.makeText(this, R.string.channel_info_load_failed, Toast.LENGTH_SHORT).show());
                    return;
                }
                JsonObject channel = res.has("channel") ? res.getAsJsonObject("channel") : null;
                boolean admin = res.has("is_admin") && res.get("is_admin").getAsBoolean();
                if (channel != null) {
                    String name = channel.has("name") ? channel.get("name").getAsString() : "";
                    String description = channel.has("description") ? channel.get("description").getAsString() : "";
                    String username = channel.has("custom_link") ? channel.get("custom_link").getAsString() : "";
                    boolean isPrivate = channel.has("is_private") && channel.get("is_private").getAsBoolean();
                    boolean showAuthor = channel.has("show_author") && channel.get("show_author").getAsBoolean();
                    runOnUiThread(() -> {
                        originalName = name;
                        originalDescription = description;
                        originalUsername = username;
                        originalPrivate = isPrivate;
                        originalShowAuthor = showAuthor;

                        binding.etName.setText(name);
                        binding.etDescription.setText(description);
                        binding.etUsername.setText(username);
                        binding.switchPrivate.setChecked(isPrivate);
                        binding.switchShowAuthor.setChecked(showAuthor);
                        applyAdminState(admin);
                    });
                } else {
                    runOnUiThread(() -> Toast.makeText(this, R.string.channel_info_load_failed, Toast.LENGTH_SHORT).show());
                }
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void applyAdminState(boolean admin) {
        isAdmin = admin;
        binding.channelInfoNote.setVisibility(admin ? android.view.View.GONE : android.view.View.VISIBLE);
        binding.etName.setEnabled(admin);
        binding.etDescription.setEnabled(admin);
        binding.etUsername.setEnabled(admin);
        binding.switchPrivate.setEnabled(admin);
        binding.switchShowAuthor.setEnabled(admin);
        binding.btnSave.setVisibility(admin ? android.view.View.VISIBLE : android.view.View.GONE);
        binding.btnManageMembers.setVisibility(admin ? android.view.View.VISIBLE : android.view.View.GONE);
    }

    private void saveChanges() {
        if (!isAdmin) return;
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;
        String newName = binding.etName.getText() != null ? binding.etName.getText().toString().trim() : "";
        String newDescription = binding.etDescription.getText() != null ? binding.etDescription.getText().toString().trim() : "";
        String newUsername = binding.etUsername.getText() != null ? binding.etUsername.getText().toString().trim() : "";
        boolean newPrivate = binding.switchPrivate.isChecked();
        boolean newShowAuthor = binding.switchShowAuthor.isChecked();

        binding.btnSave.setEnabled(false);
        io.execute(() -> {
            try {
                com.xipher.wrapper.data.api.ApiClient client = new com.xipher.wrapper.data.api.ApiClient(token);
                if (!TextUtils.isEmpty(newName) && !newName.equals(originalName)) {
                    client.updateChannelName(token, channelId, newName);
                    originalName = newName;
                }
                if (!newDescription.equals(originalDescription)) {
                    client.updateChannelDescription(token, channelId, newDescription);
                    originalDescription = newDescription;
                }
                if (!newUsername.equals(originalUsername)) {
                    client.setChannelCustomLink(token, channelId, newUsername);
                    originalUsername = newUsername;
                }
                if (newPrivate != originalPrivate) {
                    client.setChannelPrivacy(token, channelId, newPrivate);
                    originalPrivate = newPrivate;
                }
                if (newShowAuthor != originalShowAuthor) {
                    client.setChannelShowAuthor(token, channelId, newShowAuthor);
                    originalShowAuthor = newShowAuthor;
                }
                runOnUiThread(() -> {
                    binding.btnSave.setEnabled(true);
                    Toast.makeText(this, R.string.channel_info_saved, Toast.LENGTH_SHORT).show();
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    binding.btnSave.setEnabled(true);
                    Toast.makeText(this, R.string.channel_info_save_failed, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }
}
