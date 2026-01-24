package com.xipher.wrapper.util;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.Calendar;
import java.util.LinkedHashMap;
import java.util.Map;

public final class BusinessHoursHelper {
    public static final String[] DAY_ORDER = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    private static final String[] JS_DAY_KEYS = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

    private BusinessHoursHelper() {
    }

    public static final class Entry {
        public boolean enabled;
        public String start;
        public String end;

        Entry(boolean enabled, String start, String end) {
            this.enabled = enabled;
            this.start = start;
            this.end = end;
        }

        Entry copy() {
            return new Entry(enabled, start, end);
        }
    }

    public static final class Status {
        public boolean isOpen;
        public String closeTime;
        public String nextOpenDay;
        public String nextOpenTime;
        public Integer nextOpenMinutes;
        public boolean nextOpenIsToday;
    }

    public static Map<String, Entry> defaultHours() {
        Map<String, Entry> out = new LinkedHashMap<>();
        out.put("mon", new Entry(true, "09:00", "18:00"));
        out.put("tue", new Entry(true, "09:00", "18:00"));
        out.put("wed", new Entry(true, "09:00", "18:00"));
        out.put("thu", new Entry(true, "09:00", "18:00"));
        out.put("fri", new Entry(true, "09:00", "18:00"));
        out.put("sat", new Entry(false, "10:00", "16:00"));
        out.put("sun", new Entry(false, "10:00", "16:00"));
        return out;
    }

    public static Map<String, Entry> normalize(JsonObject raw) {
        Map<String, Entry> out = defaultHours();
        if (raw == null) {
            return out;
        }
        for (String day : DAY_ORDER) {
            JsonElement element = raw.get(day);
            if (element == null || !element.isJsonObject()) {
                continue;
            }
            JsonObject obj = element.getAsJsonObject();
            Entry base = out.get(day);
            if (base == null) {
                base = new Entry(false, "", "");
                out.put(day, base);
            }
            if (obj.has("enabled")) {
                base.enabled = obj.get("enabled").getAsBoolean();
            }
            if (obj.has("start")) {
                base.start = obj.get("start").getAsString();
            }
            if (obj.has("end")) {
                base.end = obj.get("end").getAsString();
            }
        }
        return out;
    }

    public static JsonObject toJson(Map<String, Entry> hours) {
        JsonObject root = new JsonObject();
        if (hours == null) {
            return root;
        }
        for (String day : DAY_ORDER) {
            Entry entry = hours.get(day);
            if (entry == null) {
                continue;
            }
            JsonObject obj = new JsonObject();
            obj.addProperty("enabled", entry.enabled);
            obj.addProperty("start", entry.start != null ? entry.start : "");
            obj.addProperty("end", entry.end != null ? entry.end : "");
            root.add(day, obj);
        }
        return root;
    }

    public static boolean hasEnabledDays(Map<String, Entry> hours) {
        if (hours == null) return false;
        for (Entry entry : hours.values()) {
            if (entry != null && entry.enabled) {
                return true;
            }
        }
        return false;
    }

    public static Status getStatus(Map<String, Entry> hours, Calendar now) {
        Status status = new Status();
        if (hours == null || now == null) {
            return status;
        }
        String todayKey = getTodayKey(now);
        int todayIndex = indexOfDay(todayKey);
        int nowMinutes = now.get(Calendar.HOUR_OF_DAY) * 60 + now.get(Calendar.MINUTE);

        if (todayKey != null) {
            Entry today = hours.get(todayKey);
            if (today != null && today.enabled) {
                Integer start = parseTimeToMinutes(today.start);
                Integer end = parseTimeToMinutes(today.end);
                if (start != null && end != null) {
                    if (nowMinutes >= start && nowMinutes < end) {
                        status.isOpen = true;
                        status.closeTime = today.end;
                        return status;
                    }
                    if (nowMinutes < start) {
                        status.nextOpenDay = todayKey;
                        status.nextOpenTime = today.start;
                        status.nextOpenMinutes = start - nowMinutes;
                        status.nextOpenIsToday = true;
                        return status;
                    }
                }
            }
        }

        int baseIndex = todayIndex >= 0 ? todayIndex : 0;
        for (int offset = 1; offset <= DAY_ORDER.length; offset++) {
            String dayKey = DAY_ORDER[(baseIndex + offset) % DAY_ORDER.length];
            Entry entry = hours.get(dayKey);
            if (entry == null || !entry.enabled) {
                continue;
            }
            Integer start = parseTimeToMinutes(entry.start);
            Integer end = parseTimeToMinutes(entry.end);
            if (start == null || end == null) {
                continue;
            }
            status.nextOpenDay = dayKey;
            status.nextOpenTime = entry.start;
            status.nextOpenIsToday = dayKey.equals(todayKey);
            return status;
        }

        return status;
    }

    public static Integer parseTimeToMinutes(String time) {
        if (time == null) return null;
        String[] parts = time.split(":");
        if (parts.length != 2) return null;
        try {
            int hours = Integer.parseInt(parts[0]);
            int minutes = Integer.parseInt(parts[1]);
            if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
                return null;
            }
            return hours * 60 + minutes;
        } catch (NumberFormatException e) {
            return null;
        }
    }

    public static String getTodayKey(Calendar calendar) {
        int dayOfWeek = calendar.get(Calendar.DAY_OF_WEEK);
        int index = dayOfWeek - Calendar.SUNDAY;
        if (index < 0 || index >= JS_DAY_KEYS.length) {
            return null;
        }
        return JS_DAY_KEYS[index];
    }

    private static int indexOfDay(String dayKey) {
        if (dayKey == null) return -1;
        for (int i = 0; i < DAY_ORDER.length; i++) {
            if (dayKey.equals(DAY_ORDER[i])) {
                return i;
            }
        }
        return -1;
    }
}
