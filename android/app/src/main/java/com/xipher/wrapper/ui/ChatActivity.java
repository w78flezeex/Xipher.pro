package com.xipher.wrapper.ui;

import android.content.Intent;
import android.graphics.RenderEffect;
import android.graphics.Shader;
import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.core.view.GravityCompat;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.bumptech.glide.Glide;
import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.google.android.material.switchmaterial.SwitchMaterial;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.R;
import com.xipher.wrapper.UpdateManager;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.BasicResponse;
import com.xipher.wrapper.data.model.ChatFolder;
import com.xipher.wrapper.data.model.ChatPinDto;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityChatBinding;
import com.xipher.wrapper.notifications.NotificationHelper;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.security.SecurityContext;
import com.xipher.wrapper.theme.ThemeManager;
import com.xipher.wrapper.ui.adapter.ChatListAdapter;
import com.xipher.wrapper.ui.vm.ChatListViewModel;
import com.xipher.wrapper.di.AppServices;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import org.json.JSONObject;

public class ChatActivity extends AppCompatActivity {

    private static final String UPDATE_PATH = "/app/version.json";
    private static final String SEARCH_USER_PREFIX = "search:user:";
    private static final String SEARCH_CHANNEL_PREFIX = "search:channel:";
    private static final int REMOTE_SEARCH_MIN = 3;
    private static final long REMOTE_SEARCH_DELAY_MS = 400L;

    private ActivityChatBinding binding;
    private ChatListViewModel vm;
    private ChatListAdapter adapter;
    private boolean isTwoPane;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private AppDatabase db;
    private List<ChatEntity> currentChats = new ArrayList<>();
    private static final String FOLDER_ALL_ID = "all";
    private List<ChatFolder> chatFolders = new ArrayList<>();
    private String activeFolderId = FOLDER_ALL_ID;
    private UpdateManager updateManager;
    private JSONObject updatePayload;
    private static final long PINNED_SYNC_INTERVAL_MS = 15000L;
    private static final long FOLDERS_SYNC_INTERVAL_MS = 15000L;
    private boolean pinSyncInFlight = false;
    private long lastPinSyncAt = 0L;
    private boolean folderSyncInFlight = false;
    private long lastFolderSyncAt = 0L;
    private ImageView drawerAvatarImage;
    private TextView drawerAvatarLetter;
    private TextView drawerUsername;
    private TextView drawerSubtitle;
    private SwitchMaterial drawerThemeSwitch;
    private View drawerProfile;
    private View drawerSaved;
    private View drawerWallet;
    private View drawerCreateGroup;
    private View drawerCreateChannel;
    private View drawerSettings;
    private final List<ChatEntity> remoteSearchResults = new ArrayList<>();
    private ScheduledFuture<?> remoteSearchTask;
    private String lastRemoteQuery = "";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityChatBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        isTwoPane = false;
        if (binding.chatContainer != null) {
            binding.chatContainer.setVisibility(View.GONE);
        }
        authRepo = AppServices.authRepository(this);
        if (TextUtils.isEmpty(authRepo.getToken())) {
            startActivity(new Intent(this, AuthActivity.class));
            finish();
            return;
        }
        try {
            db = AppDatabase.get(this);
        } catch (IllegalStateException e) {
            // Database not initialized yet - try to initialize it
            try {
                String key = com.xipher.wrapper.security.DbKeyStore.getOrCreateMainKey(this);
                com.xipher.wrapper.data.db.DatabaseManager.getInstance().init(getApplicationContext(), key);
                db = AppDatabase.get(this);
            } catch (Exception ex) {
                // Still failed - show error but don't redirect
                android.util.Log.e("ChatActivity", "Database init failed", ex);
                db = null;
            }
        }
        vm = new ViewModelProvider(this).get(ChatListViewModel.class);
        if (!isPanicMode()) {
            PushTokenManager.syncIfPossible(this);
        }

        adapter = new ChatListAdapter(chat -> {
            if (chat == null) return;
            if (isSearchResult(chat)) {
                handleSearchResultClick(chat);
                return;
            }
            openChatDetail(chat);
        });
        adapter.setOnLongClickListener(this::showChatActions);

