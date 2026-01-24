package com.xipher.wrapper.ui.fragment;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupMenu;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.FragmentGroupOverviewBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.CallActivity;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class GroupOverviewFragment extends Fragment {
    private static final String ARG_GROUP_ID = "group_id";
    private static final String ARG_GROUP_TITLE = "group_title";
    private static final String ARG_MY_ROLE = "my_role";

    private FragmentGroupOverviewBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String groupId;
    private String groupTitle;
    private String myRole = "member";
    private int memberCount = 0;
    private boolean isMuted = false;

    public static GroupOverviewFragment newInstance(String groupId, String groupTitle, String myRole) {
        GroupOverviewFragment fragment = new GroupOverviewFragment();
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
        binding = FragmentGroupOverviewBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        authRepo = AppServices.authRepository(requireContext());

        setupUI();
        updateAdminVisibility();
    }

    private void setupUI() {
        if (!TextUtils.isEmpty(groupTitle)) {
            binding.groupName.setText(groupTitle);
        }

        // Уведомления - toggle mute
        binding.actionNotifications.setOnClickListener(v -> toggleMute());

        // Поиск - открыть поиск в чате
        binding.actionSearch.setOnClickListener(v -> openSearch());

        // Звонок - групповой звонок
        binding.actionCall.setOnClickListener(v -> startGroupCall());

        // Ещё - popup меню
        binding.actionMore.setOnClickListener(this::showMoreMenu);

        // Покинуть группу
        binding.actionLeave.setOnClickListener(v -> showLeaveGroupDialog());

        // Удалить группу (только для владельца)
        binding.actionDelete.setOnClickListener(v -> showDeleteGroupDialog());

        // Медиа секция
        binding.sectionMedia.setOnClickListener(v -> openMediaGallery());

        // Админ настройки
        binding.settingEdit.setOnClickListener(v -> openEditGroup());
        binding.settingAdmins.setOnClickListener(v -> openAdminsList());
        binding.settingInviteLink.setOnClickListener(v -> shareGroupLink());
    }

    private void toggleMute() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.muteChat(token, groupId, !isMuted);
                boolean success = res != null && res.has("success") && res.get("success").getAsBoolean();
                
                if (success) {
                    isMuted = !isMuted;
                    requireActivity().runOnUiThread(() -> {
                        Toast.makeText(requireContext(), 
                            isMuted ? R.string.chat_muted : R.string.chat_unmuted, 
                            Toast.LENGTH_SHORT).show();
                    });
                }
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void openSearch() {
        // Вернуться в чат и открыть поиск
        Intent intent = new Intent();
        intent.putExtra("action", "search");
        requireActivity().setResult(android.app.Activity.RESULT_OK, intent);
        requireActivity().finish();
    }

    private void startGroupCall() {
        if (TextUtils.isEmpty(groupId)) return;
        
        // Групповые звонки пока не поддерживаются
        Toast.makeText(requireContext(), R.string.group_calls_not_supported, Toast.LENGTH_SHORT).show();
    }

    private void showMoreMenu(View anchor) {
        PopupMenu popup = new PopupMenu(requireContext(), anchor);
        popup.getMenu().add(0, 1, 0, R.string.group_share_link);
        popup.getMenu().add(0, 2, 1, R.string.group_report);
        
        popup.setOnMenuItemClickListener(item -> {
            switch (item.getItemId()) {
                case 1:
                    shareGroupLink();
                    return true;
                case 2:
                    reportGroup();
                    return true;
            }
            return false;
        });
        popup.show();
    }

    private void shareGroupLink() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getGroupLink(token, groupId);
                
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    String link = res.has("link") ? res.get("link").getAsString() : null;
                    if (!TextUtils.isEmpty(link)) {
                        requireActivity().runOnUiThread(() -> {
                            Intent shareIntent = new Intent(Intent.ACTION_SEND);
                            shareIntent.setType("text/plain");
                            shareIntent.putExtra(Intent.EXTRA_TEXT, link);
                            startActivity(Intent.createChooser(shareIntent, getString(R.string.group_share_link)));
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

    private void reportGroup() {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.group_report)
                .setMessage(R.string.group_report_confirm)
                .setPositiveButton(R.string.report, (dialog, which) -> {
                    Toast.makeText(requireContext(), R.string.report_sent, Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void openMediaGallery() {
        // Открыть галерею медиа группы
        Toast.makeText(requireContext(), R.string.feature_coming_soon, Toast.LENGTH_SHORT).show();
    }

    private void openEditGroup() {
        // Открыть редактирование группы
        if (getActivity() instanceof com.xipher.wrapper.ui.GroupInfoActivity) {
            // Переключиться на вкладку настроек
            ((com.xipher.wrapper.ui.GroupInfoActivity) getActivity()).switchToSettingsTab();
        }
    }

    private void openAdminsList() {
        // Открыть список админов - переключиться на вкладку участников и фильтровать
        if (getActivity() instanceof com.xipher.wrapper.ui.GroupInfoActivity) {
            ((com.xipher.wrapper.ui.GroupInfoActivity) getActivity()).switchToMembersTab();
        }
    }

    private void updateAdminVisibility() {
        boolean isOwner = "owner".equals(myRole);
        boolean isAdmin = "admin".equals(myRole) || isOwner;

        // Секция админ-настроек
        binding.sectionSettings.setVisibility(isAdmin ? View.VISIBLE : View.GONE);
        
        // Кнопка удаления группы
        binding.actionDelete.setVisibility(isOwner ? View.VISIBLE : View.GONE);
    }

    public void updateData(String groupId, String groupTitle, String myRole, int memberCount) {
        this.groupId = groupId;
        this.groupTitle = groupTitle;
        this.myRole = myRole;
        this.memberCount = memberCount;

        if (binding != null) {
            binding.groupName.setText(groupTitle);
            updateAdminVisibility();
        }
    }

    private void showLeaveGroupDialog() {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle("Покинуть группу")
                .setMessage("Вы уверены, что хотите покинуть группу \"" + groupTitle + "\"?")
                .setPositiveButton("Покинуть", (dialog, which) -> leaveGroup())
                .setNegativeButton("Отмена", null)
                .show();
    }

    private void showDeleteGroupDialog() {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle("Удалить группу")
                .setMessage("Вы уверены, что хотите удалить группу \"" + groupTitle + "\"? Это действие нельзя отменить.")
                .setPositiveButton("Удалить", (dialog, which) -> deleteGroup())
                .setNegativeButton("Отмена", null)
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
                        Toast.makeText(requireContext(), "Группа удалена", Toast.LENGTH_SHORT).show();
                        requireActivity().finish();
                    } else {
                        Toast.makeText(requireContext(), "Ошибка удаления группы", Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                        Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
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
