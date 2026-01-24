package com.xipher.wrapper.util;

public final class SpoilerCodec {
    private static final String PREFIX = "[spoiler]";

    private SpoilerCodec() {}

    public static boolean isSpoilerContent(String content) {
        return content != null && content.startsWith(PREFIX);
    }

    public static String encode(String content) {
        String safe = content != null ? content : "";
        return PREFIX + safe;
    }

    public static String strip(String content) {
        if (content == null) return "";
        return isSpoilerContent(content) ? content.substring(PREFIX.length()) : content;
    }
}
