package com.xipher.wrapper.util;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

public final class LocationUtils {

    private LocationUtils() {}

    public static final class LocationPayload {
        public final double lat;
        public final double lon;
        public final String liveId;
        public final long expiresAt;

        LocationPayload(double lat, double lon, String liveId, long expiresAt) {
            this.lat = lat;
            this.lon = lon;
            this.liveId = liveId;
            this.expiresAt = expiresAt;
        }
    }

    public static String buildMapUrl(double lat, double lon, String liveId, long expiresAt) {
        String base = String.format(Locale.US,
                "https://yandex.ru/maps/?pt=%.6f,%.6f&z=16&l=map", lon, lat);
        if (liveId == null || liveId.isEmpty()) {
            return base;
        }
        return base + "#xipher_live=1&xipher_id=" + liveId + "&xipher_expires=" + expiresAt;
    }

    public static String buildStaticMapUrl(double lat, double lon, int width, int height) {
        return String.format(Locale.US,
                "https://static-maps.yandex.ru/1.x/?ll=%.6f,%.6f&size=%d,%d&z=15&l=map&pt=%.6f,%.6f,pm2rdm",
                lon, lat, width, height, lon, lat);
    }

    public static LocationPayload parse(String content) {
        if (content == null) return null;
        String trimmed = content.trim();
        if (trimmed.isEmpty()) return null;
        double lat = Double.NaN;
        double lon = Double.NaN;

        if (trimmed.startsWith("geo:")) {
            String coords = trimmed.substring(4);
            int q = coords.indexOf('?');
            if (q >= 0) coords = coords.substring(0, q);
            double[] parsed = parseLatLon(coords);
            lat = parsed[0];
            lon = parsed[1];
        }

        if (Double.isNaN(lat) || Double.isNaN(lon)) {
            String pt = getParam(trimmed, "pt");
            if (pt != null) {
                double[] parsed = parseLonLat(pt);
                lon = parsed[0];
                lat = parsed[1];
            }
        }

        if (Double.isNaN(lat) || Double.isNaN(lon)) {
            String ll = getParam(trimmed, "ll");
            if (ll != null) {
                double[] parsed = parseLonLat(ll);
                lon = parsed[0];
                lat = parsed[1];
            }
        }

        if (Double.isNaN(lat) || Double.isNaN(lon)) {
            double[] parsed = parseLatLon(trimmed);
            lat = parsed[0];
            lon = parsed[1];
        }

        String liveId = null;
        long expiresAt = 0L;
        Map<String, String> fragment = parseFragment(trimmed);
        if (fragment != null) {
            liveId = fragment.get("xipher_id");
            expiresAt = parseLong(fragment.get("xipher_expires"));
        }

        if (Double.isNaN(lat) || Double.isNaN(lon)) {
            return null;
        }
        return new LocationPayload(lat, lon, liveId, expiresAt);
    }

    private static String getParam(String text, String key) {
        String target = key + "=";
        int start = text.indexOf(target);
        if (start < 0) return null;
        start += target.length();
        int end = text.indexOf('&', start);
        if (end < 0) end = text.indexOf('#', start);
        if (end < 0) end = text.length();
        return text.substring(start, end);
    }

    private static Map<String, String> parseFragment(String text) {
        int hash = text.indexOf('#');
        if (hash < 0 || hash + 1 >= text.length()) return null;
        String fragment = text.substring(hash + 1);
        Map<String, String> out = new HashMap<>();
        String[] pairs = fragment.split("&");
        for (String pair : pairs) {
            if (pair.isEmpty()) continue;
            int eq = pair.indexOf('=');
            if (eq < 0) continue;
            String k = pair.substring(0, eq);
            String v = pair.substring(eq + 1);
            out.put(k, v);
        }
        return out;
    }

    private static double[] parseLonLat(String value) {
        String[] parts = value.split(",");
        if (parts.length < 2) return new double[]{Double.NaN, Double.NaN};
        double lon = parseDouble(parts[0]);
        double lat = parseDouble(parts[1]);
        return new double[]{lon, lat};
    }

    private static double[] parseLatLon(String value) {
        String[] parts = value.split(",");
        if (parts.length < 2) return new double[]{Double.NaN, Double.NaN};
        double lat = parseDouble(parts[0]);
        double lon = parseDouble(parts[1]);
        return new double[]{lat, lon};
    }

    private static double parseDouble(String value) {
        if (value == null) return Double.NaN;
        try {
            return Double.parseDouble(value.trim());
        } catch (Exception e) {
            return Double.NaN;
        }
    }

    private static long parseLong(String value) {
        if (value == null || value.isEmpty()) return 0L;
        try {
            return Long.parseLong(value);
        } catch (Exception e) {
            return 0L;
        }
    }
}
