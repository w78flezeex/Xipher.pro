package com.xipher.wrapper.ui.fragment;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.FragmentGroupSettingsBinding;
import com.xipher.wrapper.di.AppServices;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Фрагмент настроек группы (только для админов)
 */
public class GroupSettingsFragment extends Fragment {
    private static final String ARG_GROUP_ID = "group_id";
    private static final String ARG_GROUP_TITLE = "group_title";
    private static final String ARG_MY_ROLE = "my_role";

    private FragmentGroupSettingsBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String groupId;
    private String groupTitle;
    private String myRole = "member";
    private String originalName = "";
    private String originalDescription = "";
    private boolean isForumMode = false;

    public static GroupSettingsFragment newInstance(String groupId, String groupTitle, String myRole) {
        GroupSettingsFragment fragment = new GroupSettingsFragment();
        Bundle args = new Bundle();
        args.putString(ARG_GROUP_ID, groupId);
        args.putString(ARG_GROUP_TITLE, groupTitle);
        args.putString(ARG_MY_ROLE, myRole);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            groupId = getArguments().getString(ARG_GROUP_ID);
            groupTitle = getArguments().getString(ARG_GROUP_TITLE);
            myRole = getArguments().getString(ARG_MY_ROLE, "member");
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentGroupSettingsBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        authRepo = AppServices.authRepository(requireContext());

        setupUI();
        loadGroupSettings();
    }

