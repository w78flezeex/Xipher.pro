package com.xipher.wrapper.ui;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.ChannelMemberDto;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityChannelAdminPermissionsBinding;
import com.xipher.wrapper.di.AppServices;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Активность для настройки прав администратора канала
 */
public class ChannelAdminPermissionsActivity extends AppCompatActivity {
    public static final String EXTRA_CHANNEL_ID = "channel_id";
    public static final String EXTRA_USER_ID = "user_id";
    public static final String EXTRA_USERNAME = "username";
    public static final String EXTRA_CURRENT_ROLE = "current_role";
    public static final String EXTRA_PERMISSIONS = "permissions";
    public static final String EXTRA_ADMIN_TITLE = "admin_title";

    private ActivityChannelAdminPermissionsBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String channelId;
    private String userId;
    private String username;
    private String currentRole;
    private int currentPerms;
    private String adminTitle;
    private boolean isExistingAdmin = false;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityChannelAdminPermissionsBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        // Получаем данные
        channelId = getIntent().getStringExtra(EXTRA_CHANNEL_ID);
        userId = getIntent().getStringExtra(EXTRA_USER_ID);
        username = getIntent().getStringExtra(EXTRA_USERNAME);
        currentRole = getIntent().getStringExtra(EXTRA_CURRENT_ROLE);
        currentPerms = getIntent().getIntExtra(EXTRA_PERMISSIONS, getDefaultPerms());
        adminTitle = getIntent().getStringExtra(EXTRA_ADMIN_TITLE);

        isExistingAdmin = "admin".equals(currentRole);

        setupUI();
        loadPermissions();
    }

    private int getDefaultPerms() {
        return ChannelMemberDto.PERM_CHANGE_INFO |
               ChannelMemberDto.PERM_POST_MESSAGES |
               ChannelMemberDto.PERM_INVITE;
    }

    private void setupUI() {
        binding.toolbar.setNavigationOnClickListener(v -> finish());

        // Заголовок
        binding.username.setText(username);
        if (!TextUtils.isEmpty(username)) {
            binding.avatarLetter.setText(String.valueOf(username.charAt(0)).toUpperCase());
        }

        if (isExistingAdmin) {
            binding.currentRole.setText(R.string.channel_admin_role);
            binding.btnRemoveAdmin.setVisibility(View.VISIBLE);
        } else {
            binding.currentRole.setText(R.string.channel_subscriber_to_admin);
            binding.btnRemoveAdmin.setVisibility(View.GONE);
        }

        // Кастомный заголовок администратора
        if (!TextUtils.isEmpty(adminTitle)) {
            binding.etAdminTitle.setText(adminTitle);
        }

        // Кнопка снять админа
        binding.btnRemoveAdmin.setOnClickListener(v -> showRemoveAdminDialog());

        // Кнопка сохранить
        binding.fabSave.setOnClickListener(v -> savePermissions());
    }

    private void loadPermissions() {
        // Устанавливаем текущие значения
        binding.switchChangeInfo.setChecked((currentPerms & ChannelMemberDto.PERM_CHANGE_INFO) != 0);
        binding.switchPostMessages.setChecked((currentPerms & ChannelMemberDto.PERM_POST_MESSAGES) != 0);
        binding.switchInvite.setChecked((currentPerms & ChannelMemberDto.PERM_INVITE) != 0);
        binding.switchRestrict.setChecked((currentPerms & ChannelMemberDto.PERM_RESTRICT) != 0);
        binding.switchPin.setChecked((currentPerms & ChannelMemberDto.PERM_PIN) != 0);
        binding.switchPromote.setChecked((currentPerms & ChannelMemberDto.PERM_PROMOTE) != 0);
    }

    private int collectPermissions() {
        int perms = 0;
        if (binding.switchChangeInfo.isChecked()) perms |= ChannelMemberDto.PERM_CHANGE_INFO;
        if (binding.switchPostMessages.isChecked()) perms |= ChannelMemberDto.PERM_POST_MESSAGES;
        if (binding.switchInvite.isChecked()) perms |= ChannelMemberDto.PERM_INVITE;
        if (binding.switchRestrict.isChecked()) perms |= ChannelMemberDto.PERM_RESTRICT;
        if (binding.switchPin.isChecked()) perms |= ChannelMemberDto.PERM_PIN;
        if (binding.switchPromote.isChecked()) perms |= ChannelMemberDto.PERM_PROMOTE;
        return perms;
    }

    private void savePermissions() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId) || TextUtils.isEmpty(userId)) {
            Toast.makeText(this, R.string.error_insufficient_data, Toast.LENGTH_SHORT).show();
            return;
        }

        int perms = collectPermissions();
        String title = binding.etAdminTitle.getText() != null ? 
                binding.etAdminTitle.getText().toString().trim() : "";

        if (perms == 0) {
            // Если все права сняты, предлагаем снять админа
            new MaterialAlertDialogBuilder(this)
                    .setTitle(R.string.channel_remove_admin_title)
                    .setMessage(R.string.channel_remove_admin_no_perms)
                    .setPositiveButton(R.string.channel_remove_admin, (dialog, which) -> removeAdmin())
                    .setNegativeButton(R.string.cancel, null)
                    .show();
            return;
        }

        binding.fabSave.setEnabled(false);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.setChannelAdminPermissions(token, channelId, userId, perms, title, false);

                runOnUiThread(() -> {
                    binding.fabSave.setEnabled(true);
                    if (success) {
                        Toast.makeText(this, R.string.channel_admin_rights_saved, Toast.LENGTH_SHORT).show();
                        setResult(RESULT_OK);
                        finish();
                    } else {
                        Toast.makeText(this, R.string.channel_admin_rights_save_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    binding.fabSave.setEnabled(true);
                    Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void showRemoveAdminDialog() {
        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.channel_remove_admin_title)
                .setMessage(getString(R.string.channel_remove_admin_message, username))
                .setPositiveButton(R.string.channel_remove_admin, (dialog, which) -> removeAdmin())
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void removeAdmin() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId) || TextUtils.isEmpty(userId)) {
            return;
        }

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.setChannelAdminPermissions(token, channelId, userId, 0, null, true);

                runOnUiThread(() -> {
                    if (success) {
                        Toast.makeText(this, R.string.channel_admin_removed, Toast.LENGTH_SHORT).show();
                        setResult(RESULT_OK);
                        finish();
                    } else {
                        Toast.makeText(this, R.string.channel_admin_remove_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() ->
                        Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
