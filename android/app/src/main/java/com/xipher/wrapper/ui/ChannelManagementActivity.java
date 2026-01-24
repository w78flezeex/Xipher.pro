package com.xipher.wrapper.ui;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
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
import com.xipher.wrapper.databinding.ActivityChannelManagementBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.fragment.ChannelMembersFragment;
import com.xipher.wrapper.ui.fragment.ChannelOverviewFragment;
import com.xipher.wrapper.ui.fragment.ChannelSettingsFragment;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Активность для управления каналом (в стиле Telegram)
 * - Информация (обзор)
 * - Подписчики
 * - Настройки (для админов)
 */
public class ChannelManagementActivity extends AppCompatActivity {
    public static final String EXTRA_CHANNEL_ID = "channel_id";
    public static final String EXTRA_CHANNEL_TITLE = "channel_title";

    private ActivityChannelManagementBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String channelId;
    private String channelTitle;
    private String myRole = "";
    private boolean isAdmin = false;
    private int subscriberCount = 0;
    private String myUserId = "";
    private String channelDescription = "";
    private String channelUsername = "";
    private boolean isPrivate = false;
    private boolean showAuthor = true;
    private String avatarUrl = "";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityChannelManagementBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        channelId = getIntent() != null ? getIntent().getStringExtra(EXTRA_CHANNEL_ID) : null;
        channelTitle = getIntent() != null ? getIntent().getStringExtra(EXTRA_CHANNEL_TITLE) : null;