        binding.recyclerChats.setLayoutManager(new LinearLayoutManager(this));
        binding.recyclerChats.setAdapter(adapter);
        binding.swipeRefresh.setOnRefreshListener(() -> {
            vm.refresh();
            if (!isPanicMode()) {
                syncPinnedChats(true);
            }
        });
        binding.etSearch.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                filterChats(s != null ? s.toString() : "");
            }

            @Override
            public void afterTextChanged(Editable s) {}
        });
        binding.etSearch.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_SEARCH) {
                performUserSearch();
                return true;
            }
            return false;
        });
        binding.btnSearchUser.setOnClickListener(v -> performUserSearch());
        binding.btnCreate.setOnClickListener(v -> showCreateMenu());
        loadChatFolders();
        setupFolderBar();
        setupDrawer();
        setupBottomNav();
        setupUpdateBar();
        initUpdateManager();

        vm.getChats().observe(this, chats -> {
            currentChats = chats != null ? chats : new ArrayList<>();
            filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
            binding.swipeRefresh.setRefreshing(false);
        });

        vm.getLoading().observe(this, loading -> binding.swipeRefresh.setRefreshing(loading != null && loading));
        vm.refresh();
        if (!isPanicMode()) {
            syncPinnedChats(true);
            syncChatFolders(true);
        }

        if (savedInstanceState == null) {
            handleLaunchIntent(getIntent());
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (remoteSearchTask != null) {
            remoteSearchTask.cancel(true);
            remoteSearchTask = null;
        }
        scheduler.shutdownNow();
    }

    private void openChatDetail(ChatEntity chat) {
        Intent intent = new Intent(this, ChatDetailActivity.class);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, chat.id);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, chat.title);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, chat.isGroup);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, chat.isChannel);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_SAVED, chat.isSaved);
        startActivity(intent);
    }

    private void handleLaunchIntent(Intent intent) {
        if (intent == null) return;
        String chatId = intent.getStringExtra(NotificationHelper.EXTRA_CHAT_ID);
        if (chatId == null || chatId.isEmpty()) return;
        String chatTitle = intent.getStringExtra(NotificationHelper.EXTRA_CHAT_TITLE);
        if (chatTitle == null || chatTitle.isEmpty()) {
            chatTitle = intent.getStringExtra(NotificationHelper.EXTRA_USER_NAME);
        }
        String chatType = intent.getStringExtra(NotificationHelper.EXTRA_CHAT_TYPE);
        boolean isGroup = "group".equalsIgnoreCase(chatType);
        boolean isChannel = "channel".equalsIgnoreCase(chatType);
        boolean isSaved = authRepo.getUserId() != null && authRepo.getUserId().equals(chatId);
        Intent detail = new Intent(this, ChatDetailActivity.class);
        detail.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, chatId);
        detail.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, chatTitle);
        detail.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, isGroup);
        detail.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, isChannel);
        detail.putExtra(ChatDetailActivity.EXTRA_IS_SAVED, isSaved);
        startActivity(detail);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleLaunchIntent(intent);
    }

    @Override
    protected void onResume() {
        super.onResume();
        vm.refresh();
        if (adapter != null) {
            adapter.notifyDataSetChanged();
        }
        if (!isPanicMode()) {
            syncPinnedChats(false);
            syncChatFolders(false);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        vm.startSocket();
    }

    @Override
    protected void onStop() {
        super.onStop();
        vm.stopSocket();
    }

    private void setupDrawer() {
        binding.btnMenu.setOnClickListener(v -> openDrawer());
        binding.drawerLayout.setScrimColor(android.graphics.Color.TRANSPARENT);
        binding.blurOverlay.setOnClickListener(v -> closeDrawer());

        View drawer = binding.leftDrawer;
        drawerAvatarImage = drawer.findViewById(com.xipher.wrapper.R.id.drawerAvatarImage);
        drawerAvatarLetter = drawer.findViewById(com.xipher.wrapper.R.id.drawerAvatarLetter);
        drawerUsername = drawer.findViewById(com.xipher.wrapper.R.id.drawerUsername);
        drawerSubtitle = drawer.findViewById(com.xipher.wrapper.R.id.drawerSubtitle);
        drawerProfile = drawer.findViewById(com.xipher.wrapper.R.id.drawerProfile);
        drawerSaved = drawer.findViewById(com.xipher.wrapper.R.id.drawerSaved);
        drawerWallet = drawer.findViewById(com.xipher.wrapper.R.id.drawerWallet);
        drawerCreateGroup = drawer.findViewById(com.xipher.wrapper.R.id.drawerCreateGroup);
        drawerCreateChannel = drawer.findViewById(com.xipher.wrapper.R.id.drawerCreateChannel);
        drawerSettings = drawer.findViewById(com.xipher.wrapper.R.id.drawerSettings);
        drawerThemeSwitch = drawer.findViewById(com.xipher.wrapper.R.id.drawerThemeSwitch);

        drawerProfile.setOnClickListener(v -> {
            closeDrawer();
            openMyProfile();
        });
        drawerSaved.setOnClickListener(v -> {
            closeDrawer();
            openFavorites();
        });
        
        // Wallet click handler
        if (drawerWallet != null) {
            drawerWallet.setOnClickListener(v -> {
                closeDrawer();
                startActivity(new Intent(this, com.xipher.wrapper.wallet.WalletActivity.class));
            });
        }
        
        drawerCreateGroup.setOnClickListener(v -> {
            closeDrawer();
            openCreateChat(true);
        });
        drawerCreateChannel.setOnClickListener(v -> {
            closeDrawer();
            openCreateChat(false);
        });
        
        View drawerChannelCatalog = drawer.findViewById(com.xipher.wrapper.R.id.drawerChannelCatalog);
        if (drawerChannelCatalog != null) {
            drawerChannelCatalog.setOnClickListener(v -> {
                closeDrawer();
                startActivity(new Intent(this, ChannelCatalogActivity.class));
            });
        }
        
        drawerSettings.setOnClickListener(v -> {
            closeDrawer();
            startActivity(new Intent(this, SettingsActivity.class));
        });
        
        View drawerFaq = drawer.findViewById(com.xipher.wrapper.R.id.drawerFaq);
        if (drawerFaq != null) {
            drawerFaq.setOnClickListener(v -> {
                closeDrawer();
                startActivity(new Intent(this, FaqActivity.class));
            });
        }
        
        drawerThemeSwitch.setChecked(ThemeManager.isNight(this));
        drawerThemeSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            ThemeManager.setNight(this, isChecked);
            recreate();
        });

        updateDrawerHeader(authRepo.getUsername(), authRepo.getUserId(), null);
        loadProfileHeader();

        binding.drawerLayout.addDrawerListener(new DrawerLayout.SimpleDrawerListener() {
            @Override
            public void onDrawerSlide(View drawerView, float slideOffset) {
                updateBlur(slideOffset);
            }

            @Override
            public void onDrawerClosed(View drawerView) {
                updateBlur(0f);
            }
        });
    }

    private void setupUpdateBar() {
        binding.updateBar.setVisibility(View.GONE);
        binding.btnUpdate.setOnClickListener(v -> {
            if (updatePayload == null) {
                Toast.makeText(this, R.string.update_no_data, Toast.LENGTH_SHORT).show();
                return;
            }
            updateManager.openStore(this, updatePayload);
        });
    }

    private void initUpdateManager() {
        String updateUrl = BuildConfig.API_BASE + UPDATE_PATH;
        updateManager = new UpdateManager(this, updateUrl);
        updateManager.setListener(new UpdateManager.Listener() {
            @Override
            public void onNoUpdate() {
                runOnUiThread(() -> hideUpdateBar());
            }

            @Override
            public void onUpdateAvailable(JSONObject payload) {
                runOnUiThread(() -> showUpdateBar(payload));
            }

            @Override
            public void onError(String message) {
                runOnUiThread(() -> {
                    if (updatePayload == null) {
                        hideUpdateBar();
                        return;
                    }
                    Toast.makeText(ChatActivity.this, message, Toast.LENGTH_SHORT).show();
                });
            }
        });
        updateManager.checkForUpdate();
    }

    private void showUpdateBar(JSONObject payload) {
        updatePayload = payload;
        String versionName = payload.optString("versionName", "");
        int versionCode = payload.optInt("versionCode", 0);
        String title = getString(R.string.update_available);
        if (!TextUtils.isEmpty(versionName)) {
            title = title + " " + versionName;
        } else if (versionCode > 0) {
            title = title + " (v" + versionCode + ")";
        }
        String changelog = payload.optString("changelog", getString(R.string.update_tap));
        binding.updateTitle.setText(title);
        binding.updateSubtitle.setText(changelog);
        binding.updateBar.setVisibility(View.VISIBLE);
    }

    private void hideUpdateBar() {
        updatePayload = null;
        binding.updateBar.setVisibility(View.GONE);
        binding.btnUpdate.setEnabled(true);
        binding.btnUpdate.setText(R.string.update_button);
    }

    private void setupBottomNav() {
        if (binding.bottomNav == null) return;
        binding.drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED);
        BottomNavHelper.setup(this, binding.bottomNav, com.xipher.wrapper.R.id.nav_chats);
    }

    private void openDrawer() {
        binding.drawerLayout.openDrawer(GravityCompat.START);
    }

    private void closeDrawer() {
        binding.drawerLayout.closeDrawer(GravityCompat.START);
    }

    private void updateBlur(float slideOffset) {
        if (slideOffset <= 0f) {
            binding.blurOverlay.setVisibility(View.GONE);
        } else {
            binding.blurOverlay.setVisibility(View.VISIBLE);
        }
        binding.blurOverlay.setAlpha(Math.min(0.5f, slideOffset * 0.5f));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (slideOffset > 0f) {
                float radius = 18f * slideOffset;
                RenderEffect effect = RenderEffect.createBlurEffect(radius, radius, Shader.TileMode.CLAMP);
                binding.chatContent.setRenderEffect(effect);
            } else {
                binding.chatContent.setRenderEffect(null);
            }
        }
    }

    private void updateDrawerHeader(String username, String userId, String avatarUrl) {
        String safeName = !TextUtils.isEmpty(username) ? username : getString(R.string.profile_title);
        String letter = getString(R.string.avatar_placeholder);
        if (!TextUtils.isEmpty(safeName)) {
            letter = safeName.substring(0, 1).toUpperCase();
        }
        drawerUsername.setText(safeName);
        drawerSubtitle.setText(!TextUtils.isEmpty(userId)
                ? getString(R.string.drawer_profile_id, userId)
                : getString(R.string.drawer_my_profile));
        drawerAvatarLetter.setText(letter);
        if (!TextUtils.isEmpty(avatarUrl)) {
            String url = avatarUrl.startsWith("http") ? avatarUrl : (com.xipher.wrapper.BuildConfig.API_BASE + avatarUrl);
            drawerAvatarImage.setVisibility(View.VISIBLE);
            drawerAvatarLetter.setVisibility(View.GONE);
            Glide.with(drawerAvatarImage.getContext())
                    .load(url)
                    .circleCrop()
                    .into(drawerAvatarImage);
        } else {
            drawerAvatarImage.setVisibility(View.GONE);
            drawerAvatarLetter.setVisibility(View.VISIBLE);
        }
    }

    private void loadProfileHeader() {
        if (isPanicMode()) return;
        String token = authRepo.getToken();
        String userId = authRepo.getUserId();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(userId)) return;
        io.execute(() -> {
            try {
                com.xipher.wrapper.data.api.ApiClient client = new com.xipher.wrapper.data.api.ApiClient(token);
                com.google.gson.JsonObject res = client.getUserProfile(token, userId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                    com.google.gson.JsonObject user = res.getAsJsonObject("user");
                    String avatarUrl = user.has("avatar_url") ? user.get("avatar_url").getAsString() : null;
                    String displayName = user.has("display_name") ? user.get("display_name").getAsString() : authRepo.getUsername();
                    runOnUiThread(() -> updateDrawerHeader(displayName, userId, avatarUrl));
                }
            } catch (Exception ignored) {
            }
        });
    }

    private void openCreateChat(boolean isGroup) {
        Intent intent = new Intent(this, CreateChatActivity.class);
        intent.putExtra(CreateChatActivity.EXTRA_IS_GROUP, isGroup);
        startActivity(intent);
    }

    private void showCreateMenu() {
        String[] items = {getString(com.xipher.wrapper.R.string.create_group),
                getString(com.xipher.wrapper.R.string.create_channel)};
        new AlertDialog.Builder(this)
                .setTitle(getString(com.xipher.wrapper.R.string.create_title))
                .setItems(items, (dialog, which) -> {
                    if (which == 0) {
                        openCreateChat(true);
                    } else if (which == 1) {
                        openCreateChat(false);
                    }
                })
                .show();
    }

    private void openMyProfile() {
        Intent intent = new Intent(this, ProfileActivity.class);
        if (!TextUtils.isEmpty(authRepo.getUserId())) {
            intent.putExtra(ProfileActivity.EXTRA_USER_ID, authRepo.getUserId());
        }
        startActivity(intent);
    }

    private void openFavorites() {
        ChatEntity saved = null;
        for (ChatEntity chat : currentChats) {
            if (chat != null && chat.isSaved) {
                saved = chat;
                break;
            }
        }
        if (saved == null) {
            Toast.makeText(this, R.string.saved_not_found, Toast.LENGTH_SHORT).show();
            return;
        }
        openChatDetail(saved);
    }

    private void loadChatFolders() {
        chatFolders = ChatListPrefs.getFolders(this);
        activeFolderId = ChatListPrefs.getActiveFolder(this);
        if (!FOLDER_ALL_ID.equals(activeFolderId) && findFolderById(activeFolderId) == null) {
            activeFolderId = FOLDER_ALL_ID;
            ChatListPrefs.setActiveFolder(this, activeFolderId);
        }
    }

    private void saveChatFolders() {
        ChatListPrefs.setFolders(this, chatFolders);
    }

    private void setupFolderBar() {
        if (binding.folderChipContainer == null || binding.btnFolders == null) {
            if (!FOLDER_ALL_ID.equals(activeFolderId)) {
                activeFolderId = FOLDER_ALL_ID;
                ChatListPrefs.setActiveFolder(this, activeFolderId);
            }
            return;
        }
        binding.btnFolders.setOnClickListener(v -> showFolderManager());
        renderFolderChips();
    }

    private void renderFolderChips() {
        if (binding.folderChipContainer == null) return;
        binding.folderChipContainer.removeAllViews();
        addFolderChip(FOLDER_ALL_ID, getString(R.string.folder_all));
        for (ChatFolder folder : chatFolders) {
            if (folder == null) continue;
            addFolderChip(folder.id, folder.name);
        }
    }

    private void addFolderChip(String id, String label) {
        TextView chip = (TextView) getLayoutInflater().inflate(R.layout.item_folder_chip, binding.folderChipContainer, false);
        chip.setText(label);
        chip.setSelected(id != null && id.equals(activeFolderId));
        chip.setOnClickListener(v -> setActiveFolder(id));
        binding.folderChipContainer.addView(chip);
    }

    private void setActiveFolder(String folderId) {
        String next = folderId == null || folderId.isEmpty() ? FOLDER_ALL_ID : folderId;
        if (!FOLDER_ALL_ID.equals(next) && findFolderById(next) == null) {
            next = FOLDER_ALL_ID;
        }
        activeFolderId = next;
        ChatListPrefs.setActiveFolder(this, activeFolderId);
        renderFolderChips();
        filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
    }

    private ChatFolder findFolderById(String id) {
        if (id == null) return null;
        for (ChatFolder folder : chatFolders) {
            if (folder != null && id.equals(folder.id)) {
                return folder;
            }
        }
        return null;
    }

    private void showFolderManager() {
        BottomSheetDialog sheet = new BottomSheetDialog(this);
        View view = getLayoutInflater().inflate(R.layout.bottom_sheet_folders, null);
        LinearLayout list = view.findViewById(R.id.folderSheetList);
        TextView empty = view.findViewById(R.id.folderSheetEmpty);
        View createBtn = view.findViewById(R.id.btnCreateFolder);
        createBtn.setOnClickListener(v -> {
            sheet.dismiss();
            showFolderEditor(null, null);
        });
        renderFolderSheetList(list, empty, sheet);
        sheet.setContentView(view);
        sheet.show();
    }

    private void renderFolderSheetList(LinearLayout container, TextView emptyView, BottomSheetDialog sheet) {
        container.removeAllViews();
        if (chatFolders.isEmpty()) {
            if (emptyView != null) {
                emptyView.setVisibility(View.VISIBLE);
            }
            return;
        }
        if (emptyView != null) {
            emptyView.setVisibility(View.GONE);
        }
        for (ChatFolder folder : chatFolders) {
            if (folder == null) continue;
            View row = getLayoutInflater().inflate(R.layout.item_folder_row, container, false);
            TextView name = row.findViewById(R.id.folderRowName);
            TextView count = row.findViewById(R.id.folderRowCount);
            ImageView icon = row.findViewById(R.id.folderRowIcon);
            View edit = row.findViewById(R.id.folderRowEdit);
            View delete = row.findViewById(R.id.folderRowDelete);

            name.setText(folder.name != null ? folder.name : getString(R.string.folder_manage));
            int total = countChatsInFolder(folder);
            count.setText(getResources().getQuantityString(R.plurals.folder_chat_count, total, total));

            boolean isActive = folder.id != null && folder.id.equals(activeFolderId);
            if (icon != null) {
                int color = isActive ? R.color.x_accent_start : R.color.x_text_secondary;
                icon.setColorFilter(ContextCompat.getColor(this, color));
            }

            row.setOnClickListener(v -> {
                setActiveFolder(folder.id);
                sheet.dismiss();
            });
            edit.setOnClickListener(v -> {
                sheet.dismiss();
                showFolderEditor(folder, null);
            });
            delete.setOnClickListener(v -> {
                String message = getString(R.string.folder_delete_confirm, folder.name);
                new AlertDialog.Builder(this)
                        .setTitle(R.string.folder_delete)
                        .setMessage(message)
                        .setNegativeButton(R.string.cancel, null)
                        .setPositiveButton(R.string.delete_action, (d, w) -> deleteFolder(folder))
                        .show();
            });

            container.addView(row);
        }
    }

    private void deleteFolder(ChatFolder folder) {
        if (folder == null) return;
        chatFolders.remove(folder);
        if (folder.id != null && folder.id.equals(activeFolderId)) {
            activeFolderId = FOLDER_ALL_ID;
            ChatListPrefs.setActiveFolder(this, activeFolderId);
        }
        persistFolderChanges();
    }

    private void showFolderEditor(ChatFolder existing, ChatEntity preselectChat) {
        BottomSheetDialog sheet = new BottomSheetDialog(this);
        View view = getLayoutInflater().inflate(R.layout.bottom_sheet_folder_editor, null);
        TextView title = view.findViewById(R.id.folderEditorTitle);
        EditText nameInput = view.findViewById(R.id.folderNameInput);
        EditText searchInput = view.findViewById(R.id.folderSearchInput);
        LinearLayout list = view.findViewById(R.id.folderChatList);
        View cancelBtn = view.findViewById(R.id.btnCancelFolder);
        View saveBtn = view.findViewById(R.id.btnSaveFolder);

        if (title != null) {
            title.setText(existing == null ? R.string.folder_create : R.string.folder_edit);
        }
        if (nameInput != null) {
            nameInput.setText(existing != null ? existing.name : "");
        }

        List<ChatEntity> available = getFolderSelectableChats();
        Set<String> selected = new HashSet<>();
        if (existing != null && existing.chatKeys != null) {
            selected.addAll(existing.chatKeys);
        }
        if (preselectChat != null) {
            String key = ChatListPrefs.buildKey(preselectChat);
            if (!key.isEmpty()) {
                selected.add(key);
            }
        }

        String[] query = new String[]{""};
        renderFolderChatList(list, available, selected, query[0]);
        if (searchInput != null) {
            searchInput.addTextChangedListener(new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {
                    query[0] = s != null ? s.toString() : "";
                    renderFolderChatList(list, available, selected, query[0]);
                }

                @Override
                public void afterTextChanged(Editable s) {}
            });
        }

        cancelBtn.setOnClickListener(v -> sheet.dismiss());
        saveBtn.setOnClickListener(v -> {
            String name = nameInput != null && nameInput.getText() != null
                    ? nameInput.getText().toString().trim()
                    : "";
            if (name.isEmpty()) {
                Toast.makeText(this, R.string.folder_name_hint, Toast.LENGTH_SHORT).show();
                return;
            }
            if (existing != null) {
                existing.name = name;
                existing.chatKeys = new ArrayList<>(selected);
            } else {
                ChatFolder folder = new ChatFolder(createFolderId(), name, new ArrayList<>(selected));
                chatFolders.add(folder);
                activeFolderId = folder.id;
                ChatListPrefs.setActiveFolder(this, activeFolderId);
            }
            persistFolderChanges();
            sheet.dismiss();
        });

        sheet.setContentView(view);
        sheet.show();
    }

    private void renderFolderChatList(LinearLayout container,
                                      List<ChatEntity> available,
                                      Set<String> selected,
                                      String query) {
        container.removeAllViews();
        String q = query != null ? query.trim().toLowerCase(Locale.getDefault()) : "";
        boolean hasAny = false;
        for (ChatEntity chat : available) {
            if (chat == null) continue;
            String title = chat.title != null ? chat.title.toLowerCase(Locale.getDefault()) : "";
            String username = chat.username != null ? chat.username.toLowerCase(Locale.getDefault()) : "";
            if (!q.isEmpty() && !title.contains(q) && !username.contains(q)) {
                continue;
            }
            String key = ChatListPrefs.buildKey(chat);
            if (key.isEmpty()) continue;
            View row = getLayoutInflater().inflate(R.layout.item_folder_chat_select, container, false);
            TextView name = row.findViewById(R.id.folderChatTitle);
            TextView type = row.findViewById(R.id.folderChatType);
            android.widget.CheckBox check = row.findViewById(R.id.folderChatCheck);
            name.setText(chat.title != null ? chat.title : getString(R.string.chat_title_unknown));
            type.setText(getChatTypeLabel(chat));
            check.setChecked(selected.contains(key));
            check.setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (isChecked) {
                    selected.add(key);
                } else {
                    selected.remove(key);
                }
            });
            row.setOnClickListener(v -> check.setChecked(!check.isChecked()));
            container.addView(row);
            hasAny = true;
        }
        if (!hasAny) {
            TextView empty = new TextView(this);
            empty.setText(R.string.search_not_found);
            empty.setTextColor(ContextCompat.getColor(this, R.color.x_text_secondary));
            empty.setPadding(dp(4), dp(6), dp(4), dp(6));
            container.addView(empty);
        }
    }

    private int countChatsInFolder(ChatFolder folder) {
        if (folder == null || folder.chatKeys == null) return 0;
        Set<String> keys = new HashSet<>(folder.chatKeys);
        Set<String> hidden = ChatListPrefs.getHidden(this);
        int count = 0;
        for (ChatEntity chat : currentChats) {
            if (chat == null) continue;
            String key = ChatListPrefs.buildKey(chat);
            if (key.isEmpty()) continue;
            if (hidden.contains(key)) continue;
            if (keys.contains(key)) {
                count++;
            }
        }
        return count;
    }

    private void persistFolderChanges() {
        saveChatFolders();
        ChatListPrefs.setFoldersSynced(this, false);
        renderFolderChips();
        filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
        pushChatFoldersToServer();
    }

    private void showAssignFolderDialog(ChatEntity chat) {
        if (chat == null) return;
        String key = ChatListPrefs.buildKey(chat);
        if (key.isEmpty()) return;

        if (chatFolders.isEmpty()) {
            showFolderEditor(null, chat);
            return;
        }

        String[] names = new String[chatFolders.size()];
        boolean[] checked = new boolean[chatFolders.size()];
        for (int i = 0; i < chatFolders.size(); i++) {
            ChatFolder folder = chatFolders.get(i);
            names[i] = folder != null && folder.name != null ? folder.name : getString(R.string.folder_manage);
            if (folder != null && folder.chatKeys != null) {
                checked[i] = folder.chatKeys.contains(key);
            }
        }
        String title = getString(R.string.folder_assign_title,
                chat.title != null ? chat.title : getString(R.string.chat_title_unknown));
        new AlertDialog.Builder(this)
                .setTitle(title)
                .setMultiChoiceItems(names, checked, (dialog, which, isChecked) -> checked[which] = isChecked)
                .setNegativeButton(R.string.cancel, null)
                .setNeutralButton(R.string.folder_create, (dialog, which) -> showFolderEditor(null, chat))
                .setPositiveButton(R.string.save, (dialog, which) -> {
                    for (int i = 0; i < chatFolders.size(); i++) {
                        ChatFolder folder = chatFolders.get(i);
                        if (folder == null) continue;
                        Set<String> updated = new HashSet<>(folder.chatKeys != null ? folder.chatKeys : new ArrayList<>());
                        if (checked[i]) {
                            updated.add(key);
                        } else {
                            updated.remove(key);
                        }
                        folder.chatKeys = new ArrayList<>(updated);
                    }
                    persistFolderChanges();
                })
                .show();
    }

    private void removeChatFromFolders(String key) {
        if (key == null || key.isEmpty()) return;
        boolean changed = false;
        for (ChatFolder folder : chatFolders) {
            if (folder == null || folder.chatKeys == null) continue;
            if (folder.chatKeys.remove(key)) {
                changed = true;
            }
        }
        if (changed) {
            persistFolderChanges();
        }
    }

    private List<ChatFolder> cloneFolders(List<ChatFolder> source) {
        List<ChatFolder> out = new ArrayList<>();
        if (source == null) return out;
        for (ChatFolder folder : source) {
            if (folder == null || TextUtils.isEmpty(folder.id) || TextUtils.isEmpty(folder.name)) continue;
            Set<String> unique = new HashSet<>();
            if (folder.chatKeys != null) {
                for (String key : folder.chatKeys) {
                    if (key != null && !key.isEmpty()) {
                        unique.add(key);
                    }
                }
            }
            out.add(new ChatFolder(folder.id, folder.name, new ArrayList<>(unique)));
        }
        return out;
    }

    private List<ChatEntity> getFolderSelectableChats() {
        Set<String> hidden = ChatListPrefs.getHidden(this);
        List<ChatEntity> result = new ArrayList<>();
        for (ChatEntity chat : currentChats) {
            if (chat == null) continue;
            String key = ChatListPrefs.buildKey(chat);
            if (key.isEmpty() || hidden.contains(key)) continue;
            result.add(chat);
        }
        return result;
    }

    private String getChatTypeLabel(ChatEntity chat) {
        if (chat == null) return "";
        if (chat.isSaved) {
            return getString(R.string.chat_type_saved);
        }
        if (chat.isGroup) {
            return getString(R.string.chat_type_group);
        }
        if (chat.isChannel) {
            return getString(R.string.chat_type_channel);
        }
        if (!TextUtils.isEmpty(chat.username)) {
            return "@" + chat.username;
        }
        return getString(R.string.chat_type_private);
    }

    private String formatChatLabel(ChatEntity chat) {
        String title = chat.title != null ? chat.title : getString(R.string.chat_title_unknown);
        String type = null;
        if (chat.isSaved) {
            type = getString(R.string.chat_type_saved);
        } else if (chat.isGroup) {
            type = getString(R.string.chat_type_group);
        } else if (chat.isChannel) {
            type = getString(R.string.chat_type_channel);
        }
        if (type == null || type.isEmpty()) {
            return title;
        }
        return title + " · " + type;
    }

    private void pushChatFoldersToServer() {
        if (isPanicMode()) return;
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;
        List<ChatFolder> snapshot = cloneFolders(chatFolders);
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                BasicResponse res = client.setChatFolders(token, snapshot);
                if (res != null && res.success) {
                    ChatListPrefs.setFoldersSynced(this, true);
                    lastFolderSyncAt = System.currentTimeMillis();
                }
            } catch (Exception ignored) {
            }
        });
    }

    private void syncChatFolders(boolean force) {
        if (isPanicMode()) return;
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;
        long now = System.currentTimeMillis();
        if (folderSyncInFlight) return;
        if (!force && now - lastFolderSyncAt < FOLDERS_SYNC_INTERVAL_MS) return;
        folderSyncInFlight = true;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean synced = ChatListPrefs.isFoldersSynced(this);
                boolean hasLocal = ChatListPrefs.hasStoredFolders(this);
                if (!synced && hasLocal) {
                    List<ChatFolder> local = cloneFolders(chatFolders);
                    BasicResponse res = client.setChatFolders(token, local);
                    if (res != null && res.success) {
                        ChatListPrefs.setFoldersSynced(this, true);
                    }
                    return;
                }
                List<ChatFolder> remote = cloneFolders(client.chatFolders(token));
                chatFolders = new ArrayList<>(remote);
                saveChatFolders();
                ChatListPrefs.setFoldersSynced(this, true);
                runOnUiThread(() -> {
                    if (!FOLDER_ALL_ID.equals(activeFolderId) && findFolderById(activeFolderId) == null) {
                        activeFolderId = FOLDER_ALL_ID;
                        ChatListPrefs.setActiveFolder(this, activeFolderId);
                    }
                    renderFolderChips();
                    filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
                });
            } catch (Exception ignored) {
                // keep local state
            } finally {
                lastFolderSyncAt = System.currentTimeMillis();
                folderSyncInFlight = false;
            }
        });
    }

    private String createFolderId() {
        return UUID.randomUUID().toString();
    }

    private int dp(int value) {
        return Math.round(getResources().getDisplayMetrics().density * value);
    }

    private void showChatActions(ChatEntity chat) {
        if (chat == null) return;
        if (isSearchResult(chat)) return;
        String key = ChatListPrefs.buildKey(chat);
        boolean pinned = ChatListPrefs.isPinned(this, key);
        boolean muted = ChatListPrefs.isMuted(this, key);

        List<String> labels = new ArrayList<>();
        List<Runnable> actions = new ArrayList<>();
        labels.add(getString(pinned ? R.string.chat_unpin : R.string.chat_pin));
        actions.add(() -> togglePinned(chat));

        labels.add(getString(muted ? R.string.chat_unmute : R.string.chat_mute));
        actions.add(() -> toggleMuted(chat));

        labels.add(getString(R.string.folder_assign));
        actions.add(() -> showAssignFolderDialog(chat));

        boolean canDelete = !chat.isGroup && !chat.isChannel && !chat.isSaved;
        int deleteIndex = -1;
        if (canDelete) {
            labels.add(getString(R.string.chat_delete));
            actions.add(() -> showDeleteChatDialog(chat));
            deleteIndex = labels.size() - 1;
        }

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(chat.title != null ? chat.title : getString(R.string.chat_title_unknown))
                .setItems(labels.toArray(new String[0]), (d, which) -> actions.get(which).run())
                .create();

        int finalDeleteIndex = deleteIndex;
        dialog.setOnShowListener(d -> {
            if (finalDeleteIndex >= 0) {
                ListView listView = dialog.getListView();
                if (listView != null) {
                    listView.post(() -> {
                        View item = listView.getChildAt(finalDeleteIndex);
                        if (item instanceof TextView) {
                            ((TextView) item).setTextColor(
                                    ContextCompat.getColor(this, R.color.x_call_reject));
                        }
                    });
                }
            }
        });
        dialog.show();
    }

    private void togglePinned(ChatEntity chat) {
        String key = ChatListPrefs.buildKey(chat);
        boolean pinned = ChatListPrefs.isPinned(this, key);
        boolean next = !pinned;
        ChatListPrefs.setPinned(this, key, next);
        filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
        syncPinnedUpdate(chat, key, next, pinned);
    }

    private void toggleMuted(ChatEntity chat) {
        String key = ChatListPrefs.buildKey(chat);
        boolean muted = ChatListPrefs.isMuted(this, key);
        ChatListPrefs.setMuted(this, key, !muted);
        filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
    }

    private void syncPinnedUpdate(ChatEntity chat, String key, boolean pinned, boolean previous) {
        if (isPanicMode()) return;
        if (chat == null || TextUtils.isEmpty(key)) return;
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) {
            ChatListPrefs.setPinned(this, key, previous);
            filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
            Toast.makeText(this, R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        String chatType = resolveChatType(chat);
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                BasicResponse res = pinned
                        ? client.pinChat(token, chat.id, chatType)
                        : client.unpinChat(token, chat.id, chatType);
                boolean ok = res != null && res.success;
                if (!ok) {
                    runOnUiThread(() -> {
                        ChatListPrefs.setPinned(this, key, previous);
                        filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
                        Toast.makeText(this, R.string.chat_pin_failed, Toast.LENGTH_SHORT).show();
                    });
                } else {
                    ChatListPrefs.setPinnedSynced(this, true);
                    lastPinSyncAt = System.currentTimeMillis();
                }
            } catch (Exception e) {
                runOnUiThread(() -> {
                    ChatListPrefs.setPinned(this, key, previous);
                    filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
                    Toast.makeText(this, R.string.chat_pin_failed, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void syncPinnedChats(boolean force) {
        if (isPanicMode()) return;
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;
        long now = System.currentTimeMillis();
        if (pinSyncInFlight) return;
        if (!force && now - lastPinSyncAt < PINNED_SYNC_INTERVAL_MS) return;
        pinSyncInFlight = true;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                List<ChatPinDto> pins = client.chatPins(token);
                Set<String> remote = new HashSet<>();
                if (pins != null) {
                    for (ChatPinDto pin : pins) {
                        if (pin == null) continue;
                        String key = ChatListPrefs.buildKey(pin.chat_id, pin.chat_type);
                        if (!TextUtils.isEmpty(key)) {
                            remote.add(key);
                        }
                    }
                }
                boolean synced = ChatListPrefs.isPinnedSynced(this);
                Set<String> local = ChatListPrefs.getPinned(this);
                if (!synced && remote.isEmpty() && local != null && !local.isEmpty()) {
                    pushLocalPinsToServer(client, token, local);
                    remote = new HashSet<>(local);
                }
                ChatListPrefs.setPinnedSet(this, remote);
                ChatListPrefs.setPinnedSynced(this, true);
                runOnUiThread(() -> filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : ""));
            } catch (Exception e) {
                // keep local state
            } finally {
                lastPinSyncAt = System.currentTimeMillis();
                pinSyncInFlight = false;
            }
        });
    }

    private void pushLocalPinsToServer(ApiClient client, String token, Set<String> keys) {
        if (client == null || TextUtils.isEmpty(token) || keys == null || keys.isEmpty()) return;
        for (String key : keys) {
            String[] parts = splitChatKey(key);
            if (parts == null) continue;
            try {
                client.pinChat(token, parts[1], parts[0]);
            } catch (Exception ignored) {
            }
        }
    }

    private String[] splitChatKey(String key) {
        if (TextUtils.isEmpty(key)) return null;
        int idx = key.indexOf(':');
        if (idx <= 0 || idx >= key.length() - 1) return null;
        String type = key.substring(0, idx);
        String id = key.substring(idx + 1);
        if (TextUtils.isEmpty(type) || TextUtils.isEmpty(id)) return null;
        return new String[]{type, id};
    }

    private String resolveChatType(ChatEntity chat) {
        if (chat.isGroup) return "group";
        if (chat.isChannel) return "channel";
        if (chat.isSaved) return "saved";
        return "chat";
    }

    private void showDeleteChatDialog(ChatEntity chat) {
        String title = chat.title != null ? chat.title : getString(R.string.chat_title_unknown);
        String message = getString(R.string.chat_delete_confirm, title);
        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(R.string.chat_delete_title)
                .setMessage(message)
                .setNegativeButton(R.string.cancel, null)
                .setNeutralButton(R.string.chat_delete_for_me, (d, which) -> deleteChatForMe(chat))
                .setPositiveButton(R.string.chat_delete_for_all, (d, which) -> deleteChatForAll(chat))
                .create();
        dialog.setOnShowListener(d -> {
            if (dialog.getButton(AlertDialog.BUTTON_POSITIVE) != null) {
                dialog.getButton(AlertDialog.BUTTON_POSITIVE)
                        .setTextColor(ContextCompat.getColor(this, R.color.x_call_reject));
            }
        });
        dialog.show();
    }

    private void deleteChatForMe(ChatEntity chat) {
        applyLocalChatDeletion(chat, R.string.chat_deleted_for_me);
    }

    private void deleteChatForAll(ChatEntity chat) {
        if (isPanicMode()) {
            runOnUiThread(() -> applyLocalChatDeletion(chat, R.string.chat_deleted_for_me));
            return;
        }
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) {
            Toast.makeText(this, R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        String chatType = chat.isGroup ? "group" : chat.isChannel ? "channel" : "chat";
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.deleteChat(token, chat.id, chatType);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                if (ok) {
                    runOnUiThread(() -> applyLocalChatDeletion(chat, R.string.chat_deleted_for_all));
                } else {
                    runOnUiThread(() -> Toast.makeText(this, R.string.chat_delete_failed, Toast.LENGTH_SHORT).show());
                }
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.chat_delete_failed, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void applyLocalChatDeletion(ChatEntity chat, int toastResId) {
        String key = ChatListPrefs.buildKey(chat);
        if (key.isEmpty()) return;
        ChatListPrefs.setHidden(this, key, true);
        ChatListPrefs.setPinned(this, key, false);
        ChatListPrefs.setMuted(this, key, false);
        removeChatFromFolders(key);
        if (db != null) {
            io.execute(() -> db.messageDao().clearChat(chat.id));
        }
        Toast.makeText(this, toastResId, Toast.LENGTH_SHORT).show();
        filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : "");
    }

    private void filterChats(String query) {
        String raw = query != null ? query.trim() : "";
        String q = normalizeSearchQuery(raw);
        List<ChatEntity> base = new ArrayList<>();
        List<ChatEntity> filtered = new ArrayList<>();
        List<ChatEntity> pinnedFirst = new ArrayList<>();
        Set<String> hidden = ChatListPrefs.getHidden(this);
        Set<String> pinned = ChatListPrefs.getPinned(this);
        Set<String> muted = ChatListPrefs.getMuted(this);
        if (!FOLDER_ALL_ID.equals(activeFolderId) && findFolderById(activeFolderId) == null) {
            activeFolderId = FOLDER_ALL_ID;
            ChatListPrefs.setActiveFolder(this, activeFolderId);
        }
        ChatFolder activeFolder = FOLDER_ALL_ID.equals(activeFolderId) ? null : findFolderById(activeFolderId);
        Set<String> folderKeys = null;
        if (activeFolder != null && activeFolder.chatKeys != null) {
            folderKeys = new HashSet<>(activeFolder.chatKeys);
        }
        renderFolderChips();

        List<ChatEntity> source = currentChats != null ? currentChats : new ArrayList<>();
        for (ChatEntity chat : source) {
            if (chat == null) continue;
            String key = ChatListPrefs.buildKey(chat);
            if (key.isEmpty()) continue;
            if (hidden.contains(key)) continue;
            if (folderKeys != null && !folderKeys.contains(key)) continue;
            base.add(chat);
        }

        if (q.isEmpty()) {
            filtered.addAll(base);
        } else {
            for (ChatEntity chat : base) {
                if (matchesQuery(chat, q)) {
                    filtered.add(chat);
                }
            }
        }

        for (ChatEntity chat : filtered) {
            String key = ChatListPrefs.buildKey(chat);
            if (pinned.contains(key)) {
                pinnedFirst.add(chat);
            }
        }
        // Add unread chats first (after pinned)
        for (ChatEntity chat : filtered) {
            String key = ChatListPrefs.buildKey(chat);
            if (!pinned.contains(key) && chat.unread > 0) {
                pinnedFirst.add(chat);
            }
        }
        // Then add read chats
        for (ChatEntity chat : filtered) {
            String key = ChatListPrefs.buildKey(chat);
            if (!pinned.contains(key) && chat.unread == 0) {
                pinnedFirst.add(chat);
            }
        }

        adapter.setPinnedIds(pinned);
        adapter.setMutedIds(muted);
        List<ChatEntity> display = new ArrayList<>(pinnedFirst);
        if (!q.isEmpty()) {
            scheduleRemoteSearch(raw, false);
            appendRemoteResults(display);
        } else {
            clearRemoteResults();
        }
        adapter.submitChatList(display);
    }

    private void performUserSearch() {
        if (isPanicMode()) return;
        String raw = binding.etSearch.getText() != null ? binding.etSearch.getText().toString().trim() : "";
        if (raw.isEmpty()) return;
        scheduleRemoteSearch(raw, true);
    }

    private boolean tryUserSearch(ApiClient client, String username) throws Exception {
        if (isPanicMode()) return false;
        JsonObject res = client.findUser(authRepo.getToken(), username);
        boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
        if (ok && res.has("user_id")) {
            String userId = res.get("user_id").getAsString();
            String foundUsername = res.has("username") ? res.get("username").getAsString() : username;
            runOnUiThread(() -> {
                Intent intent = new Intent(this, ProfileActivity.class);
                intent.putExtra(ProfileActivity.EXTRA_USER_ID, userId);
                intent.putExtra(ProfileActivity.EXTRA_USERNAME, foundUsername);
                startActivity(intent);
            });
            return true;
        }
        return false;
    }

    private boolean tryChannelSearch(ApiClient client, String username) throws Exception {
        if (isPanicMode()) return false;
        JsonObject res = client.searchChannel(authRepo.getToken(), username);
        boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
        if (!ok || !res.has("channel")) {
            return false;
        }
        JsonObject channel = res.getAsJsonObject("channel");
        String channelId = channel.has("id") ? channel.get("id").getAsString() : null;
        if (channelId == null) return false;
        String name = channel.has("name") ? channel.get("name").getAsString() : username;
        boolean isPrivate = channel.has("is_private") && channel.get("is_private").getAsBoolean();
        runOnUiThread(() -> showChannelSearchDialog(channelId, name, isPrivate));
        return true;
    }

    private void showChannelSearchDialog(String channelId, String name, boolean isPrivate) {
        if (channelId == null) return;
        if (isChannelInList(channelId)) {
            openChannelDetail(channelId, name);
            return;
        }
        String message = isPrivate ? getString(R.string.channel_private_hint) : getString(R.string.channel_public_hint);
        String action = isPrivate ? getString(R.string.channel_request_access) : getString(R.string.channel_subscribe);
        new AlertDialog.Builder(this)
                .setTitle(name)
                .setMessage(message)
                .setPositiveButton(action, (dialog, which) -> subscribeToChannel(channelId, name, isPrivate))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private boolean isChannelInList(String channelId) {
        for (ChatEntity chat : currentChats) {
            if (chat != null && channelId.equals(chat.id)) {
                return true;
            }
        }
        return false;
    }

    private void subscribeToChannel(String channelId, String name, boolean isPrivate) {
        if (isPanicMode()) return;
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.subscribeChannel(token, channelId);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                String msg = res != null && res.has("message") ? res.get("message").getAsString() : null;
                runOnUiThread(() -> {
                    if (ok) {
                        String toast = msg != null && !msg.isEmpty()
                                ? msg
                                : getString(isPrivate ? R.string.channel_join_request_sent : R.string.channel_subscribed);
                        Toast.makeText(this, toast, Toast.LENGTH_SHORT).show();
                        if (!isPrivate) {
                            vm.refresh();
                            openChannelDetail(channelId, name);
                        }
                    } else {
                        Toast.makeText(this, R.string.channel_subscribe_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.channel_subscribe_failed, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void openChannelDetail(String channelId, String name) {
        Intent detail = new Intent(this, ChatDetailActivity.class);
        detail.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, channelId);
        detail.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, name);
        detail.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, false);
        detail.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, true);
        detail.putExtra(ChatDetailActivity.EXTRA_IS_SAVED, false);
        startActivity(detail);
    }

    private void scheduleRemoteSearch(String raw, boolean immediate) {
        if (isPanicMode()) {
            clearRemoteResults();
            return;
        }
        String query = normalizeSearchQuery(raw);
        if (query.length() < REMOTE_SEARCH_MIN) {
            clearRemoteResults();
            return;
        }
        if (!query.equals(lastRemoteQuery)) {
            synchronized (remoteSearchResults) {
                remoteSearchResults.clear();
            }
        }
        lastRemoteQuery = query;
        if (remoteSearchTask != null) {
            remoteSearchTask.cancel(true);
            remoteSearchTask = null;
        }
        long delay = immediate ? 0L : REMOTE_SEARCH_DELAY_MS;
        remoteSearchTask = scheduler.schedule(() -> performRemoteSearch(query), delay, TimeUnit.MILLISECONDS);
    }

    private void performRemoteSearch(String query) {
        if (isPanicMode()) return;
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;
        List<ChatEntity> results = new ArrayList<>();
        try {
            ApiClient client = new ApiClient(token);
            String username = query.startsWith("@") ? query.substring(1) : query;

            JsonObject userRes = client.findUser(token, username);
            ChatEntity userItem = buildSearchUserItem(userRes);
            String userId = extractSearchId(userItem);
            if (userItem != null && !isChatInList(userId, userItem.username)) {
                results.add(userItem);
            }

            JsonObject channelRes = client.searchChannel(token, username);
            ChatEntity channelItem = buildSearchChannelItem(channelRes);
            String channelId = extractSearchId(channelItem);
            if (channelItem != null && !isChatInList(channelId, channelItem.title)) {
                results.add(channelItem);
            }
        } catch (Exception ignored) {
            // ignore search errors
        }
        synchronized (remoteSearchResults) {
            remoteSearchResults.clear();
            remoteSearchResults.addAll(results);
        }
        runOnUiThread(() -> filterChats(binding.etSearch.getText() != null ? binding.etSearch.getText().toString() : ""));
    }

    private ChatEntity buildSearchUserItem(JsonObject res) {
        if (res == null || !res.has("success") || !res.get("success").getAsBoolean()) return null;
        if (!res.has("user_id")) return null;
        String userId = res.get("user_id").getAsString();
        if (userId == null || userId.isEmpty()) return null;
        String username = res.has("username") ? res.get("username").getAsString() : null;
        if (username == null || username.isEmpty()) return null;
        String currentUserId = authRepo.getUserId();
        if (currentUserId != null && currentUserId.equals(userId)) return null;
        ChatEntity item = new ChatEntity();
        item.id = SEARCH_USER_PREFIX + userId;
        item.title = username;
        item.username = username;
        item.lastMessage = getString(R.string.search_result_user);
        return item;
    }

    private ChatEntity buildSearchChannelItem(JsonObject res) {
        if (res == null || !res.has("success") || !res.get("success").getAsBoolean()) return null;
        if (!res.has("channel")) return null;
        JsonObject channel = res.getAsJsonObject("channel");
        if (channel == null) return null;
        String channelId = channel.has("id") ? channel.get("id").getAsString() : null;
        if (channelId == null || channelId.isEmpty()) return null;
        String name = channel.has("name") ? channel.get("name").getAsString() : null;
        if (name == null || name.isEmpty()) return null;
        boolean isPrivate = channel.has("is_private") && channel.get("is_private").getAsBoolean();
        ChatEntity item = new ChatEntity();
        item.id = SEARCH_CHANNEL_PREFIX + channelId;
        item.title = name;
        item.username = name;
        item.isChannel = true;
        item.isPrivate = isPrivate;
        item.lastMessage = getString(R.string.search_result_channel);
        return item;
    }

    private void clearRemoteResults() {
        if (remoteSearchTask != null) {
            remoteSearchTask.cancel(true);
            remoteSearchTask = null;
        }
        if (!remoteSearchResults.isEmpty()) {
            synchronized (remoteSearchResults) {
                remoteSearchResults.clear();
            }
        }
        lastRemoteQuery = "";
    }

    private void appendRemoteResults(List<ChatEntity> display) {
        if (display == null) return;
        List<ChatEntity> results;
        synchronized (remoteSearchResults) {
            results = new ArrayList<>(remoteSearchResults);
        }
        if (results.isEmpty()) return;
        Set<String> existingIds = new HashSet<>();
        for (ChatEntity chat : display) {
            if (chat != null && chat.id != null) {
                existingIds.add(chat.id);
            }
        }
        for (ChatEntity chat : results) {
            if (chat == null || chat.id == null) continue;
            if (existingIds.contains(chat.id)) continue;
            display.add(chat);
        }
    }

    private String extractSearchId(ChatEntity chat) {
        if (chat == null || chat.id == null) return null;
        if (chat.id.startsWith(SEARCH_USER_PREFIX)) {
            return chat.id.substring(SEARCH_USER_PREFIX.length());
        }
        if (chat.id.startsWith(SEARCH_CHANNEL_PREFIX)) {
            return chat.id.substring(SEARCH_CHANNEL_PREFIX.length());
        }
        return chat.id;
    }

    private boolean isChatInList(String id, String username) {
        if (id == null && (username == null || username.isEmpty())) return false;
        for (ChatEntity chat : currentChats) {
            if (chat == null) continue;
            if (id != null && id.equals(chat.id)) return true;
            if (username != null && chat.username != null
                    && username.equalsIgnoreCase(chat.username)) return true;
            if (username != null && chat.title != null
                    && username.equalsIgnoreCase(chat.title)) return true;
        }
        return false;
    }

    private boolean isSearchResult(ChatEntity chat) {
        if (chat == null || chat.id == null) return false;
        return chat.id.startsWith(SEARCH_USER_PREFIX) || chat.id.startsWith(SEARCH_CHANNEL_PREFIX);
    }

    private boolean isPanicMode() {
        return SecurityContext.get().isPanicMode();
    }

    private void handleSearchResultClick(ChatEntity chat) {
        if (chat == null || chat.id == null) return;
        if (chat.id.startsWith(SEARCH_USER_PREFIX)) {
            String userId = chat.id.substring(SEARCH_USER_PREFIX.length());
            if (userId.isEmpty()) return;
            Intent intent = new Intent(this, ProfileActivity.class);
            intent.putExtra(ProfileActivity.EXTRA_USER_ID, userId);
            intent.putExtra(ProfileActivity.EXTRA_USERNAME, chat.title != null ? chat.title : "");
            startActivity(intent);
            return;
        }
        if (chat.id.startsWith(SEARCH_CHANNEL_PREFIX)) {
            String channelId = chat.id.substring(SEARCH_CHANNEL_PREFIX.length());
            if (channelId.isEmpty()) return;
            showChannelSearchDialog(channelId, chat.title != null ? chat.title : channelId, chat.isPrivate);
        }
    }

    private String normalizeSearchQuery(String raw) {
        if (raw == null) return "";
        String trimmed = raw.trim().toLowerCase(Locale.getDefault());
        if (trimmed.startsWith("@")) {
            trimmed = trimmed.substring(1);
        }
        return trimmed;
    }

    private boolean matchesQuery(ChatEntity chat, String q) {
        if (q == null || q.isEmpty()) return true;
        String title = chat.title != null ? chat.title.toLowerCase(Locale.getDefault()) : "";
        String last = chat.lastMessage != null ? chat.lastMessage.toLowerCase(Locale.getDefault()) : "";
        String username = chat.username != null ? chat.username.toLowerCase(Locale.getDefault()) : "";
        String usernameWithAt = !username.isEmpty() ? ("@" + username) : "";
        if (title.contains(q) || last.contains(q) || username.contains(q) || usernameWithAt.contains(q)) {
            return true;
        }
        if (q.length() < 3) return false;
        if (isSubsequence(q, title) || isSubsequence(q, username)) {
            return true;
        }
        int titleDist = levenshteinDistance(q, title);
        if (titleDist <= fuzzyThreshold(q, title)) return true;
        int userDist = levenshteinDistance(q, username);
        return userDist <= fuzzyThreshold(q, username);
    }

    private int fuzzyThreshold(String q, String s) {
        int len = Math.max(q.length(), s.length());
        if (len <= 4) return 1;
        if (len <= 7) return 2;
        return 3;
    }

    private boolean isSubsequence(String q, String s) {
        if (q == null || s == null || q.isEmpty() || s.isEmpty()) return false;
        int qi = 0;
        for (int i = 0; i < s.length() && qi < q.length(); i++) {
            if (q.charAt(qi) == s.charAt(i)) {
                qi++;
            }
        }
        return qi == q.length();
    }

    private int levenshteinDistance(String a, String b) {
        if (a == null || b == null) return Integer.MAX_VALUE;
        int n = a.length();
        int m = b.length();
        if (n == 0) return m;
        if (m == 0) return n;
        int[] prev = new int[m + 1];
        int[] curr = new int[m + 1];
        for (int j = 0; j <= m; j++) {
            prev[j] = j;
        }
        for (int i = 1; i <= n; i++) {
            curr[0] = i;
            char ca = a.charAt(i - 1);
            for (int j = 1; j <= m; j++) {
                int cost = ca == b.charAt(j - 1) ? 0 : 1;
                int del = prev[j] + 1;
                int ins = curr[j - 1] + 1;
                int sub = prev[j - 1] + cost;
                curr[j] = Math.min(del, Math.min(ins, sub));
            }
            int[] tmp = prev;
            prev = curr;
            curr = tmp;
        }
        return prev[m];
    }
}
