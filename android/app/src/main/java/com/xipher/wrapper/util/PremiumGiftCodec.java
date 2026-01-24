package com.xipher.wrapper.util;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.xipher.wrapper.data.model.PremiumGiftPayload;

public final class PremiumGiftCodec {
    public static final String PREMIUM_GIFT_PREFIX = "[[XIPHER_PREMIUM_GIFT]]";

    private static final Gson GSON = new GsonBuilder().create();

    private PremiumGiftCodec() {}

    public static boolean isPremiumGiftContent(String content) {
        return content != null && content.startsWith(PREMIUM_GIFT_PREFIX);
    }

    public static PremiumGiftPayload parseGift(String content) {
        if (!isPremiumGiftContent(content)) return null;
        String json = content.substring(PREMIUM_GIFT_PREFIX.length()).trim();
        if (json.isEmpty()) return null;
        try {
            return GSON.fromJson(json, PremiumGiftPayload.class);
        } catch (Exception e) {
            return null;
        }
    }
}
