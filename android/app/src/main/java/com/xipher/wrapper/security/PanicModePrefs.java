package com.xipher.wrapper.security;

import android.content.Context;
import android.content.SharedPreferences;

public final class PanicModePrefs {
    private static final String PREFS = "xipher_prefs";
    private static final String KEY_PANIC_ENABLED = "panic_mode_enabled";

    private PanicModePrefs() {}

    public static boolean isEnabled(Context context) {
        return getPrefs(context).getBoolean(KEY_PANIC_ENABLED, false);
    }

    public static void setEnabled(Context context, boolean enabled) {
        getPrefs(context).edit().putBoolean(KEY_PANIC_ENABLED, enabled).apply();
    }

    private static SharedPreferences getPrefs(Context context) {
        return context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
