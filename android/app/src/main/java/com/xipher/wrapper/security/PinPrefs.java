package com.xipher.wrapper.security;

import android.content.Context;
import android.content.SharedPreferences;

public final class PinPrefs {
    private static final String PREFS = "xipher_prefs";
    private static final String KEY_MAIN_PIN_SET = "main_pin_set";

    private PinPrefs() {}

    public static boolean isMainPinSet(Context context) {
        return getPrefs(context).getBoolean(KEY_MAIN_PIN_SET, false);
    }

    public static void setMainPinSet(Context context, boolean isSet) {
        getPrefs(context).edit().putBoolean(KEY_MAIN_PIN_SET, isSet).apply();
    }

    private static SharedPreferences getPrefs(Context context) {
        return context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
