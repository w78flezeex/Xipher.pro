package com.xipher.wrapper.data.db;

import android.content.Context;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.db.entity.MessageEntity;

import java.security.SecureRandom;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;

final class DecoyDataGenerator {
    private static final String[] DIRECT_NAMES = {
            "Alex", "Mila", "Dani", "Max", "Nora", "Sam", "Liam", "Ivy", "Leo", "Eva",
            "Omar", "Zoe", "Maya", "Theo", "Lara", "Noah", "Iris", "Evan", "Lina", "Jude"
    };
    private static final String[] GROUP_NAMES = {
            "Family", "Weekend Plan", "Project Atlas", "Gym Buddies", "Book Club", "Study Group",
            "Neighborhood", "Travel Team", "Events Crew", "Household"
    };
    private static final String[] CHANNEL_NAMES = {
            "City News", "Market Update", "Tech Brief", "Sports Feed", "Travel Deals",
            "Daily Digest", "Community Alerts", "Product Updates"
    };
    private static final String[] DIRECT_MESSAGES = {
            "Hey, are we still on for today?",
            "Got it, thanks!",
            "On my way.",
            "Can you send the file?",
            "Let's do it tomorrow.",
            "Sounds good.",
            "I'll check and get back to you.",
            "See you at 6.",
            "All set here.",
            "Thanks, appreciate it."
    };
    private static final String[] GROUP_MESSAGES = {
            "Reminder: meeting at 4pm.",
            "Anyone free this weekend?",
            "I'll bring snacks.",
            "Updated the doc.",
            "Please review the notes.",
            "Let's sync after lunch.",
            "Sharing the draft now.",
            "Heads-up: schedule changed."
    };
    private static final String[] CHANNEL_MESSAGES = {
            "Daily update: new features rolling out.",
            "Tip of the day: use labels to stay organized.",
            "Weekly digest is live.",
            "Don't miss the upcoming event.",
            "System maintenance scheduled for tonight."
    };

    private DecoyDataGenerator() {}

    static void generate(Context context, AppDatabase source, AppDatabase target, String selfId, String selfName) {
        if (target == null) return;
        List<ChatEntity> sourceChats = source != null ? source.chatDao().getAll() : null;
        if (sourceChats == null || sourceChats.isEmpty()) {
            seedFallback(target, System.currentTimeMillis());
            return;
        }

        SecureRandom random = new SecureRandom();
        long now = System.currentTimeMillis();
        Set<String> usedNames = new HashSet<>();
        List<ChatEntity> chats = new ArrayList<>();
        List<MessageEntity> messages = new ArrayList<>();
        boolean createdSaved = false;

        int index = 0;
        for (ChatEntity src : sourceChats) {
            if (src == null) continue;
            boolean isGroup = src.isGroup;
            boolean isChannel = src.isChannel;
            boolean isSaved = src.isSaved && !createdSaved;

            ChatEntity chat = new ChatEntity();
            chat.id = "decoy_" + index + "_" + UUID.randomUUID().toString();
            chat.isGroup = isGroup;
            chat.isChannel = isChannel;
            chat.isSaved = isSaved;
            chat.isPrivate = src.isPrivate;
            chat.avatarUrl = null;
            chat.unread = random.nextInt(3);

            if (isSaved) {
                createdSaved = true;
                chat.title = context != null ? context.getString(R.string.chat_type_saved) : "Saved Messages";
                chat.username = null;
            } else if (isGroup) {
                String name = pickUnique(GROUP_NAMES, usedNames, random);
                chat.title = name;
                chat.username = null;
            } else if (isChannel) {
                String name = pickUnique(CHANNEL_NAMES, usedNames, random);
                chat.title = name;
                chat.username = toHandle(name);
            } else {
                String name = pickUnique(DIRECT_NAMES, usedNames, random);
                chat.title = name;
                chat.username = toHandle(name);
                chat.isPrivate = true;
            }

            int msgCount = estimateMessageCount(source, src, random);
            long baseTime = now - (index * 10L * 60L * 1000L) - random.nextInt(5 * 60 * 1000);
            List<MessageEntity> chatMessages = buildMessages(chat, msgCount, baseTime, random, selfId, selfName);
            if (!chatMessages.isEmpty()) {
                MessageEntity last = chatMessages.get(chatMessages.size() - 1);
                chat.lastMessage = last.content != null ? last.content : "";
                chat.updatedAt = last.createdAtMillis > 0L ? last.createdAtMillis : baseTime;
            } else {
                chat.lastMessage = pickMessage(chat, random);
                chat.updatedAt = baseTime;
            }

            chats.add(chat);
            messages.addAll(chatMessages);
            index++;
        }

        target.runInTransaction(() -> {
            target.messageDao().clearAll();
            target.chatDao().clear();
            target.chatDao().upsertAll(chats);
            target.messageDao().upsertAll(messages);
        });
    }

    private static int estimateMessageCount(AppDatabase source, ChatEntity chat, SecureRandom random) {
        if (source == null || chat == null || random == null) return 3;
        int realCount = 0;
        try {
            realCount = source.messageDao().countByChat(chat.id);
        } catch (Exception ignored) {
        }
        int count = 2 + (int) Math.round(Math.log(realCount + 1) * 2.0d);
        if (count < 2) count = 2;
        if (count > 12) count = 12;
        return count;
    }

