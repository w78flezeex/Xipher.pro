package com.xipher.wrapper.ui.fragment;

import android.content.Intent;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.GroupMemberDto;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.BottomSheetMemberActionsBinding;
import com.xipher.wrapper.databinding.FragmentGroupMembersBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.AdminPermissionsActivity;
import com.xipher.wrapper.ui.ChatDetailActivity;
import com.xipher.wrapper.ui.ProfileActivity;
import com.xipher.wrapper.ui.adapter.GroupMemberAdapter;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class GroupMembersFragment extends Fragment implements GroupMemberAdapter.OnMemberClickListener {
    private static final String ARG_GROUP_ID = "group_id";
    private static final String ARG_MY_ROLE = "my_role";
    private static final String ARG_MY_USER_ID = "my_user_id";

    private FragmentGroupMembersBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String groupId;
    private String myRole = "member";
    private String myUserId = "";

    private GroupMemberAdapter adapter;
    private List<GroupMemberDto> allMembers = new ArrayList<>();
    private List<GroupMemberDto> filteredMembers = new ArrayList<>();

    public static GroupMembersFragment newInstance(String groupId, String myRole, String myUserId) {
        GroupMembersFragment fragment = new GroupMembersFragment();
        Bundle args = new Bundle();
        args.putString(ARG_GROUP_ID, groupId);
        args.putString(ARG_MY_ROLE, myRole);
        args.putString(ARG_MY_USER_ID, myUserId);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            groupId = getArguments().getString(ARG_GROUP_ID);
            myRole = getArguments().getString(ARG_MY_ROLE, "member");
            myUserId = getArguments().getString(ARG_MY_USER_ID, "");
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentGroupMembersBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        authRepo = AppServices.authRepository(requireContext());

        setupRecyclerView();
        setupSearch();
        setupAddMember();
        loadMembers();
    }

    private void setupRecyclerView() {
        adapter = new GroupMemberAdapter(filteredMembers, this);
        binding.recyclerMembers.setLayoutManager(new LinearLayoutManager(requireContext()));
        binding.recyclerMembers.setAdapter(adapter);
    }

    private void setupSearch() {
        binding.searchInput.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                filterMembers(s.toString());
            }

            @Override
            public void afterTextChanged(Editable s) {}
        });
    }

    private void setupAddMember() {
        boolean canInvite = "owner".equals(myRole) || "admin".equals(myRole);
        binding.btnAddMember.setVisibility(canInvite ? View.VISIBLE : View.GONE);
        binding.btnAddMember.setOnClickListener(v -> showAddMemberDialog());
    }

    private void showAddMemberDialog() {
        // Получаем ссылку-приглашение и делимся
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
                            shareIntent.putExtra(Intent.EXTRA_TEXT, getString(R.string.group_invite_message) + "\n" + link);
                            startActivity(Intent.createChooser(shareIntent, getString(R.string.group_invite_title)));
                        });
                    }
                } else {
                    // Если нет ссылки, создаём новую
                    JsonObject createRes = client.createGroupInvite(token, groupId);
                    if (createRes != null && createRes.has("success") && createRes.get("success").getAsBoolean()) {
                        String link = createRes.has("link") ? createRes.get("link").getAsString() : null;
                        if (!TextUtils.isEmpty(link)) {
                            requireActivity().runOnUiThread(() -> {
                                Intent shareIntent = new Intent(Intent.ACTION_SEND);
                                shareIntent.setType("text/plain");
                                shareIntent.putExtra(Intent.EXTRA_TEXT, getString(R.string.group_invite_message) + "\n" + link);
                                startActivity(Intent.createChooser(shareIntent, getString(R.string.group_invite_title)));
                            });
                        }
                    }
                }
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void filterMembers(String query) {
        filteredMembers.clear();
        if (TextUtils.isEmpty(query)) {
            filteredMembers.addAll(allMembers);
        } else {
            String lowerQuery = query.toLowerCase();
            for (GroupMemberDto member : allMembers) {
                if (member.getUsername() != null && 
                    member.getUsername().toLowerCase().contains(lowerQuery)) {
                    filteredMembers.add(member);
                }
            }
        }
        adapter.notifyDataSetChanged();
    }

    private void loadMembers() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        binding.progressBar.setVisibility(View.VISIBLE);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getGroupMembers(token, groupId);

                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    JsonArray membersArray = res.has("members") ? res.getAsJsonArray("members") : null;
                    
                    List<GroupMemberDto> members = new ArrayList<>();
                    if (membersArray != null) {
                        Gson gson = new Gson();
                        for (JsonElement element : membersArray) {
                            JsonObject obj = element.getAsJsonObject();
                            GroupMemberDto member = new GroupMemberDto();
                            member.setUserId(obj.has("user_id") ? obj.get("user_id").getAsString() : "");
                            member.setUsername(obj.has("username") ? obj.get("username").getAsString() : "");
                            member.setRole(obj.has("role") ? obj.get("role").getAsString() : "member");
                            member.setMuted(obj.has("is_muted") && obj.get("is_muted").getAsBoolean());
                            member.setBanned(obj.has("is_banned") && obj.get("is_banned").getAsBoolean());
                            member.setJoinedAt(obj.has("joined_at") ? obj.get("joined_at").getAsString() : "");
                            
                            if (obj.has("permissions") && !obj.get("permissions").isJsonNull()) {
                                JsonObject perms = obj.getAsJsonObject("permissions");
                                GroupMemberDto.AdminPermissions permissions = new GroupMemberDto.AdminPermissions();
                                permissions.setCanChangeInfo(perms.has("can_change_info") && perms.get("can_change_info").getAsBoolean());
                                permissions.setCanDeleteMessages(perms.has("can_delete_messages") && perms.get("can_delete_messages").getAsBoolean());
                                permissions.setCanBanUsers(perms.has("can_ban_users") && perms.get("can_ban_users").getAsBoolean());
                                permissions.setCanInviteUsers(perms.has("can_invite_users") && perms.get("can_invite_users").getAsBoolean());
                                permissions.setCanPinMessages(perms.has("can_pin_messages") && perms.get("can_pin_messages").getAsBoolean());
                                permissions.setCanManageVoice(perms.has("can_manage_voice") && perms.get("can_manage_voice").getAsBoolean());
                                permissions.setCanPromoteMembers(perms.has("can_promote_members") && perms.get("can_promote_members").getAsBoolean());
                                permissions.setAnonymous(perms.has("is_anonymous") && perms.get("is_anonymous").getAsBoolean());
                                member.setPermissions(permissions);
                            }
                            
                            members.add(member);
                        }
                    }

                    requireActivity().runOnUiThread(() -> {
                        binding.progressBar.setVisibility(View.GONE);
                        allMembers.clear();
                        allMembers.addAll(members);
                        filterMembers(binding.searchInput.getText().toString());
                    });
                } else {
                    requireActivity().runOnUiThread(() -> {
                        binding.progressBar.setVisibility(View.GONE);
                        Toast.makeText(requireContext(), "Ошибка загрузки участников", Toast.LENGTH_SHORT).show();
                    });
                }
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> {
                    binding.progressBar.setVisibility(View.GONE);
                    Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    public void updateData(String groupId, String myRole, String myUserId) {
        this.groupId = groupId;
        this.myRole = myRole;
        this.myUserId = myUserId;
        
        if (binding != null) {
            setupAddMember();
            loadMembers();
        }
    }

    @Override
    public void onMemberClick(GroupMemberDto member) {
        // Открыть профиль
        Intent intent = new Intent(requireContext(), ProfileActivity.class);
        intent.putExtra("user_id", member.getUserId());
        startActivity(intent);
    }

    @Override
    public void onMemberLongClick(GroupMemberDto member) {
        showMemberActionsSheet(member);
    }

    private void openChatWithMember(GroupMemberDto member) {
        Intent intent = new Intent(requireContext(), ChatDetailActivity.class);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, member.getUserId());
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, member.getUsername());
        intent.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, false);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, false);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_SAVED, false);
        startActivity(intent);
    }

    private void showMemberActionsSheet(GroupMemberDto member) {
        BottomSheetDialog dialog = new BottomSheetDialog(requireContext());
        BottomSheetMemberActionsBinding sheetBinding = BottomSheetMemberActionsBinding.inflate(getLayoutInflater());
        dialog.setContentView(sheetBinding.getRoot());

        // Заголовок
        if (!TextUtils.isEmpty(member.getUsername())) {
            sheetBinding.username.setText(member.getUsername());
            sheetBinding.avatarLetter.setText(String.valueOf(member.getUsername().charAt(0)).toUpperCase());
        }
        
        String roleText = "";
        switch (member.getRole()) {
            case "owner":
                roleText = "Владелец";
                break;
            case "admin":
                roleText = "Администратор";
                break;
            default:
                roleText = "Участник";
                break;
        }
        sheetBinding.roleLabel.setText(roleText);

        // Открыть профиль
        sheetBinding.actionProfile.setOnClickListener(v -> {
            dialog.dismiss();
            onMemberClick(member);
        });

        // Написать - открыть чат с пользователем
        sheetBinding.actionMessage.setOnClickListener(v -> {
            dialog.dismiss();
            openChatWithMember(member);
        });

        // Админ-действия
        boolean isOwner = "owner".equals(myRole);
        boolean isAdmin = "admin".equals(myRole);
        boolean isMemberOwner = "owner".equals(member.getRole());
        boolean isMemberAdmin = "admin".equals(member.getRole());
        boolean isSelf = member.getUserId().equals(myUserId);

        // Показываем админ-действия только если мы админ/владелец и это не мы сами
        if ((isOwner || isAdmin) && !isSelf) {
            sheetBinding.adminDivider.setVisibility(View.VISIBLE);

            // Назначить/снять админа
            if (isOwner || (isAdmin && !isMemberOwner && !isMemberAdmin)) {
                sheetBinding.actionPromote.setVisibility(View.VISIBLE);
                if (isMemberAdmin) {
                    sheetBinding.promoteText.setText("Изменить права");
                } else {
                    sheetBinding.promoteText.setText("Назначить админом");
                }
                sheetBinding.actionPromote.setOnClickListener(v -> {
                    dialog.dismiss();
                    openAdminPermissions(member);
                });
            }

            // Заглушить
            if (!isMemberOwner && (isOwner || !isMemberAdmin)) {
                sheetBinding.actionMute.setVisibility(View.VISIBLE);
                if (member.isMuted()) {
                    sheetBinding.muteText.setText("Снять глушение");
                }
                sheetBinding.actionMute.setOnClickListener(v -> {
                    dialog.dismiss();
                    toggleMute(member);
                });
            }

            // Исключить
            if (!isMemberOwner && (isOwner || !isMemberAdmin)) {
                sheetBinding.actionKick.setVisibility(View.VISIBLE);
                sheetBinding.actionKick.setOnClickListener(v -> {
                    dialog.dismiss();
                    showKickConfirmation(member);
                });
            }

            // Заблокировать
            if (!isMemberOwner && (isOwner || !isMemberAdmin)) {
                sheetBinding.actionBan.setVisibility(View.VISIBLE);
                sheetBinding.actionBan.setOnClickListener(v -> {
                    dialog.dismiss();
                    showBanConfirmation(member);
                });
            }
        }

        dialog.show();
    }

    private void openAdminPermissions(GroupMemberDto member) {
        Intent intent = new Intent(requireContext(), AdminPermissionsActivity.class);
        intent.putExtra(AdminPermissionsActivity.EXTRA_GROUP_ID, groupId);
        intent.putExtra(AdminPermissionsActivity.EXTRA_USER_ID, member.getUserId());
        intent.putExtra(AdminPermissionsActivity.EXTRA_USERNAME, member.getUsername());
        intent.putExtra(AdminPermissionsActivity.EXTRA_CURRENT_ROLE, member.getRole());
        if (member.getPermissions() != null) {
            intent.putExtra(AdminPermissionsActivity.EXTRA_PERMISSIONS, new Gson().toJson(member.getPermissions()));
        }
        startActivity(intent);
    }

    private void toggleMute(GroupMemberDto member) {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.muteMember(token, groupId, member.getUserId(), !member.isMuted());
                
                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        member.setMuted(!member.isMuted());
                        adapter.notifyDataSetChanged();
                        String msg = member.isMuted() ? "Участник заглушен" : "Глушение снято";
                        Toast.makeText(requireContext(), msg, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(requireContext(), "Ошибка", Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                        Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void showKickConfirmation(GroupMemberDto member) {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle("Исключить участника")
                .setMessage("Исключить " + member.getUsername() + " из группы?")
                .setPositiveButton("Исключить", (dialog, which) -> kickMember(member))
                .setNegativeButton("Отмена", null)
                .show();
    }

    private void kickMember(GroupMemberDto member) {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.kickMember(token, groupId, member.getUserId());
                
                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        allMembers.remove(member);
                        filterMembers(binding.searchInput.getText().toString());
                        Toast.makeText(requireContext(), "Участник исключён", Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(requireContext(), "Ошибка исключения", Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                        Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void showBanConfirmation(GroupMemberDto member) {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle("Заблокировать участника")
                .setMessage("Заблокировать " + member.getUsername() + "? Он не сможет вернуться в группу.")
                .setPositiveButton("Заблокировать", (dialog, which) -> banMember(member))
                .setNegativeButton("Отмена", null)
                .show();
    }

    private void banMember(GroupMemberDto member) {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.banMember(token, groupId, member.getUserId(), true);
                
                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        allMembers.remove(member);
                        filterMembers(binding.searchInput.getText().toString());
                        Toast.makeText(requireContext(), "Участник заблокирован", Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(requireContext(), "Ошибка блокировки", Toast.LENGTH_SHORT).show();
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
    public void onResume() {
        super.onResume();
        // Обновляем список при возврате (после редактирования прав)
        loadMembers();
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
