package com.xipher.wrapper.ui;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.tabs.TabLayoutMediator;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityGroupInfoBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.fragment.GroupMembersFragment;
import com.xipher.wrapper.ui.fragment.GroupOverviewFragment;
import com.xipher.wrapper.ui.fragment.GroupSettingsFragment;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class GroupInfoActivity extends AppCompatActivity {
    public static final String EXTRA_GROUP_ID = "group_id";
    public static final String EXTRA_GROUP_TITLE = "group_title";

    private ActivityGroupInfoBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    
    private String groupId;
    private String groupTitle;
    private String myRole = "member";
    private int memberCount = 0;
    private String myUserId = "";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityGroupInfoBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        groupId = getIntent() != null ? getIntent().getStringExtra(EXTRA_GROUP_ID) : null;
        groupTitle = getIntent() != null ? getIntent().getStringExtra(EXTRA_GROUP_TITLE) : null;

        setupToolbar();
        setupViewPager();
        loadGroupInfo();
    }

    private void setupToolbar() {
        binding.toolbar.setNavigationOnClickListener(v -> finish());

        if (!TextUtils.isEmpty(groupTitle)) {
            binding.groupName.setText(groupTitle);
            if (!TextUtils.isEmpty(groupTitle)) {
                binding.avatarLetter.setText(String.valueOf(groupTitle.charAt(0)).toUpperCase());
            }
        }
    }

    private void setupViewPager() {
        GroupPagerAdapter adapter = new GroupPagerAdapter(this);
        binding.viewPager.setAdapter(adapter);

        new TabLayoutMediator(binding.tabLayout, binding.viewPager, (tab, position) -> {
            switch (position) {
                case 0:
                    tab.setText(R.string.group_tab_overview);
                    break;
                case 1:
                    tab.setText(R.string.group_tab_members);
                    break;
                case 2:
                    tab.setText(R.string.group_tab_settings);
                    break;
            }
        }).attach();
    }

    private void loadGroupInfo() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(groupId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getGroupInfo(token, groupId);
                
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    JsonObject group = res.has("group") ? res.getAsJsonObject("group") : null;
                    // Server returns user_role, not role
                    String role = res.has("user_role") ? res.get("user_role").getAsString() : "member";
                    String userId = res.has("user_id") ? res.get("user_id").getAsString() : "";
                    
                    if (group != null) {
                        String name = group.has("name") ? group.get("name").getAsString() : "";
                        int count = group.has("member_count") ? group.get("member_count").getAsInt() : 0;
                        
                        runOnUiThread(() -> {
                            myRole = role;
                            myUserId = userId;
                            memberCount = count;
                            groupTitle = name;
                            
                            binding.groupName.setText(name);
                            binding.memberCount.setText(count + " участников");
                            
                            if (!TextUtils.isEmpty(name)) {
                                binding.avatarLetter.setText(String.valueOf(name.charAt(0)).toUpperCase());
                            }
                            
                            // Уведомляем фрагменты
                            notifyFragments();
                        });
                    }
                }
            } catch (Exception e) {
                runOnUiThread(() -> 
                    Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void notifyFragments() {
        for (Fragment fragment : getSupportFragmentManager().getFragments()) {
            if (fragment instanceof GroupOverviewFragment) {
                ((GroupOverviewFragment) fragment).updateData(groupId, groupTitle, myRole, memberCount);
            } else if (fragment instanceof GroupMembersFragment) {
                ((GroupMembersFragment) fragment).updateData(groupId, myRole, myUserId);
            } else if (fragment instanceof GroupSettingsFragment) {
                ((GroupSettingsFragment) fragment).updateData(groupId, groupTitle, myRole);
            }
        }
    }

    public String getGroupId() {
        return groupId;
    }

    public String getMyRole() {
        return myRole;
    }

    public String getMyUserId() {
        return myUserId;
    }

    public int getMemberCount() {
        return memberCount;
    }

    public void switchToMembersTab() {
        binding.viewPager.setCurrentItem(1, true);
    }

    public void switchToSettingsTab() {
        boolean isAdmin = "admin".equals(myRole) || "creator".equals(myRole);
        if (isAdmin) {
            binding.viewPager.setCurrentItem(2, true);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }

    private class GroupPagerAdapter extends FragmentStateAdapter {
        public GroupPagerAdapter(@NonNull AppCompatActivity activity) {
            super(activity);
        }

        @NonNull
        @Override
        public Fragment createFragment(int position) {
            switch (position) {
                case 0:
                    return GroupOverviewFragment.newInstance(groupId, groupTitle, myRole);
                case 1:
                    return GroupMembersFragment.newInstance(groupId, myRole, myUserId);
                case 2:
                    return GroupSettingsFragment.newInstance(groupId, groupTitle, myRole);
                default:
                    return GroupOverviewFragment.newInstance(groupId, groupTitle, myRole);
            }
        }

        @Override
        public int getItemCount() {
            // Показываем вкладку настроек для всех (но редактирование только для админов)
            return 3;
        }
    }
}
