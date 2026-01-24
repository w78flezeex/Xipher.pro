package com.xipher.wrapper.data.repo;

import android.content.SharedPreferences;

import com.google.gson.JsonObject;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.AuthResponse;
import com.xipher.wrapper.data.model.BasicResponse;

public class AuthRepository {
    private final SharedPreferences prefs;
    private final SharedPreferences legacyPrefs;

    private static final String KEY_TOKEN = "xipher_token";
    private static final String KEY_USER_ID = "xipher_user_id";
    private static final String KEY_USERNAME = "xipher_username";
    private static final String KEY_FIRST_NAME = "xipher_first_name";
    private static final String KEY_LAST_NAME = "xipher_last_name";
    private static final String KEY_PHONE = "xipher_phone";
    private static final String KEY_AVATAR_URL = "xipher_avatar_url";

    public AuthRepository(SharedPreferences prefs) {
        this(prefs, null);
    }

    public AuthRepository(SharedPreferences prefs, SharedPreferences legacyPrefs) {
        this.prefs = prefs;
        this.legacyPrefs = legacyPrefs;
        migrateLegacyIfNeeded();
    }

    public AuthResponse login(String username, String password) throws Exception {
        ApiClient client = new ApiClient(null);
        AuthResponse res = client.login(username, password);
        if (res != null && res.success) {
            AuthResponse.AuthData data = res.data != null ? res.data : new AuthResponse.AuthData();
            String token = data.token != null ? data.token : "";
            if (!token.isEmpty()) {
                if ((data.user_id == null || data.user_id.isEmpty())
                        || (data.username == null || data.username.isEmpty())) {
                    try {
                        ApiClient authed = new ApiClient(token);
                        JsonObject info = authed.validateTokenInfo(token);
                        if (info != null && info.has("success") && info.get("success").getAsBoolean() && info.has("data")) {
                            JsonObject infoData = info.getAsJsonObject("data");
                            if ((data.user_id == null || data.user_id.isEmpty()) && infoData.has("user_id")) {
                                data.user_id = infoData.get("user_id").getAsString();
                            }
                            if ((data.username == null || data.username.isEmpty()) && infoData.has("username")) {
                                data.username = infoData.get("username").getAsString();
                            }
                        }
                    } catch (Exception ignored) {
                    }
                }
                save(token, data.user_id, data.username);
            }
        }
        return res;
    }

    public BasicResponse register(String username, String password) throws Exception {
        ApiClient client = new ApiClient(null);
        return client.register(username, password);
    }

    public BasicResponse checkUsername(String username) throws Exception {
        ApiClient client = new ApiClient(null);
        return client.checkUsername(username);
    }

    public boolean validate() {
        String token = getToken();
        if (token == null) return false;
        ApiClient client = new ApiClient(token);
        return client.validate(token);
    }

    public void save(String token, String userId, String username) {
        prefs.edit()
                .putString(KEY_TOKEN, token)
                .putString(KEY_USER_ID, userId)
                .putString(KEY_USERNAME, username)
                .apply();
        if (legacyPrefs != null) {
            legacyPrefs.edit()
                    .remove(KEY_TOKEN)
                    .remove(KEY_USER_ID)
                    .remove(KEY_USERNAME)
                    .apply();
        }
    }

    public void clear() {
        prefs.edit().clear().apply();
        if (legacyPrefs != null) {
            legacyPrefs.edit().clear().apply();
        }
    }

    public String getToken() { return prefs.getString(KEY_TOKEN, null); }
    public String getUserId() { return prefs.getString(KEY_USER_ID, null); }
    public String getUsername() { return prefs.getString(KEY_USERNAME, null); }
    public String getFirstName() { return prefs.getString(KEY_FIRST_NAME, null); }
    public String getLastName() { return prefs.getString(KEY_LAST_NAME, null); }
    public String getPhone() { return prefs.getString(KEY_PHONE, null); }
    public String getAvatarUrl() { return prefs.getString(KEY_AVATAR_URL, null); }

    public void saveProfile(String firstName, String lastName, String phone, String avatarUrl) {
        prefs.edit()
                .putString(KEY_FIRST_NAME, firstName)
                .putString(KEY_LAST_NAME, lastName)
                .putString(KEY_PHONE, phone)
                .putString(KEY_AVATAR_URL, avatarUrl)
                .apply();
    }

    private void migrateLegacyIfNeeded() {
        if (legacyPrefs == null) return;
        String legacyToken = legacyPrefs.getString(KEY_TOKEN, null);
        String legacyUserId = legacyPrefs.getString(KEY_USER_ID, null);
        String legacyUsername = legacyPrefs.getString(KEY_USERNAME, null);
        boolean hasLegacy = (legacyToken != null && !legacyToken.isEmpty())
                || (legacyUserId != null && !legacyUserId.isEmpty())
                || (legacyUsername != null && !legacyUsername.isEmpty());
        if (!hasLegacy) return;
        String currentToken = prefs.getString(KEY_TOKEN, null);
        if (currentToken == null || currentToken.isEmpty()) {
            prefs.edit()
                    .putString(KEY_TOKEN, legacyToken)
                    .putString(KEY_USER_ID, legacyUserId)
                    .putString(KEY_USERNAME, legacyUsername)
                    .apply();
        }
        legacyPrefs.edit()
                .remove(KEY_TOKEN)
                .remove(KEY_USER_ID)
                .remove(KEY_USERNAME)
                .apply();
    }
}
