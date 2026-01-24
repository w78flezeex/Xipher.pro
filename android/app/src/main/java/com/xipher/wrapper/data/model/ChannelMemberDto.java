package com.xipher.wrapper.data.model;

import com.google.gson.annotations.SerializedName;

/**
 * DTO для участника канала (подписчика/администратора)
 */
public class ChannelMemberDto {
    private String id;
    
    @SerializedName("user_id")
    private String userId;
    
    private String username;
    
    private String role; // "subscriber", "admin", "owner", "creator"
    
    @SerializedName("is_banned")
    private boolean isBanned;
    
    @SerializedName("admin_perms")
    private int adminPerms; // Битовая маска прав
    
    @SerializedName("admin_title")
    private String adminTitle;
    
    @SerializedName("joined_at")
    private String joinedAt;
    
    // Константы прав администратора канала
    public static final int PERM_CHANGE_INFO = 1;      // Изменение информации
    public static final int PERM_POST_MESSAGES = 2;    // Публикация сообщений
    public static final int PERM_INVITE = 16;          // Добавление участников
    public static final int PERM_RESTRICT = 32;        // Блокировка пользователей
    public static final int PERM_PIN = 64;             // Закрепление сообщений
    public static final int PERM_PROMOTE = 128;        // Добавление админов
    
    // Getters
    public String getId() { return id; }
    public String getUserId() { return userId; }
    public String getUsername() { return username; }
    public String getRole() { return role; }
    public boolean isBanned() { return isBanned; }
    public int getAdminPerms() { return adminPerms; }
    public String getAdminTitle() { return adminTitle; }
    public String getJoinedAt() { return joinedAt; }
    
    // Setters
    public void setId(String id) { this.id = id; }
    public void setUserId(String userId) { this.userId = userId; }
    public void setUsername(String username) { this.username = username; }
    public void setRole(String role) { this.role = role; }
    public void setBanned(boolean banned) { isBanned = banned; }
    public void setAdminPerms(int adminPerms) { this.adminPerms = adminPerms; }
    public void setAdminTitle(String adminTitle) { this.adminTitle = adminTitle; }
    public void setJoinedAt(String joinedAt) { this.joinedAt = joinedAt; }
    
    // Хелпер-методы для проверки прав
    public boolean hasPermission(int permission) {
        return (adminPerms & permission) != 0;
    }
    
    public boolean canChangeInfo() {
        return hasPermission(PERM_CHANGE_INFO);
    }
    
    public boolean canPostMessages() {
        return hasPermission(PERM_POST_MESSAGES);
    }
    
    public boolean canInvite() {
        return hasPermission(PERM_INVITE);
    }
    
    public boolean canRestrict() {
        return hasPermission(PERM_RESTRICT);
    }
    
    public boolean canPin() {
        return hasPermission(PERM_PIN);
    }
    
    public boolean canPromote() {
        return hasPermission(PERM_PROMOTE);
    }
    
    public boolean isOwner() {
        return "owner".equals(role) || "creator".equals(role);
    }
    
    public boolean isAdmin() {
        return "admin".equals(role);
    }
    
    public boolean isAdminOrOwner() {
        return isOwner() || isAdmin();
    }
    
    public String getRoleDisplayName() {
        if (isOwner()) return "Владелец";
        if (isAdmin()) return "Администратор";
        return "Подписчик";
    }
    
    public String getPermissionsDisplayString() {
        if (isOwner()) return "Все права";
        if (!isAdmin()) return "";
        
        StringBuilder sb = new StringBuilder();
        if (canChangeInfo()) appendPerm(sb, "Инфо");
        if (canPostMessages()) appendPerm(sb, "Сообщения");
        if (canInvite()) appendPerm(sb, "Приглашения");
        if (canRestrict()) appendPerm(sb, "Блокировки");
        if (canPin()) appendPerm(sb, "Пины");
        if (canPromote()) appendPerm(sb, "Админы");
        return sb.toString();
    }
    
    private void appendPerm(StringBuilder sb, String perm) {
        if (sb.length() > 0) sb.append(", ");
        sb.append(perm);
    }
}
