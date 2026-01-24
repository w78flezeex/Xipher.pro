package com.xipher.wrapper.ui;

import android.content.ContentResolver;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.os.Bundle;
import android.util.Base64;
import android.widget.ImageView;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;

import android.view.View;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import com.bumptech.glide.Glide;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityProfileBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.util.BusinessHoursHelper;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.Calendar;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ProfileActivity extends AppCompatActivity {
    public static final String EXTRA_USER_ID = "user_id";
    public static final String EXTRA_USERNAME = "username";
    private static final String PREF_MUTED_CHATS = "xipher_muted_chats";
    private static final String KEY_MUTED_SET = "muted_chat_ids";

    private ActivityProfileBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private ActivityResultLauncher<String[]> pickAvatar;
    private String profileUserId;
    private String profileUsername;
    private String avatarUrl;
    private boolean isSelf;
    private boolean isMuted;
    private boolean isPremiumUser;
    private String premiumPlan;
    private String premiumExpiresAt;
    private Map<String, BusinessHoursHelper.Entry> businessHours;
    private String personalChannelId;
    private String personalChannelName;
    private String personalChannelLink;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityProfileBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);
        profileUserId = getIntent() != null ? getIntent().getStringExtra(EXTRA_USER_ID) : null;
        profileUsername = getIntent() != null ? getIntent().getStringExtra(EXTRA_USERNAME) : null;
        if (profileUserId == null || profileUserId.isEmpty()) {
            profileUserId = authRepo.getUserId();
        }

        // Toolbar setup
        binding.toolbar.setNavigationOnClickListener(v -> finish());
        
        binding.profileAvatar.setOnClickListener(v -> openAvatarPreview());
        binding.btnEditAvatar.setOnClickListener(v -> pickAvatar.launch(new String[]{"image/*"}));
        binding.btnSaveProfile.setOnClickListener(v -> saveProfile());
        binding.btnMessage.setOnClickListener(v -> openChat());
        binding.btnOpenPersonalChannel.setOnClickListener(v -> openPersonalChannel());
        
        // Profile action buttons (for other users)
        binding.btnCall.setOnClickListener(v -> onCallClicked());
        binding.btnMute.setOnClickListener(v -> onMuteClicked());
        binding.btnMore.setOnClickListener(v -> onMoreClicked(v));
        
        // Copy username on click
        binding.usernameRow.setOnClickListener(v -> {
            if (profileUsername != null && !profileUsername.isEmpty()) {
                copyToClipboard("@" + profileUsername);
                Toast.makeText(this, R.string.copied, Toast.LENGTH_SHORT).show();
            }
        });

        pickAvatar = registerForActivityResult(new ActivityResultContracts.OpenDocument(), this::handlePickedAvatar);

        loadProfile();
        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_profile);
    }

    private void loadProfile() {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty() || profileUserId == null) return;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getUserProfile(token, profileUserId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                    JsonObject user = res.getAsJsonObject("user");
                    String displayName = user.has("display_name") ? user.get("display_name").getAsString() : "";
                    String username = user.has("username") ? user.get("username").getAsString() : "";
                    String firstName = user.has("first_name") ? user.get("first_name").getAsString() : "";
                    String lastName = user.has("last_name") ? user.get("last_name").getAsString() : "";
                    String bio = user.has("bio") ? user.get("bio").getAsString() : "";
                    String aUrl = user.has("avatar_url") ? user.get("avatar_url").getAsString() : null;
                    boolean self = user.has("is_self") && user.get("is_self").getAsBoolean();
                    boolean isPremium = user.has("is_premium") && user.get("is_premium").getAsBoolean();
                    String premiumPlan = user.has("premium_plan") ? user.get("premium_plan").getAsString() : "";
                    String premiumExpiresAt = user.has("premium_expires_at") ? user.get("premium_expires_at").getAsString() : "";
                    JsonObject businessRaw = user.has("business_hours") && user.get("business_hours").isJsonObject()
                            ? user.getAsJsonObject("business_hours") : null;
                    JsonObject personal = user.has("personal_channel") ? user.getAsJsonObject("personal_channel") : null;
                    String channelId = personal != null && personal.has("id") ? personal.get("id").getAsString() : null;
                    String channelName = personal != null && personal.has("name") ? personal.get("name").getAsString() : "";
                    String channelLink = personal != null && personal.has("custom_link") ? personal.get("custom_link").getAsString() : "";
                    boolean isOnline = getBoolean(user, "is_online");
                    if (!isOnline) {
                        isOnline = getBoolean(user, "online");
                    }
                    String lastSeen = getStringOrNull(user, "last_seen");
                    if (lastSeen == null) {
                        lastSeen = getStringOrNull(user, "lastSeen");
                    }
                    final boolean finalIsOnline = isOnline;
                    final String finalLastSeen = lastSeen;
                    
                    // Save profile data for Settings screen if viewing own profile
                    if (self) {
                        String phone = user.has("phone") ? user.get("phone").getAsString() : null;
                        authRepo.saveProfile(firstName, lastName, phone, aUrl);
                    }
                    
                    runOnUiThread(() -> bindProfile(displayName, username, firstName, lastName, bio, aUrl, self,
                            channelId, channelName, channelLink, isPremium, premiumPlan, premiumExpiresAt, finalIsOnline, finalLastSeen, businessRaw));
                }
            } catch (Exception ignored) {
            }
        });
    }

    private void bindProfile(String displayName, String username, String firstName, String lastName, String bio,
                             String avatarUrl, boolean self, String channelId, String channelName, String channelLink,
                             boolean isPremium, String premiumPlan, String premiumExpiresAt,
                             boolean isOnline, String lastSeen, JsonObject businessRaw) {
        this.isSelf = self;
        this.isPremiumUser = isPremium;
        this.premiumPlan = premiumPlan != null ? premiumPlan : "";
        this.premiumExpiresAt = premiumExpiresAt != null ? premiumExpiresAt : "";
        this.avatarUrl = avatarUrl != null && !avatarUrl.isEmpty()
                ? (avatarUrl.startsWith("http") ? avatarUrl : BuildConfig.API_BASE + avatarUrl)
                : null;
        this.personalChannelId = channelId;
        this.personalChannelName = channelName;
        this.personalChannelLink = channelLink;
        this.profileUsername = username;
        String safeName = displayName != null && !displayName.isEmpty()
                ? displayName
                : (username != null ? username : getString(R.string.profile_title));

        // Header info
        binding.profileName.setText(safeName);
        binding.profileUsername.setText(username != null && !username.isEmpty() ? "@" + username : "");
        
        updateAvatar(safeName, this.avatarUrl);
        bindPremiumBadge();
        bindStatus(isOnline, lastSeen);

        // Self vs other user visibility
        binding.btnEditAvatar.setVisibility(self ? View.VISIBLE : View.GONE);
        binding.btnSaveProfile.setVisibility(self ? View.VISIBLE : View.GONE);
        binding.profileActionsRow.setVisibility(self ? View.GONE : View.VISIBLE);
        binding.editSection.setVisibility(self ? View.VISIBLE : View.GONE);
        binding.infoSection.setVisibility(self ? View.GONE : View.VISIBLE);
        
        // For self - populate edit fields
        if (self) {
            binding.etFirstName.setText(firstName != null ? firstName : "");
            binding.etLastName.setText(lastName != null ? lastName : "");
            binding.etBio.setText(bio != null ? bio : "");
        } else {
            // For others - show bio in info section
            if (bio != null && !bio.isEmpty()) {
                binding.profileBio.setText(bio);
                binding.bioRow.setVisibility(View.VISIBLE);
            } else {
                binding.bioRow.setVisibility(View.GONE);
            }
            // Username info
            if (username != null && !username.isEmpty()) {
                binding.profileUsernameInfo.setText(username);
                binding.usernameRow.setVisibility(View.VISIBLE);
            } else {
                binding.usernameRow.setVisibility(View.GONE);
            }
            updateMuteButton();
        }

        bindPersonalChannel();
        bindBusinessHours(businessRaw);
        binding.bottomNav.setVisibility(View.VISIBLE);
    }

    private void bindStatus(boolean isOnline, String lastSeen) {
        if (binding == null) return;
        String statusText;
        if (isOnline) {
            statusText = getString(R.string.status_online);
        } else if (lastSeen != null && !lastSeen.isEmpty()) {
            statusText = getString(R.string.status_last_seen, lastSeen);
        } else {
            statusText = getString(R.string.status_hidden);
        }
        binding.profileStatus.setText(statusText);
        binding.profileStatus.setVisibility(View.VISIBLE);
    }

    private static String getStringOrNull(JsonObject obj, String key) {
        if (obj == null || key == null || !obj.has(key) || obj.get(key).isJsonNull()) return null;
        try {
            return obj.get(key).getAsString();
        } catch (Exception e) {
            return null;
        }
    }

    private static boolean getBoolean(JsonObject obj, String key) {
        if (obj == null || key == null || !obj.has(key) || obj.get(key).isJsonNull()) return false;
        try {
            return obj.get(key).getAsBoolean();
        } catch (Exception e) {
            return false;
        }
    }

    private void bindPersonalChannel() {
        if (personalChannelId == null || personalChannelId.isEmpty()) {
            binding.personalChannelCard.setVisibility(View.GONE);
            return;
        }
        binding.personalChannelCard.setVisibility(View.VISIBLE);
        String name = personalChannelName != null && !personalChannelName.isEmpty()
                ? personalChannelName
                : getString(R.string.profile_channel_placeholder);
        binding.personalChannelName.setText(name);
        if (personalChannelLink != null && !personalChannelLink.isEmpty()) {
            binding.personalChannelLink.setVisibility(View.VISIBLE);
            binding.personalChannelLink.setText("@" + personalChannelLink);
        } else {
            binding.personalChannelLink.setVisibility(View.GONE);
        }
    }

    private void bindPremiumBadge() {
        boolean showBadge = isPremiumUser || isSelf;
        binding.profilePremiumBadge.setVisibility(showBadge ? View.VISIBLE : View.GONE);
        if (!showBadge) {
            binding.profilePremiumBadge.setOnClickListener(null);
            return;
        }
        if (isPremiumUser) {
            binding.profilePremiumBadge.setBackgroundResource(R.drawable.premium_badge_bg);
            binding.profilePremiumBadge.setColorFilter(getColor(R.color.x_on_accent));
            binding.profilePremiumBadge.setContentDescription(getString(R.string.premium_badge));
            binding.profilePremiumBadge.setOnClickListener(null);
        } else {
            binding.profilePremiumBadge.setBackgroundResource(R.drawable.premium_badge_cta_bg);
            binding.profilePremiumBadge.setColorFilter(getColor(R.color.x_accent_start));
            binding.profilePremiumBadge.setContentDescription(getString(R.string.premium_open));
            binding.profilePremiumBadge.setOnClickListener(v -> openPremium());
        }
    }

    private void bindBusinessHours(JsonObject raw) {
        businessHours = BusinessHoursHelper.normalize(raw);
        if (raw == null || raw.entrySet().isEmpty()) {
            binding.profileBusinessCard.setVisibility(View.GONE);
            return;
        }
        binding.profileBusinessCard.setVisibility(View.VISIBLE);

        BusinessHoursHelper.Status status = BusinessHoursHelper.getStatus(businessHours, Calendar.getInstance());
        if (status.isOpen) {
            binding.profileBusinessStatus.setText(R.string.business_status_open);
            binding.profileBusinessStatus.setBackgroundResource(R.drawable.business_status_open_bg);
            binding.profileBusinessStatus.setTextColor(getColor(R.color.x_on_accent));
        } else {
            binding.profileBusinessStatus.setText(R.string.business_status_closed);
            binding.profileBusinessStatus.setBackgroundResource(R.drawable.business_status_closed_bg);
            binding.profileBusinessStatus.setTextColor(getColor(R.color.x_on_accent));
        }

        String hint = buildBusinessHint(status);
        if (hint.isEmpty()) {
            binding.profileBusinessNext.setVisibility(View.GONE);
        } else {
            binding.profileBusinessNext.setVisibility(View.VISIBLE);
            binding.profileBusinessNext.setText(hint);
        }

        updateBusinessTime(binding.profileBusinessTimeMon, businessHours.get("mon"));
        updateBusinessTime(binding.profileBusinessTimeTue, businessHours.get("tue"));
        updateBusinessTime(binding.profileBusinessTimeWed, businessHours.get("wed"));
        updateBusinessTime(binding.profileBusinessTimeThu, businessHours.get("thu"));
        updateBusinessTime(binding.profileBusinessTimeFri, businessHours.get("fri"));
        updateBusinessTime(binding.profileBusinessTimeSat, businessHours.get("sat"));
        updateBusinessTime(binding.profileBusinessTimeSun, businessHours.get("sun"));
    }

    private void updateBusinessTime(TextView target, BusinessHoursHelper.Entry entry) {
        if (target == null || entry == null) return;
        if (entry.enabled && entry.start != null && entry.end != null && !entry.start.isEmpty() && !entry.end.isEmpty()) {
            target.setText(entry.start + "-" + entry.end);
            target.setTextColor(getColor(R.color.x_text_primary));
        } else {
            target.setText(R.string.business_closed);
            target.setTextColor(getColor(R.color.x_text_secondary));
        }
    }

    private String buildBusinessHint(BusinessHoursHelper.Status status) {
        if (status == null) return "";
        if (status.isOpen && status.closeTime != null && !status.closeTime.isEmpty()) {
            return getString(R.string.business_status_until, status.closeTime);
        }
        if (status.nextOpenDay != null && status.nextOpenTime != null) {
            if (status.nextOpenIsToday && status.nextOpenMinutes != null && status.nextOpenMinutes > 0) {
                return getString(R.string.business_status_opens_in, formatCountdown(status.nextOpenMinutes));
            }
            if (status.nextOpenIsToday) {
                return getString(R.string.business_status_opens_today, status.nextOpenTime);
            }
            String dayLabel = mapDayLabel(status.nextOpenDay);
            if (!dayLabel.isEmpty()) {
                return getString(R.string.business_status_opens_day, dayLabel, status.nextOpenTime);
            }
        }
        return "";
    }

    private String formatCountdown(int minutes) {
        int safeMinutes = Math.max(0, minutes);
        int hours = safeMinutes / 60;
        int mins = safeMinutes % 60;
        if (hours > 0) {
            return String.format(Locale.getDefault(), "%d:%02d", hours, mins);
        }
        return String.format(Locale.getDefault(), "0:%02d", mins);
    }

    private String mapDayLabel(String dayKey) {
        if (dayKey == null) return "";
        switch (dayKey) {
            case "mon":
                return getString(R.string.business_day_mon);
            case "tue":
                return getString(R.string.business_day_tue);
            case "wed":
                return getString(R.string.business_day_wed);
            case "thu":
                return getString(R.string.business_day_thu);
            case "fri":
                return getString(R.string.business_day_fri);
            case "sat":
                return getString(R.string.business_day_sat);
            case "sun":
                return getString(R.string.business_day_sun);
            default:
                return "";
        }
    }

    private void openPremium() {
        startActivity(new Intent(this, PremiumActivity.class));
    }

    private void updateAvatar(String name, String url) {
        String letter = "?";
        if (name != null && !name.isEmpty()) {
            letter = name.substring(0, 1).toUpperCase(Locale.getDefault());
        }
        binding.profileAvatarLetter.setText(letter);
        if (url != null && !url.isEmpty()) {
            binding.profileAvatarImage.setVisibility(android.view.View.VISIBLE);
            binding.profileAvatarLetter.setVisibility(android.view.View.GONE);
            Glide.with(binding.profileAvatarImage.getContext())
                    .load(url)
                    .circleCrop()
                    .into(binding.profileAvatarImage);
        } else {
            binding.profileAvatarImage.setVisibility(android.view.View.GONE);
            binding.profileAvatarLetter.setVisibility(android.view.View.VISIBLE);
        }
    }

    private void saveProfile() {
        if (!isSelf) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        String firstName = binding.etFirstName.getText() != null ? binding.etFirstName.getText().toString().trim() : "";
        String lastName = binding.etLastName.getText() != null ? binding.etLastName.getText().toString().trim() : "";
        String bio = binding.etBio.getText() != null ? binding.etBio.getText().toString().trim() : "";
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.updateMyProfile(token, firstName, lastName, bio);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                runOnUiThread(() -> {
                    if (ok) {
                        Toast.makeText(this, R.string.profile_saved, Toast.LENGTH_SHORT).show();
                        loadProfile();
                    } else {
                        Toast.makeText(this, R.string.save_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void handlePickedAvatar(Uri uri) {
        if (uri == null || !isSelf) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        io.execute(() -> {
            try {
                ReadBytesResult rr = readBytes(getContentResolver(), uri, 10 * 1024 * 1024L);
                if (rr.tooLarge) {
                    runOnUiThread(() -> Toast.makeText(this, R.string.photo_too_large, Toast.LENGTH_SHORT).show());
                    return;
                }
                if (rr.bytes == null) {
                    runOnUiThread(() -> Toast.makeText(this, R.string.photo_read_failed, Toast.LENGTH_SHORT).show());
                    return;
                }
                String fileName = resolveFileName(uri);
                if (fileName == null || fileName.isEmpty()) {
                    fileName = "avatar.jpg";
                }
                String lowerName = fileName.toLowerCase(Locale.US);
                if (lowerName.endsWith(".gif") && !isPremiumUser) {
                    runOnUiThread(() -> Toast.makeText(this, R.string.premium_gif_locked, Toast.LENGTH_SHORT).show());
                    return;
                }
                String base64 = Base64.encodeToString(rr.bytes, Base64.NO_WRAP);
                ApiClient client = new ApiClient(token);
                JsonObject res = client.uploadAvatar(token, fileName, base64);
                String url = null;
                if (res != null) {
                    if (res.has("success") && !res.get("success").getAsBoolean()) {
                        String message = res.has("message") ? res.get("message").getAsString() : null;
                        if (message != null && message.toLowerCase(Locale.US).contains("premium")) {
                            runOnUiThread(() -> Toast.makeText(this, R.string.premium_gif_locked, Toast.LENGTH_SHORT).show());
                        } else {
                            runOnUiThread(() -> Toast.makeText(this, R.string.photo_update_failed, Toast.LENGTH_SHORT).show());
                        }
                        return;
                    }
                    if (res.has("url")) {
                        url = res.get("url").getAsString();
                    } else if (res.has("avatar_url")) {
                        url = res.get("avatar_url").getAsString();
                    }
                }
                String finalUrl = url;
                runOnUiThread(() -> {
                    if (finalUrl != null) {
                        String fullUrl = finalUrl.startsWith("http") ? finalUrl : BuildConfig.API_BASE + finalUrl;
                        avatarUrl = fullUrl;
                        updateAvatar(binding.profileName.getText().toString(), fullUrl);
                        Toast.makeText(this, R.string.photo_updated, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(this, R.string.photo_update_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.upload_failed, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private String resolveFileName(Uri uri) {
        if (uri == null) return null;
        ContentResolver resolver = getContentResolver();
        Cursor cursor = null;
        try {
            cursor = resolver.query(uri, null, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) {
                    return cursor.getString(index);
                }
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        String path = uri.getPath();
        if (path == null) return null;
        int slash = path.lastIndexOf('/');
        if (slash >= 0 && slash + 1 < path.length()) {
            return path.substring(slash + 1);
        }
        return null;
    }

    private void openAvatarPreview() {
        if (avatarUrl == null || avatarUrl.isEmpty()) return;
        Intent intent = new Intent(this, ImagePreviewActivity.class);
        intent.putExtra(ImagePreviewActivity.EXTRA_IMAGE_URL, avatarUrl);
        startActivity(intent);
    }

    private void openChat() {
        if (profileUserId == null) return;
        Intent intent = new Intent(this, ChatDetailActivity.class);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, profileUserId);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, binding.profileName.getText().toString());
        intent.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, false);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, false);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_SAVED, false);
        startActivity(intent);
    }

    private void openPersonalChannel() {
        if (personalChannelId == null || personalChannelId.isEmpty()) return;
        Intent intent = new Intent(this, ChatDetailActivity.class);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, personalChannelId);
        String title = personalChannelName != null && !personalChannelName.isEmpty()
                ? personalChannelName
                : getString(R.string.chat_type_channel);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, title);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, false);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, true);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_SAVED, false);
        startActivity(intent);
    }

    private static final class ReadBytesResult {
        final byte[] bytes;
        final boolean tooLarge;

        ReadBytesResult(byte[] bytes, boolean tooLarge) {
            this.bytes = bytes;
            this.tooLarge = tooLarge;
        }
    }

    private ReadBytesResult readBytes(ContentResolver resolver, Uri uri, long maxBytes) {
        InputStream in = null;
        try {
            String scheme = uri.getScheme();
            if (scheme == null || "file".equals(scheme)) {
                String path = uri.getPath();
                if (path == null) return new ReadBytesResult(null, false);
                File f = new File(path);
                if (!f.exists()) return new ReadBytesResult(null, false);
                if (f.length() > maxBytes) return new ReadBytesResult(null, true);
                in = new FileInputStream(f);
            } else {
                in = resolver.openInputStream(uri);
            }
            if (in == null) return new ReadBytesResult(null, false);
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int read;
            long total = 0;
            while ((read = in.read(buffer)) != -1) {
                total += read;
                if (total > maxBytes) {
                    return new ReadBytesResult(null, true);
                }
                out.write(buffer, 0, read);
            }
            return new ReadBytesResult(out.toByteArray(), false);
        } catch (Exception e) {
            return new ReadBytesResult(null, false);
        } finally {
            try {
                if (in != null) in.close();
            } catch (Exception ignored) {
            }
        }
    }

    // ========== Profile Action Buttons ==========

    private void onCallClicked() {
        if (profileUserId == null) return;
        // Open chat with call hint - calls are initiated from chat screen
        Intent intent = new Intent(this, ChatDetailActivity.class);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, profileUserId);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, binding.profileName.getText().toString());
        intent.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, false);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, false);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_SAVED, false);
        startActivity(intent);
        Toast.makeText(this, R.string.call_from_chat_hint, Toast.LENGTH_LONG).show();
    }

    private void onMuteClicked() {
        if (profileUserId == null) return;
        isMuted = !isMuted;
        saveMutedState(profileUserId, isMuted);
        updateMuteButton();
        Toast.makeText(this, 
            isMuted ? R.string.chat_muted : R.string.chat_unmuted, 
            Toast.LENGTH_SHORT).show();
    }

    private void updateMuteButton() {
        if (profileUserId == null) return;
        isMuted = isChatMuted(profileUserId);
        ImageView icon = binding.btnMuteIcon;
        TextView label = binding.btnMuteLabel;
        if (isMuted) {
            icon.setImageResource(R.drawable.ic_muted);
            label.setText(R.string.unmute_notifications);
        } else {
            icon.setImageResource(R.drawable.ic_notifications);
            label.setText(R.string.mute_notifications);
        }
    }

    private boolean isChatMuted(String chatId) {
        SharedPreferences prefs = getSharedPreferences(PREF_MUTED_CHATS, MODE_PRIVATE);
        Set<String> mutedSet = prefs.getStringSet(KEY_MUTED_SET, new HashSet<>());
        return mutedSet.contains(chatId);
    }

    private void saveMutedState(String chatId, boolean muted) {
        SharedPreferences prefs = getSharedPreferences(PREF_MUTED_CHATS, MODE_PRIVATE);
        Set<String> mutedSet = new HashSet<>(prefs.getStringSet(KEY_MUTED_SET, new HashSet<>()));
        if (muted) {
            mutedSet.add(chatId);
        } else {
            mutedSet.remove(chatId);
        }
        prefs.edit().putStringSet(KEY_MUTED_SET, mutedSet).apply();
    }

    private void onMoreClicked(View anchor) {
        PopupMenu popup = new PopupMenu(this, anchor);
        popup.getMenu().add(0, 1, 0, R.string.copy_id);
        popup.getMenu().add(0, 2, 1, R.string.copy_username);
        popup.getMenu().add(0, 3, 2, R.string.remove_friend);
        popup.getMenu().add(0, 4, 3, R.string.clear_history);
        popup.getMenu().add(0, 5, 4, R.string.report_user);

        popup.setOnMenuItemClickListener(item -> {
            switch (item.getItemId()) {
                case 1: // Copy ID
                    copyToClipboard(profileUserId);
                    Toast.makeText(this, R.string.copied, Toast.LENGTH_SHORT).show();
                    return true;
                case 2: // Copy username
                    if (profileUsername != null && !profileUsername.isEmpty()) {
                        copyToClipboard("@" + profileUsername);
                        Toast.makeText(this, R.string.copied, Toast.LENGTH_SHORT).show();
                    }
                    return true;
                case 3: // Remove friend
                    confirmRemoveFriend();
                    return true;
                case 4: // Clear history
                    confirmClearHistory();
                    return true;
                case 5: // Report
                    showReportDialog();
                    return true;
            }
            return false;
        });

        popup.show();
    }

    private void copyToClipboard(String text) {
        if (text == null) return;
        android.content.ClipboardManager clipboard = 
            (android.content.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        android.content.ClipData clip = android.content.ClipData.newPlainText("Xipher", text);
        clipboard.setPrimaryClip(clip);
    }

    private void confirmRemoveFriend() {
        new AlertDialog.Builder(this)
            .setTitle(R.string.remove_friend)
            .setMessage(R.string.confirm_remove_friend)
            .setPositiveButton(R.string.remove, (dialog, which) -> removeFriend())
            .setNegativeButton(R.string.cancel, null)
            .show();
    }

    private void removeFriend() {
        String token = authRepo.getToken();
        if (token == null || profileUserId == null) return;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.removeFriend(token, profileUserId);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                runOnUiThread(() -> {
                    if (ok) {
                        Toast.makeText(this, R.string.friend_removed, Toast.LENGTH_SHORT).show();
                        finish();
                    } else {
                        Toast.makeText(this, R.string.error_occurred, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void confirmClearHistory() {
        new AlertDialog.Builder(this)
            .setTitle(R.string.clear_history)
            .setMessage(R.string.confirm_clear_history)
            .setPositiveButton(R.string.clear, (dialog, which) -> clearHistory())
            .setNegativeButton(R.string.cancel, null)
            .show();
    }

    private void clearHistory() {
        String token = authRepo.getToken();
        if (token == null || profileUserId == null) return;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.clearChatHistory(token, profileUserId);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                runOnUiThread(() -> {
                    if (ok) {
                        Toast.makeText(this, R.string.history_cleared, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(this, R.string.error_occurred, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void showReportDialog() {
        android.widget.EditText input = new android.widget.EditText(this);
        input.setHint(R.string.report_reason_hint);
        new AlertDialog.Builder(this)
            .setTitle(R.string.report_user)
            .setView(input)
            .setPositiveButton(R.string.report, (dialog, which) -> {
                String reason = input.getText().toString().trim();
                if (!reason.isEmpty()) {
                    reportUser(reason);
                }
            })
            .setNegativeButton(R.string.cancel, null)
            .show();
    }

    private void reportUser(String reason) {
        String token = authRepo.getToken();
        if (token == null || profileUserId == null) return;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.reportUser(token, profileUserId, reason);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                runOnUiThread(() -> {
                    if (ok) {
                        Toast.makeText(this, R.string.report_sent, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(this, R.string.error_occurred, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show());
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