        setupToolbar();
        loadChannelInfo();
    }

    private void setupToolbar() {
        binding.toolbar.setNavigationOnClickListener(v -> finish());

        if (!TextUtils.isEmpty(channelTitle)) {
            binding.channelName.setText(channelTitle);
            binding.avatarLetter.setText(String.valueOf(channelTitle.charAt(0)).toUpperCase());
        }
    }

    private void setupViewPager() {
        ChannelPagerAdapter adapter = new ChannelPagerAdapter(this);
        binding.viewPager.setAdapter(adapter);

        new TabLayoutMediator(binding.tabLayout, binding.viewPager, (tab, position) -> {
            switch (position) {
                case 0:
                    tab.setText(R.string.channel_tab_overview);
                    break;
                case 1:
                    tab.setText(R.string.channel_tab_subscribers);
                    break;
                case 2:
                    if (isAdmin) {
                        tab.setText(R.string.channel_tab_settings);
                    }
                    break;
            }
        }).attach();
    }

    private void loadChannelInfo() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;

        binding.progressBar.setVisibility(View.VISIBLE);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getChannelInfo(token, channelId);

                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    JsonObject channel = res.has("channel") ? res.getAsJsonObject("channel") : null;
                    String role = res.has("user_role") ? res.get("user_role").getAsString() : "";
                    boolean admin = res.has("is_admin") && res.get("is_admin").getAsBoolean();
                    String userId = res.has("user_id") ? res.get("user_id").getAsString() : "";
                    int subCountTemp = res.has("subscribers_count") ? res.get("subscribers_count").getAsInt() : 0;
                    if (subCountTemp == 0 && res.has("total_members")) {
                        subCountTemp = res.get("total_members").getAsInt();
                    }
                    final int subCount = subCountTemp;

                    if (channel != null) {
                        String name = channel.has("name") ? channel.get("name").getAsString() : "";
                        String desc = channel.has("description") ? channel.get("description").getAsString() : "";
                        String username = channel.has("custom_link") ? channel.get("custom_link").getAsString() : "";
                        boolean priv = channel.has("is_private") && channel.get("is_private").getAsBoolean();
                        boolean show = !channel.has("show_author") || channel.get("show_author").getAsBoolean();
                        String avatar = channel.has("avatar_url") ? channel.get("avatar_url").getAsString() : "";

                        runOnUiThread(() -> {
                            binding.progressBar.setVisibility(View.GONE);
                            
                            myRole = role;
                            isAdmin = admin || "owner".equals(role) || "creator".equals(role) || "admin".equals(role);
                            myUserId = userId;
                            subscriberCount = subCount;
                            channelTitle = name;
                            channelDescription = desc;
                            channelUsername = username;
                            isPrivate = priv;
                            showAuthor = show;
                            avatarUrl = avatar;

                            binding.channelName.setText(name);
                            binding.subscriberCount.setText(formatSubscriberCount(subCount));

                            if (!TextUtils.isEmpty(name)) {
                                binding.avatarLetter.setText(String.valueOf(name.charAt(0)).toUpperCase());
                            }
                            
                            // Иконка канала
                            binding.channelIcon.setText(isPrivate ? "🔒" : "📢");

                            // Настраиваем ViewPager после получения информации
                            setupViewPager();
                            notifyFragments();
                        });
                    } else {
                        runOnUiThread(() -> {
                            binding.progressBar.setVisibility(View.GONE);
                            Toast.makeText(this, R.string.channel_info_load_failed, Toast.LENGTH_SHORT).show();
                        });
                    }
                } else {
                    runOnUiThread(() -> {
                        binding.progressBar.setVisibility(View.GONE);
                        Toast.makeText(this, R.string.channel_info_load_failed, Toast.LENGTH_SHORT).show();
                    });
                }
            } catch (Exception e) {
                runOnUiThread(() -> {
                    binding.progressBar.setVisibility(View.GONE);
                    Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void notifyFragments() {
        for (Fragment fragment : getSupportFragmentManager().getFragments()) {
            if (fragment instanceof ChannelOverviewFragment) {
                ((ChannelOverviewFragment) fragment).updateData(
                        channelId, channelTitle, channelDescription, channelUsername,
                        isPrivate, showAuthor, myRole, isAdmin, subscriberCount
                );
            } else if (fragment instanceof ChannelMembersFragment) {
                ((ChannelMembersFragment) fragment).updateData(channelId, myRole, isAdmin, myUserId);
            } else if (fragment instanceof ChannelSettingsFragment) {
                ((ChannelSettingsFragment) fragment).updateData(
                        channelId, channelTitle, channelDescription, channelUsername,
                        isPrivate, showAuthor, myRole
                );
            }
        }
    }

    private String formatSubscriberCount(int count) {
        int abs = Math.abs(count) % 100;
        int last = abs % 10;
        if (abs > 10 && abs < 20) return count + " подписчиков";
        if (last == 1) return count + " подписчик";
        if (last > 1 && last < 5) return count + " подписчика";
        return count + " подписчиков";
    }

    // Публичные методы для фрагментов
    public String getChannelId() { return channelId; }
    public String getMyRole() { return myRole; }
    public boolean isAdmin() { return isAdmin; }
    public String getMyUserId() { return myUserId; }
    public int getSubscriberCount() { return subscriberCount; }

    public void copyToClipboard(String text) {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText("channel_link", text);
        clipboard.setPrimaryClip(clip);
        Toast.makeText(this, R.string.copied_to_clipboard, Toast.LENGTH_SHORT).show();
    }

    public void refreshData() {
        loadChannelInfo();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }

    private class ChannelPagerAdapter extends FragmentStateAdapter {
        public ChannelPagerAdapter(@NonNull AppCompatActivity activity) {
            super(activity);
        }

        @NonNull
        @Override
        public Fragment createFragment(int position) {
            switch (position) {
                case 0:
                    return ChannelOverviewFragment.newInstance(
                            channelId, channelTitle, channelDescription, channelUsername,
                            isPrivate, showAuthor, myRole, isAdmin, subscriberCount
                    );
                case 1:
                    return ChannelMembersFragment.newInstance(channelId, myRole, isAdmin, myUserId);
                case 2:
                    return ChannelSettingsFragment.newInstance(
                            channelId, channelTitle, channelDescription, channelUsername,
                            isPrivate, showAuthor, myRole
                    );
                default:
                    return ChannelOverviewFragment.newInstance(
                            channelId, channelTitle, channelDescription, channelUsername,
                            isPrivate, showAuthor, myRole, isAdmin, subscriberCount
                    );
            }
        }

        @Override
        public int getItemCount() {
            return isAdmin ? 3 : 2;
        }
    }
}
