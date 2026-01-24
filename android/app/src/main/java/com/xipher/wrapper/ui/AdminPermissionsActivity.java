package com.xipher.wrapper.ui;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.GroupMemberDto;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityAdminPermissionsBinding;
import com.xipher.wrapper.di.AppServices;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class AdminPermissionsActivity extends AppCompatActivity {
    public static final String EXTRA_GROUP_ID = "group_id";
    public static final String EXTRA_USER_ID = "user_id";
    public static final String EXTRA_USERNAME = "username";
    public static final String EXTRA_CURRENT_ROLE = "current_role";
    public static final String EXTRA_PERMISSIONS = "permissions";

    private ActivityAdminPermissionsBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String groupId;
    private String userId;
    private String username;
    private String currentRole;
    private GroupMemberDto.AdminPermissions permissions;
    private boolean isExistingAdmin = false;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityAdminPermissionsBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        // Получаем данные
        groupId = getIntent().getStringExtra(EXTRA_GROUP_ID);
        userId = getIntent().getStringExtra(EXTRA_USER_ID);
        username = getIntent().getStringExtra(EXTRA_USERNAME);
        currentRole = getIntent().getStringExtra(EXTRA_CURRENT_ROLE);
        String permissionsJson = getIntent().getStringExtra(EXTRA_PERMISSIONS);

        isExistingAdmin = "admin".equals(currentRole);

        // Парсим права
        if (!TextUtils.isEmpty(permissionsJson)) {
            try {
                permissions = new Gson().fromJson(permissionsJson, GroupMemberDto.AdminPermissions.class);
            } catch (Exception e) {
                permissions = new GroupMemberDto.AdminPermissions();
            }
        } else {
            permissions = new GroupMemberDto.AdminPermissions();
        }

        setupUI();
        loadPermissions();
    }

    private void setupUI() {
        binding.toolbar.setNavigationOnClickListener(v -> finish());

        // Заголовок
        binding.username.setText(username);
        if (!TextUtils.isEmpty(username)) {
            binding.avatarLetter.setText(String.valueOf(username.charAt(0)).toUpperCase());
        }
        
        if (isExistingAdmin) {
            binding.currentRole.setText("Администратор");
            binding.btnRemoveAdmin.setVisibility(View.VISIBLE);
        } else {
            binding.currentRole.setText("Участник → Администратор");
            binding.btnRemoveAdmin.setVisibility(View.GONE);
        }

        // Кнопка снять админа
        binding.btnRemoveAdmin.setOnClickListener(v -> showRemoveAdminDialog());

        // Кнопка сохранить
        binding.fabSave.setOnClickListener(v -> savePermissions());
    }

    private void loadPermissions() {
        // Устанавливаем текущие значения
        binding.switchChangeInfo.setChecked(permissions.isCanChangeInfo());
        binding.switchDeleteMessages.setChecked(permissions.isCanDeleteMessages());
        binding.switchBanUsers.setChecked(permissions.isCanBanUsers());
        binding.switchInviteUsers.setChecked(permissions.isCanInviteUsers());
        binding.switchPinMessages.setChecked(permissions.isCanPinMessages());
        binding.switchManageVoice.setChecked(permissions.isCanManageVoice());
        binding.switchPromoteMembers.setChecked(permissions.isCanPromoteMembers());
        binding.switchAnonymous.setChecked(permissions.isAnonymous());
    }

    private void savePermissions() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId) || TextUtils.isEmpty(userId)) {
            Toast.makeText(this, "Ошибка: недостаточно данных", Toast.LENGTH_SHORT).show();
            return;
        }

        // Собираем права
        JsonObject perms = new JsonObject();
        perms.addProperty("can_change_info", binding.switchChangeInfo.isChecked());
        perms.addProperty("can_delete_messages", binding.switchDeleteMessages.isChecked());
        perms.addProperty("can_ban_users", binding.switchBanUsers.isChecked());
        perms.addProperty("can_invite_users", binding.switchInviteUsers.isChecked());
        perms.addProperty("can_pin_messages", binding.switchPinMessages.isChecked());
        perms.addProperty("can_manage_voice", binding.switchManageVoice.isChecked());
        perms.addProperty("can_promote_members", binding.switchPromoteMembers.isChecked());
        perms.addProperty("is_anonymous", binding.switchAnonymous.isChecked());

        binding.fabSave.setEnabled(false);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.setAdminPermissions(token, groupId, userId, perms.toString());

                runOnUiThread(() -> {
                    binding.fabSave.setEnabled(true);
                    if (success) {
                        Toast.makeText(this, "Права сохранены", Toast.LENGTH_SHORT).show();
                        setResult(RESULT_OK);
                        finish();
                    } else {
                        Toast.makeText(this, "Ошибка сохранения прав", Toast.LENGTH_SHORT).show();
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
                .setTitle("Снять с администратора")
                .setMessage("Снять " + username + " с должности администратора?")
                .setPositiveButton("Снять", (dialog, which) -> removeAdmin())
                .setNegativeButton("Отмена", null)
                .show();
    }

    private void removeAdmin() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId) || TextUtils.isEmpty(userId)) {
            return;
        }

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                // Снимаем админа - устанавливаем пустые права
                JsonObject emptyPerms = new JsonObject();
                emptyPerms.addProperty("remove_admin", true);
                boolean success = client.setAdminPermissions(token, groupId, userId, emptyPerms.toString());

                runOnUiThread(() -> {
                    if (success) {
                        Toast.makeText(this, "Администратор снят", Toast.LENGTH_SHORT).show();
                        setResult(RESULT_OK);
                        finish();
                    } else {
                        Toast.makeText(this, "Ошибка", Toast.LENGTH_SHORT).show();
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
