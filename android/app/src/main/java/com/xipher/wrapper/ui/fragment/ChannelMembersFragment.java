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
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.ChannelMemberDto;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.BottomSheetChannelMemberActionsBinding;
import com.xipher.wrapper.databinding.FragmentChannelMembersBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.ChannelAdminPermissionsActivity;
import com.xipher.wrapper.ui.ProfileActivity;
import com.xipher.wrapper.ui.adapter.ChannelMemberAdapter;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Фрагмент со списком подписчиков канала
 */
public class ChannelMembersFragment extends Fragment implements ChannelMemberAdapter.OnMemberClickListener {
    private static final String ARG_CHANNEL_ID = "channel_id";
    private static final String ARG_MY_ROLE = "my_role";
    private static final String ARG_IS_ADMIN = "is_admin";
    private static final String ARG_MY_USER_ID = "my_user_id";

    private FragmentChannelMembersBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String channelId;
    private String myRole = "";
    private boolean isAdmin = false;
    private String myUserId = "";

    private ChannelMemberAdapter adapter;
    private List<ChannelMemberDto> allMembers = new ArrayList<>();
    private List<ChannelMemberDto> filteredMembers = new ArrayList<>();

    public static ChannelMembersFragment newInstance(String channelId, String myRole, boolean isAdmin, String myUserId) {
        ChannelMembersFragment fragment = new ChannelMembersFragment();
        Bundle args = new Bundle();
        args.putString(ARG_CHANNEL_ID, channelId);
        args.putString(ARG_MY_ROLE, myRole);
        args.putBoolean(ARG_IS_ADMIN, isAdmin);
        args.putString(ARG_MY_USER_ID, myUserId);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            channelId = getArguments().getString(ARG_CHANNEL_ID);
            myRole = getArguments().getString(ARG_MY_ROLE, "");
            isAdmin = getArguments().getBoolean(ARG_IS_ADMIN);
            myUserId = getArguments().getString(ARG_MY_USER_ID, "");
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentChannelMembersBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        authRepo = AppServices.authRepository(requireContext());

        setupRecyclerView();
        setupSearch();
        loadMembers();
    }