    private static List<MessageEntity> buildMessages(ChatEntity chat, int count, long baseTime, SecureRandom random,
                                                     String selfId, String selfName) {
        List<MessageEntity> result = new ArrayList<>();
        if (chat == null || count <= 0) return result;
        String localSelfId = (selfId != null && !selfId.isEmpty()) ? selfId : "decoy_self";
        String localSelfName = (selfName != null && !selfName.isEmpty()) ? selfName : "You";
        List<String> groupOthers = new ArrayList<>();
        if (chat.isGroup) {
            groupOthers.add("Alex");
            groupOthers.add("Mila");
            groupOthers.add("Sam");
        }
        for (int i = 0; i < count; i++) {
            MessageEntity m = new MessageEntity();
            m.id = chat.id + "_m" + i;
            m.tempId = m.id;
            m.chatId = chat.id;
            m.messageType = "text";
            m.content = pickMessage(chat, random);

            boolean fromSelf;
            if (chat.isChannel) {
                fromSelf = false;
            } else if (chat.isGroup) {
                fromSelf = random.nextBoolean();
            } else {
                fromSelf = (i % 2 == 0);
            }
            if (chat.isChannel) {
                m.senderId = "decoy_channel";
                m.senderName = chat.title != null ? chat.title : "Channel";
            } else if (fromSelf) {
                m.senderId = localSelfId;
                m.senderName = localSelfName;
            } else if (chat.isGroup) {
                String name = groupOthers.get(random.nextInt(groupOthers.size()));
                m.senderId = "decoy_" + toHandle(name);
                m.senderName = name;
            } else {
                String name = chat.title != null ? chat.title : "Contact";
                m.senderId = "decoy_" + toHandle(name);
                m.senderName = name;
            }

            long jitter = random.nextInt(60 * 1000);
            long createdAt = baseTime - ((count - 1 - i) * 2L * 60L * 1000L) + jitter;
            m.createdAtMillis = createdAt;
            m.createdAt = formatIso(createdAt);
            m.status = "read";
            result.add(m);
        }
        return result;
    }

    private static String pickMessage(ChatEntity chat, SecureRandom random) {
        if (chat == null || random == null) return "All good.";
        if (chat.isChannel) {
            return CHANNEL_MESSAGES[random.nextInt(CHANNEL_MESSAGES.length)];
        }
        if (chat.isGroup) {
            return GROUP_MESSAGES[random.nextInt(GROUP_MESSAGES.length)];
        }
        return DIRECT_MESSAGES[random.nextInt(DIRECT_MESSAGES.length)];
    }

    private static String pickUnique(String[] pool, Set<String> used, SecureRandom random) {
        if (pool == null || pool.length == 0) return "Chat";
        for (int i = 0; i < pool.length * 2; i++) {
            String candidate = pool[random.nextInt(pool.length)];
            if (used.add(candidate)) {
                return candidate;
            }
        }
        String fallback = pool[random.nextInt(pool.length)] + " " + (used.size() + 1);
        used.add(fallback);
        return fallback;
    }

    private static String toHandle(String name) {
        if (name == null) return "user";
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (c >= 'A' && c <= 'Z') {
                sb.append((char) (c + 32));
            } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                sb.append(c);
            }
        }
        if (sb.length() < 3) {
            sb.append("user");
        }
        return sb.toString();
    }

    private static String formatIso(long millis) {
        SimpleDateFormat fmt = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.US);
        return fmt.format(millis);
    }

    private static void seedFallback(AppDatabase db, long now) {
        if (db == null) return;
        ChatEntity welcome = new ChatEntity();
        welcome.id = "decoy_welcome";
        welcome.title = "Welcome";
        welcome.username = "xipher";
        welcome.lastMessage = "Welcome to Xipher. Tap here for tips.";
        welcome.unread = 0;
        welcome.isChannel = false;
        welcome.isGroup = false;
        welcome.isSaved = false;
        welcome.isPrivate = true;
        welcome.updatedAt = now - 60000L;

        ChatEntity support = new ChatEntity();
        support.id = "decoy_support";
        support.title = "Support";
        support.username = "support";
        support.lastMessage = "We are here if you need anything.";
        support.unread = 0;
        support.isChannel = false;
        support.isGroup = false;
        support.isSaved = false;
        support.isPrivate = true;
        support.updatedAt = now - 30000L;

        MessageEntity welcomeMsg = new MessageEntity();
        welcomeMsg.id = "decoy_welcome_1";
        welcomeMsg.chatId = welcome.id;
        welcomeMsg.senderId = "support_bot";
        welcomeMsg.senderName = "Xipher";
        welcomeMsg.content = "Welcome to Xipher. Your chats are ready to go.";
        welcomeMsg.messageType = "text";
        welcomeMsg.createdAtMillis = now - 60000L;
        welcomeMsg.createdAt = formatIso(welcomeMsg.createdAtMillis);
        welcomeMsg.status = "read";

        MessageEntity supportMsg = new MessageEntity();
        supportMsg.id = "decoy_support_1";
        supportMsg.chatId = support.id;
        supportMsg.senderId = "support";
        supportMsg.senderName = "Support";
        supportMsg.content = "Hi! Let us know if you have any questions.";
        supportMsg.messageType = "text";
        supportMsg.createdAtMillis = now - 30000L;
        supportMsg.createdAt = formatIso(supportMsg.createdAtMillis);
        supportMsg.status = "read";

        List<ChatEntity> chats = new ArrayList<>();
        chats.add(welcome);
        chats.add(support);
        List<MessageEntity> messages = new ArrayList<>();
        messages.add(welcomeMsg);
        messages.add(supportMsg);

        db.runInTransaction(() -> {
            db.messageDao().clearAll();
            db.chatDao().clear();
            db.chatDao().upsertAll(chats);
            db.messageDao().upsertAll(messages);
        });
    }
}