    private void setupUI() {
        boolean isOwner = "owner".equals(myRole) || "creator".equals(myRole);
        boolean isAdmin = "admin".equals(myRole) || isOwner;

        // Показываем/скрываем элементы в зависимости от роли
        binding.dangerZoneCard.setVisibility(isOwner ? View.VISIBLE : View.GONE);
        // Показываем форум-режим всегда (на сервере проверяется право)
        binding.forumModeCard.setVisibility(View.VISIBLE);
        binding.forumModeSwitch.setEnabled(isAdmin);
        binding.saveButton.setEnabled(isAdmin);

        binding.saveButton.setOnClickListener(v -> saveSettings());

        binding.deleteGroupButton.setOnClickListener(v -> {
            if (isOwner) {
                showDeleteConfirmation();
            }
        });

        binding.leaveGroupButton.setOnClickListener(v -> showLeaveConfirmation());

        binding.copyLinkButton.setOnClickListener(v -> copyInviteLink());
        binding.regenerateLinkButton.setOnClickListener(v -> regenerateInviteLink());
        
        // Forum mode switch
        binding.forumModeSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (buttonView.isPressed()) { // Only react to user clicks
                toggleForumMode(isChecked);
            }
        });
    }

    private void loadGroupSettings() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getGroupInfo(token, groupId);

                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    JsonObject group = res.has("group") ? res.getAsJsonObject("group") : null;

                    if (group != null) {
                        String name = group.has("name") ? group.get("name").getAsString() : "";
                        String description = group.has("description") ? group.get("description").getAsString() : "";
                        String inviteLink = group.has("invite_link") ? group.get("invite_link").getAsString() : "";
                        boolean forumMode = group.has("forum_mode") && group.get("forum_mode").getAsBoolean();

                        requireActivity().runOnUiThread(() -> {
                            originalName = name;
                            originalDescription = description;
                            isForumMode = forumMode;

                            binding.nameInput.setText(name);
                            binding.descriptionInput.setText(description);
                            
                            if (!TextUtils.isEmpty(inviteLink)) {
                                binding.inviteLinkText.setText(inviteLink);
                            }
                            
                            // Update forum mode UI
                            binding.forumModeSwitch.setChecked(forumMode);
                            binding.forumModeStatus.setText(forumMode ? R.string.enabled : R.string.disabled);
                        });
                    }
                }
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> 
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void saveSettings() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        String newName = binding.nameInput.getText() != null ? 
            binding.nameInput.getText().toString().trim() : "";
        String newDescription = binding.descriptionInput.getText() != null ? 
            binding.descriptionInput.getText().toString().trim() : "";

        if (TextUtils.isEmpty(newName)) {
            binding.nameInputLayout.setError(getString(R.string.group_name_required));
            return;
        }

        binding.saveButton.setEnabled(false);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = true;

                // Обновляем название
                if (!newName.equals(originalName)) {
                    JsonObject res = client.updateGroupName(token, groupId, newName);
                    success = res != null && res.has("success") && res.get("success").getAsBoolean();
                    if (success) originalName = newName;
                }

                // Обновляем описание
                if (success && !newDescription.equals(originalDescription)) {
                    JsonObject res = client.updateGroupDescription(token, groupId, newDescription);
                    success = res != null && res.has("success") && res.get("success").getAsBoolean();
                    if (success) originalDescription = newDescription;
                }

                boolean finalSuccess = success;
                requireActivity().runOnUiThread(() -> {
                    binding.saveButton.setEnabled(true);
                    if (finalSuccess) {
                        Toast.makeText(requireContext(), R.string.group_settings_saved, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(requireContext(), R.string.group_settings_save_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> {
                    binding.saveButton.setEnabled(true);
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void showDeleteConfirmation() {
        new MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.group_confirm_delete_title)
            .setMessage(R.string.group_confirm_delete_message)
            .setPositiveButton(R.string.delete, (dialog, which) -> deleteGroup())
            .setNegativeButton(R.string.cancel, null)
            .show();
    }

    private void deleteGroup() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.deleteGroup(token, groupId);

                boolean success = res != null && res.has("success") && res.get("success").getAsBoolean();

                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        Toast.makeText(requireContext(), R.string.group_deleted_success, Toast.LENGTH_SHORT).show();
                        requireActivity().finish();
                    } else {
                        Toast.makeText(requireContext(), R.string.group_delete_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> 
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void showLeaveConfirmation() {
        new MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.group_confirm_leave_title)
            .setMessage(getString(R.string.group_confirm_leave_message, groupTitle))
            .setPositiveButton(R.string.leave, (dialog, which) -> leaveGroup())
            .setNegativeButton(R.string.cancel, null)
            .show();
    }

    private void leaveGroup() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.leaveGroup(token, groupId);

                boolean success = res != null && res.has("success") && res.get("success").getAsBoolean();

                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        Toast.makeText(requireContext(), R.string.group_left, Toast.LENGTH_SHORT).show();
                        requireActivity().finish();
                    } else {
                        Toast.makeText(requireContext(), R.string.group_leave_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> 
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void copyInviteLink() {
        String link = binding.inviteLinkText.getText().toString();
        if (!TextUtils.isEmpty(link)) {
            android.content.ClipboardManager clipboard = 
                (android.content.ClipboardManager) requireContext().getSystemService(android.content.Context.CLIPBOARD_SERVICE);
            android.content.ClipData clip = android.content.ClipData.newPlainText("invite_link", link);
            clipboard.setPrimaryClip(clip);
            Toast.makeText(requireContext(), R.string.group_link_copied, Toast.LENGTH_SHORT).show();
        }
    }

    private void regenerateInviteLink() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.createGroupInvite(token, groupId);

                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    String newLink = res.has("invite_url") ? res.get("invite_url").getAsString() : "";
                    
                    requireActivity().runOnUiThread(() -> {
                        if (!TextUtils.isEmpty(newLink)) {
                            binding.inviteLinkText.setText(newLink);
                            Toast.makeText(requireContext(), R.string.group_link_regenerated, Toast.LENGTH_SHORT).show();
                        }
                    });
                } else {
                    requireActivity().runOnUiThread(() -> 
                        Toast.makeText(requireContext(), R.string.group_link_regenerate_failed, Toast.LENGTH_SHORT).show()
                    );
                }
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> 
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    /**
     * Toggle forum mode for the group
     */
    private void toggleForumMode(boolean enabled) {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        binding.forumModeSwitch.setEnabled(false);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.setGroupForumMode(token, groupId, enabled);

                boolean success = res != null && res.has("success") && res.get("success").getAsBoolean();

                requireActivity().runOnUiThread(() -> {
                    binding.forumModeSwitch.setEnabled(true);
                    
                    if (success) {
                        isForumMode = enabled;
                        binding.forumModeStatus.setText(enabled ? R.string.enabled : R.string.disabled);
                        Toast.makeText(requireContext(), 
                            enabled ? R.string.forum_mode_enabled : R.string.forum_mode_disabled, 
                            Toast.LENGTH_SHORT).show();
                    } else {
                        // Revert switch
                        binding.forumModeSwitch.setChecked(!enabled);
                        Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> {
                    binding.forumModeSwitch.setEnabled(true);
                    binding.forumModeSwitch.setChecked(!enabled); // Revert
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    public void updateData(String groupId, String groupTitle, String myRole) {
        this.groupId = groupId;
        this.groupTitle = groupTitle;
        this.myRole = myRole;

        if (binding != null) {
            setupUI();
            loadGroupSettings();
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
