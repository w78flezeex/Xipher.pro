package com.xipher.wrapper.data.model;

import com.google.gson.annotations.SerializedName;

/**
 * DTO для участника группы
 */
public class GroupMemberDto {
    private String id;
    
    @SerializedName("user_id")
    private String userId;
    
    private String username;
    
    private String role; // "member", "admin", "owner"
    
    @SerializedName("is_muted")
    private boolean isMuted;
    
    @SerializedName("is_banned")
    private boolean isBanned;
    
    @SerializedName("joined_at")
    private String joinedAt;
    
    private AdminPermissions permissions;
    
    // Getters
    public String getId() { return id; }
    public String getUserId() { return userId; }
    public String getUsername() { return username; }
    public String getRole() { return role; }
    public boolean isMuted() { return isMuted; }
    public boolean isBanned() { return isBanned; }
    public String getJoinedAt() { return joinedAt; }
    public AdminPermissions getPermissions() { return permissions; }
    
    // Setters
    public void setId(String id) { this.id = id; }
    public void setUserId(String userId) { this.userId = userId; }
    public void setUsername(String username) { this.username = username; }
    public void setRole(String role) { this.role = role; }
    public void setMuted(boolean muted) { this.isMuted = muted; }
    public void setBanned(boolean banned) { this.isBanned = banned; }
    public void setJoinedAt(String joinedAt) { this.joinedAt = joinedAt; }
    public void setPermissions(AdminPermissions permissions) { this.permissions = permissions; }
    
    public boolean isOwner() {
        return "owner".equals(role);
    }
    
    public boolean isAdmin() {
        return "admin".equals(role) || "owner".equals(role);
    }
    
    /**
     * Права администратора
     */
    public static class AdminPermissions {
        @SerializedName("can_change_info")
        private boolean canChangeInfo = true;
        
        @SerializedName("can_delete_messages")
        private boolean canDeleteMessages = true;
        
        @SerializedName("can_ban_users")
        private boolean canBanUsers = true;
        
        @SerializedName("can_invite_users")
        private boolean canInviteUsers = true;
        
        @SerializedName("can_pin_messages")
        private boolean canPinMessages = true;
        
        @SerializedName("can_manage_voice")
        private boolean canManageVoice = true;
        
        @SerializedName("can_promote_members")
        private boolean canPromoteMembers = false;
        
        @SerializedName("is_anonymous")
        private boolean isAnonymous = false;
        
        // Getters
        public boolean isCanChangeInfo() { return canChangeInfo; }
        public boolean isCanDeleteMessages() { return canDeleteMessages; }
        public boolean isCanBanUsers() { return canBanUsers; }
        public boolean isCanInviteUsers() { return canInviteUsers; }
        public boolean isCanPinMessages() { return canPinMessages; }
        public boolean isCanManageVoice() { return canManageVoice; }
        public boolean isCanPromoteMembers() { return canPromoteMembers; }
        public boolean isAnonymous() { return isAnonymous; }
        
        // Setters
        public void setCanChangeInfo(boolean v) { this.canChangeInfo = v; }
        public void setCanDeleteMessages(boolean v) { this.canDeleteMessages = v; }
        public void setCanBanUsers(boolean v) { this.canBanUsers = v; }
        public void setCanInviteUsers(boolean v) { this.canInviteUsers = v; }
        public void setCanPinMessages(boolean v) { this.canPinMessages = v; }
        public void setCanManageVoice(boolean v) { this.canManageVoice = v; }
        public void setCanPromoteMembers(boolean v) { this.canPromoteMembers = v; }
        public void setAnonymous(boolean v) { this.isAnonymous = v; }
        
        public String getSummary() {
            int count = 0;
            if (canChangeInfo) count++;
            if (canDeleteMessages) count++;
            if (canBanUsers) count++;
            if (canInviteUsers) count++;
            if (canPinMessages) count++;
            if (canManageVoice) count++;
            if (canPromoteMembers) count++;
            
            if (count == 7) return "Все права";
            if (count == 0) return "Без особых прав";
            return count + " прав";
        }
    }
}