    private void setupRecyclerView() {
        adapter = new ChannelMemberAdapter(filteredMembers, this);
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

    private void filterMembers(String query) {
        filteredMembers.clear();
        if (TextUtils.isEmpty(query)) {
            filteredMembers.addAll(allMembers);
        } else {
            String lowerQuery = query.toLowerCase();
            for (ChannelMemberDto member : allMembers) {
                if (member.getUsername() != null &&
                        member.getUsername().toLowerCase().contains(lowerQuery)) {
                    filteredMembers.add(member);
                }
            }
        }
        adapter.notifyDataSetChanged();
        updateMemberCount();
    }

    private void updateMemberCount() {
        binding.memberCount.setText(String.format("%d участников", allMembers.size()));
    }

    private void loadMembers() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;

        binding.progressBar.setVisibility(View.VISIBLE);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getChannelMembers(token, channelId);

                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    JsonArray membersArray = res.has("members") ? res.getAsJsonArray("members") : null;

                    List<ChannelMemberDto> members = new ArrayList<>();
                    if (membersArray != null) {
                        for (JsonElement element : membersArray) {
                            JsonObject obj = element.getAsJsonObject();
                            ChannelMemberDto member = new ChannelMemberDto();
                            member.setUserId(obj.has("user_id") ? obj.get("user_id").getAsString() : "");
                            member.setUsername(obj.has("username") ? obj.get("username").getAsString() : "");
                            member.setRole(obj.has("role") ? obj.get("role").getAsString() : "subscriber");
                            member.setBanned(obj.has("is_banned") && obj.get("is_banned").getAsBoolean());
                            member.setAdminPerms(obj.has("admin_perms") ? obj.get("admin_perms").getAsInt() : 0);
                            member.setAdminTitle(obj.has("admin_title") ? obj.get("admin_title").getAsString() : "");
                            member.setJoinedAt(obj.has("joined_at") ? obj.get("joined_at").getAsString() : "");
                            members.add(member);
                        }
                    }

                    // Сортировка: owner > admin > subscriber
                    members.sort((a, b) -> {
                        int orderA = getRoleOrder(a.getRole());
                        int orderB = getRoleOrder(b.getRole());
                        return Integer.compare(orderA, orderB);
                    });

                    requireActivity().runOnUiThread(() -> {
                        binding.progressBar.setVisibility(View.GONE);
                        allMembers.clear();
                        allMembers.addAll(members);
                        filterMembers(binding.searchInput.getText().toString());
                    });
                } else {
                    requireActivity().runOnUiThread(() -> {
                        binding.progressBar.setVisibility(View.GONE);
                        Toast.makeText(requireContext(), R.string.channel_members_load_failed, Toast.LENGTH_SHORT).show();
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

    private int getRoleOrder(String role) {
        switch (role != null ? role : "") {
            case "owner":
            case "creator":
                return 0;
            case "admin":
                return 1;
            default:
                return 2;
        }
    }

    public void updateData(String channelId, String myRole, boolean isAdmin, String myUserId) {
        this.channelId = channelId;
        this.myRole = myRole;
        this.isAdmin = isAdmin;
        this.myUserId = myUserId;

        if (binding != null) {
            loadMembers();
        }
    }

    @Override
    public void onMemberClick(ChannelMemberDto member) {
        // Открыть профиль
        Intent intent = new Intent(requireContext(), ProfileActivity.class);
        intent.putExtra("user_id", member.getUserId());
        startActivity(intent);
    }

    @Override
    public void onMemberLongClick(ChannelMemberDto member) {
        if (isAdmin) {
            showMemberActionsSheet(member);
        }
    }

    private void showMemberActionsSheet(ChannelMemberDto member) {
        BottomSheetDialog dialog = new BottomSheetDialog(requireContext());
        BottomSheetChannelMemberActionsBinding sheetBinding = 
                BottomSheetChannelMemberActionsBinding.inflate(getLayoutInflater());
        dialog.setContentView(sheetBinding.getRoot());

        // Заголовок
        if (!TextUtils.isEmpty(member.getUsername())) {
            sheetBinding.username.setText(member.getUsername());
            sheetBinding.avatarLetter.setText(String.valueOf(member.getUsername().charAt(0)).toUpperCase());
        }

        sheetBinding.roleLabel.setText(member.getRoleDisplayName());

        // Открыть профиль
        sheetBinding.actionProfile.setOnClickListener(v -> {
            dialog.dismiss();
            onMemberClick(member);
        });

        // Написать
        sheetBinding.actionMessage.setOnClickListener(v -> {
            dialog.dismiss();
            Toast.makeText(requireContext(), "Написать " + member.getUsername(), Toast.LENGTH_SHORT).show();
        });

        // Админ-действия
        boolean isSelf = member.getUserId().equals(myUserId);
        boolean isMemberOwner = member.isOwner();
        boolean isMemberAdmin = member.isAdmin();
        boolean amOwner = "owner".equals(myRole) || "creator".equals(myRole);

        if (isAdmin && !isSelf) {
            sheetBinding.adminDivider.setVisibility(View.VISIBLE);

            // Назначить/изменить права админа
            if (amOwner || (!isMemberOwner && !isMemberAdmin)) {
                sheetBinding.actionPromote.setVisibility(View.VISIBLE);
                if (isMemberAdmin) {
                    sheetBinding.promoteText.setText(R.string.channel_edit_admin_rights);
                } else {
                    sheetBinding.promoteText.setText(R.string.channel_make_admin);
                }
                sheetBinding.actionPromote.setOnClickListener(v -> {
                    dialog.dismiss();
                    openAdminPermissions(member);
                });
            }

            // Заблокировать
            if (!isMemberOwner && (amOwner || !isMemberAdmin)) {
                sheetBinding.actionBan.setVisibility(View.VISIBLE);
                if (member.isBanned()) {
                    sheetBinding.banText.setText(R.string.channel_unban);
                } else {
                    sheetBinding.banText.setText(R.string.channel_ban);
                }
                sheetBinding.actionBan.setOnClickListener(v -> {
                    dialog.dismiss();
                    if (member.isBanned()) {
                        unbanMember(member);
                    } else {
                        showBanConfirmation(member);
                    }
                });
            }
        }

        dialog.show();
    }

    private void openAdminPermissions(ChannelMemberDto member) {
        Intent intent = new Intent(requireContext(), ChannelAdminPermissionsActivity.class);
        intent.putExtra(ChannelAdminPermissionsActivity.EXTRA_CHANNEL_ID, channelId);
        intent.putExtra(ChannelAdminPermissionsActivity.EXTRA_USER_ID, member.getUserId());
        intent.putExtra(ChannelAdminPermissionsActivity.EXTRA_USERNAME, member.getUsername());
        intent.putExtra(ChannelAdminPermissionsActivity.EXTRA_CURRENT_ROLE, member.getRole());
        intent.putExtra(ChannelAdminPermissionsActivity.EXTRA_PERMISSIONS, member.getAdminPerms());
        intent.putExtra(ChannelAdminPermissionsActivity.EXTRA_ADMIN_TITLE, member.getAdminTitle());
        startActivity(intent);
    }

    private void showBanConfirmation(ChannelMemberDto member) {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.channel_ban_title)
                .setMessage(getString(R.string.channel_ban_message, member.getUsername()))
                .setPositiveButton(R.string.channel_ban, (dialog, which) -> banMember(member))
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void banMember(ChannelMemberDto member) {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.banChannelMember(token, channelId, member.getUserId(), true);

                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        member.setBanned(true);
                        adapter.notifyDataSetChanged();
                        Toast.makeText(requireContext(), R.string.channel_member_banned, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(requireContext(), R.string.channel_ban_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                        Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void unbanMember(ChannelMemberDto member) {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.banChannelMember(token, channelId, member.getUserId(), false);

                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        member.setBanned(false);
                        adapter.notifyDataSetChanged();
                        Toast.makeText(requireContext(), R.string.channel_member_unbanned, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(requireContext(), R.string.channel_unban_failed, Toast.LENGTH_SHORT).show();
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
