#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include <string>
#include <map>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include "../auth/auth_manager.hpp"
#include "../database/db_manager.hpp"
#include "../storage/in_memory_storage.hpp"
#include "../notifications/fcm_client.hpp"
#include "../notifications/rustore_client.hpp"
#include <functional>
#include "admin_handler.hpp"
#include "../security/admin_security.hpp"

namespace xipher {

class RequestHandler {
public:
    RequestHandler(DatabaseManager& db_manager, AuthManager& auth_manager);
    
    std::string handleRequest(const std::string& method,
                             const std::string& path,
                             const std::map<std::string, std::string>& headers,
                             const std::string& body);

    enum class ChannelPermission {
        ManageSettings,
        PostMessage,
        BanUser,
        ManageAdmins,
        ManageRequests,
        ManageReactions,
        PinMessage
    };

    // WebSocket bridge for notifying peers about message actions
    void setWebSocketSender(std::function<void(const std::string&, const std::string&)> sender);

    // Utility exposed for signaling handlers (WS forwarders)
    std::string base64Decode(const std::string& encoded);

    // Signaling storage helpers (reuse HTTP logic for WS forwarders)
    void storeCallOffer(const std::string& caller_id, const std::string& receiver_id, const std::string& offer_json);
    void storeCallAnswer(const std::string& caller_id, const std::string& receiver_id, const std::string& answer_json);
    void storeCallIce(const std::string& sender_id, const std::string& receiver_id, const std::string& candidate_json);
    
private:
    DatabaseManager& db_manager_;
    AuthManager& auth_manager_;
    AdminHandler admin_handler_;
    FcmClient fcm_client_;
    RuStoreClient rustore_client_;
    
    // Route handlers
    std::string handleGet(const std::string& path, const std::map<std::string, std::string>& headers);
    std::string handlePost(const std::string& path, const std::string& body, const std::map<std::string, std::string>& headers = std::map<std::string, std::string>());
    std::string handleOptions(const std::string& path, const std::map<std::string, std::string>& headers);
    
    // API endpoints
    std::string handleRegister(const std::string& body);
    std::string handleLogin(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleLogout(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleRefreshSession(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleGetSessions(const std::string& body);
    std::string handleRevokeSession(const std::string& body);
    std::string handleRevokeSelectedSessions(const std::string& body);
    std::string handleRevokeOtherSessions(const std::string& body);
    std::string handleCheckUsername(const std::string& body);
    std::string handleGetFriends(const std::string& body);
    std::string handleGetChats(const std::string& body);
    std::string handleGetChatPins(const std::string& body);
    std::string handlePinChat(const std::string& body);
    std::string handleUnpinChat(const std::string& body);
    std::string handleGetChatFolders(const std::string& body);
    std::string handleSetChatFolders(const std::string& body);
    std::string handleSetPremium(const std::string& body);
    std::string handleCreatePremiumPayment(const std::string& body);
    std::string handleCreatePremiumGiftPayment(const std::string& body);
    std::string handleYooMoneyNotification(const std::string& body);
    std::string handleStripeWebhook(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleSetPersonalChannel(const std::string& body);
    std::string handleGetMessages(const std::string& body);
    std::string handleSendMessage(const std::string& body);
    std::string handleRegisterPushToken(const std::string& body);
    std::string handleDeletePushToken(const std::string& body);
    std::string handleDeleteMessage(const std::string& body);
    std::string handleDeleteChannelMessage(const std::string& body);
    std::string handleDeleteGroupMessage(const std::string& body);
    std::string handleCreateReport(const std::string& body);
    std::string handleFriendRequest(const std::string& body);
    std::string handleAcceptFriend(const std::string& body);
    std::string handleRejectFriend(const std::string& body);
    std::string handleGetFriendRequests(const std::string& body);
    std::string handleFindUser(const std::string& body);
    std::string handleResolveUsername(const std::string& body);
    std::string handleBotCallbackQuery(const std::string& body);
    std::string handleValidateToken(const std::string& body);
    std::string handleAiProxyModels(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleAiProxyChat(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleAiOauthExchange(const std::string& body, const std::map<std::string, std::string>& headers);
    // Profile/Privacy
    std::string handleGetUserProfile(const std::string& body);
    std::string handleUpdateMyProfile(const std::string& body);
    std::string handleUpdateMyPrivacy(const std::string& body);
    std::string handleSetBusinessHours(const std::string& body);
    std::string handleUpdateActivity(const std::string& body);
    std::string handleUserStatus(const std::string& body);
    std::string handleGetTurnConfig(const std::string& body);
    std::string handleGetSfuConfig(const std::string& body);
    std::string handleGetSfuRoom(const std::string& body);
    std::string handleUploadFile(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleUploadVoice(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleUploadAvatar(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleUploadChannelAvatar(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleGetFile(const std::string& path);
    std::string handleGetAvatar(const std::string& path);

    bool sendPremiumGiftMessage(const PremiumPayment& payment);
    
    // Call handlers
    std::string handleCallNotification(const std::string& body);
    std::string handleCallResponse(const std::string& body);
    std::string handleGroupCallNotification(const std::string& body);
    std::string handleCallOffer(const std::string& body);
    std::string handleCallAnswer(const std::string& body);
    std::string handleCallIce(const std::string& body);
    std::string handleCallEnd(const std::string& body);
    std::string handleCheckIncomingCalls(const std::string& body);
    std::string handleCheckCallResponse(const std::string& body);
    std::string handleGetCallOffer(const std::string& body);
    std::string handleGetCallAnswer(const std::string& body);
    std::string handleGetCallIce(const std::string& body);
    
    // In-memory storage for active calls (key: receiver_id, value: call info)
    // Also stores call responses (key: caller_id_receiver_id, value: response)
    static std::map<std::string, std::map<std::string, std::string>> active_calls_;
    static std::map<std::string, std::string> call_responses_; // key: "caller_id_receiver_id", value: "accepted"/"rejected"
    // Storage for WebRTC signaling data
    static std::map<std::string, std::string> call_offers_; // key: "caller_id_receiver_id", value: offer JSON
    static std::map<std::string, std::string> call_answers_; // key: "caller_id_receiver_id", value: answer JSON
    static std::map<std::string, std::vector<std::string>> call_ice_candidates_; // key: "caller_id_receiver_id", value: vector of ICE candidate JSONs
    static std::map<std::string, int64_t> call_signaling_timestamps_; // key: "caller_id_receiver_id", value: last update timestamp
    static std::mutex calls_mutex_;
    
    // Group handlers
    std::string handleCreateGroup(const std::string& body);
    std::string handleGetGroups(const std::string& body);
    std::string handleGetGroupMembers(const std::string& body);
    std::string handleGetGroupMessages(const std::string& body);
    std::string handleSendGroupMessage(const std::string& body);
    std::string handleCreateGroupInvite(const std::string& body);
    std::string handleJoinGroup(const std::string& body);
    std::string handleUpdateGroupName(const std::string& body);
    std::string handleUpdateGroupDescription(const std::string& body);
    std::string handleLeaveGroup(const std::string& body);
    std::string handlePinMessage(const std::string& body);
    std::string handleUnpinMessage(const std::string& body);
    std::string handleBanMember(const std::string& body);
    std::string handleMuteMember(const std::string& body);
    std::string handleKickMember(const std::string& body);
    std::string handleAddFriendToGroup(const std::string& body);
    std::string handleSetAdminPermissions(const std::string& body);
    std::string handleDeleteGroup(const std::string& body);
    std::string handleSearchGroupMessages(const std::string& body);
    std::string handleUpdateGroupPermissions(const std::string& body);
    std::string handleForwardMessage(const std::string& body);
    
    // Channel handlers
    std::string handleCreateChannel(const std::string& body);
    std::string handleGetChannels(const std::string& body);
    std::string handlePublicDirectory(const std::string& body);
    std::string handleRequestVerification(const std::string& body);
    std::string handleGetVerificationRequests(const std::string& body);
    std::string handleReviewVerification(const std::string& body);
    std::string handleGetMyVerificationRequests(const std::string& body);
    std::string handleSetGroupPublic(const std::string& body);
    std::string handleSetChannelPublic(const std::string& body);
    std::string handleGetChannelMessages(const std::string& body);
    std::string handleSendChannelMessage(const std::string& body);
    std::string handleReadChannel(const std::string& body);
    std::string handleSubscribeChannel(const std::string& body);
    std::string handleCreateChannelInvite(const std::string& body);
    std::string handleRevokeChannelInvite(const std::string& body);
    std::string handleJoinChannelByInvite(const std::string& body);
    std::string handleAddMessageReaction(const std::string& body);
    std::string handleRemoveMessageReaction(const std::string& body);
    std::string handleGetMessageReactions(const std::string& body);
    std::string handleAddMessageView(const std::string& body);
    std::string handleCreatePoll(const std::string& body);
    std::string handleVotePoll(const std::string& body);
    std::string handleGetPoll(const std::string& body);
    std::string handleUpdateChannelName(const std::string& body);
    std::string handleUpdateChannelDescription(const std::string& body);
    std::string handleSetChannelCustomLink(const std::string& body);
    std::string handleSetChannelPrivacy(const std::string& body);
    std::string handleSetChannelShowAuthor(const std::string& body);
    std::string handleAddAllowedReaction(const std::string& body);
    std::string handleRemoveAllowedReaction(const std::string& body);
    std::string handleGetChannelAllowedReactions(const std::string& body);
    std::string handleGetChannelJoinRequests(const std::string& body);
    std::string handleAcceptChannelJoinRequest(const std::string& body);
    std::string handleRejectChannelJoinRequest(const std::string& body);
    std::string handleSetChannelAdminPermissions(const std::string& body);
    std::string handleBanChannelMember(const std::string& body);
    std::string handleGetChannelMembers(const std::string& body);
    std::string handleSearchChannel(const std::string& body);
    std::string handleChannelUrl(const std::string& path);
    std::string handleGetChannelInfo(const std::string& body);
    std::string handleUnsubscribeChannel(const std::string& body);
    std::string handleDeleteChannel(const std::string& body);
    std::string handleLinkChannelDiscussion(const std::string& body);
    
    // Marketplace handlers
    std::string handleCreateStoreItem(const std::string& body);
    std::string handleEditStoreItem(const std::string& body);
    std::string handleDeleteStoreItem(const std::string& body);
    std::string handleGetStoreItems(const std::string& body);
    std::string handlePurchaseProduct(const std::string& body);
    
    // Integration handlers
    std::string handleCreateIntegration(const std::string& body);
    std::string handleGetIntegrations(const std::string& body);
    std::string handleDeleteIntegration(const std::string& body);
    std::string handleOAuthCallback(const std::string& path, const std::string& body);

    std::string buildHttpJson(const std::string& body,
                              const std::map<std::string, std::string>& extra_headers = {});
    std::string extractSessionTokenFromHeaders(const std::map<std::string, std::string>& headers);
    std::string maybeInjectSessionToken(const std::string& body, const std::map<std::string, std::string>& headers);
    bool isSecureRequest(const std::map<std::string, std::string>& headers) const;
    
    // Bot Builder handlers
    std::string handleCreateBot(const std::string& body);
    std::string handleUpdateBotProfile(const std::string& body);
    std::string handleUpdateBotFlow(const std::string& body);
    std::string handleGetUserBots(const std::string& body);
    std::string handleDeployBot(const std::string& body);
    std::string handleDeleteBot(const std::string& body);
    std::string handleRevealBotToken(const std::string& body);
    std::string handleGetBotScript(const std::string& body);
    std::string handleUpdateBotScript(const std::string& body);
    std::string handleUploadBotAvatar(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleAddBotDeveloper(const std::string& body);
    std::string handleRemoveBotDeveloper(const std::string& body);
    std::string handleGetBotDevelopers(const std::string& body);
    
    // Bot files (IDE)
    std::string handleListBotFiles(const std::string& body);
    std::string handleGetBotFile(const std::string& body);
    std::string handleSaveBotFile(const std::string& body);
    std::string handleDeleteBotFile(const std::string& body);
    std::string handleBuildBotProject(const std::string& body);
    
    // Live Console handlers
    std::string handleGetBotLogs(const std::string& body);
    std::string handleReplayWebhook(const std::string& body);
    
    // Event Router handlers
    std::string handleCreateTriggerRule(const std::string& body);
    std::string handleUpdateTriggerRule(const std::string& body);
    std::string handleDeleteTriggerRule(const std::string& body);
    std::string handleGetTriggerRules(const std::string& body);
    std::string handleCreateEventWebhook(const std::string& body);
    
    // E2EE (End-to-End Encryption) handlers
    std::string handleE2EERegisterKey(const std::string& body);
    std::string handleE2EEGetPublicKey(const std::string& body);
    std::string handleE2EEGetPublicKeys(const std::string& body);
    std::string handleE2EESendMessage(const std::string& body);
    std::string handleE2EEStatus(const std::string& body);
    
    // Group Topics (Forum mode) handlers
    std::string handleSetGroupForumMode(const std::string& body);
    std::string handleCreateGroupTopic(const std::string& body);
    std::string handleUpdateGroupTopic(const std::string& body);
    std::string handleDeleteGroupTopic(const std::string& body);
    std::string handleGetGroupTopics(const std::string& body);
    std::string handlePinGroupTopic(const std::string& body);
    std::string handleUnpinGroupTopic(const std::string& body);
    std::string handleGetTopicMessages(const std::string& body);
    std::string handleSendTopicMessage(const std::string& body);
    std::string handleHideGeneralTopic(const std::string& body);
    std::string handleToggleTopicClosed(const std::string& body);
    std::string handleReorderPinnedTopics(const std::string& body);
    std::string handleSearchTopics(const std::string& body);
    std::string handleSetTopicNotifications(const std::string& body);
    
    // Wallet API handlers
    std::string handleWalletRegister(const std::string& body);
    std::string handleWalletGetAddress(const std::string& body);
    std::string handleWalletBalance(const std::string& body);
    std::string handleWalletRelayTx(const std::string& body);
    std::string handleWalletHistory(const std::string& body);
    std::string handleWalletSendToUser(const std::string& body);
    std::string handleWalletTokens(const std::string& query);
    std::string handleWalletPrices(const std::string& query);
    
    // Wallet sync handlers (server-side wallet storage)
    std::string handleWalletSave(const std::string& body);
    std::string handleWalletGet(const std::string& body);
    std::string handleWalletUpdate(const std::string& body);
    std::string handleWalletDelete(const std::string& body);
    
    // P2P Marketplace handlers
    std::string handleP2POffers(const std::string& body);
    std::string handleP2PCreateOffer(const std::string& body);
    std::string handleP2PStartTrade(const std::string& body);
    std::string handleP2PMyTrades(const std::string& body);
    std::string handleP2PUpdateTrade(const std::string& body);
    
    // Vouchers/Checks handlers
    std::string handleVoucherCreate(const std::string& body);
    std::string handleVoucherClaim(const std::string& body);
    std::string handleVouchersMy(const std::string& body);
    
    // Staking/Earn handlers
    std::string handleStakingProducts(const std::string& query);
    std::string handleStakingStake(const std::string& body);
    std::string handleStakingMyStakes(const std::string& body);
    std::string handleStakingUnstake(const std::string& body);
    
    // NFT handlers
    std::string handleNFTMy(const std::string& body);
    std::string handleNFTRefresh(const std::string& body);
    
    // Security settings handlers
    std::string handleSecurityGet(const std::string& body);
    std::string handleSecurityUpdate(const std::string& body);
    
    // Static file serving
    std::string serveStaticFile(const std::string& path);
    std::string serveStaticFileWithStatus(const std::string& path, const std::string& status_line);
    std::string getMimeType(const std::string& extension);
    
    // Bot API handlers (Telegram Bot API format: /bot<token>/method_name)
    std::string handleBotApiRequest(const std::string& path, const std::string& method,
                                   const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleBotApiGetMe(const InMemoryStorage::Bot& bot);
    std::string handleBotApiGetUpdates(const std::string& bot_token, const std::string& body,
                                      const std::map<std::string, std::string>& query_params = std::map<std::string, std::string>());
    std::string handleBotApiSetWebhook(const std::string& bot_token, const std::string& body);
    std::string handleBotApiDeleteWebhook(const std::string& bot_token, const std::string& body);
    std::string handleBotApiGetWebhookInfo(const std::string& bot_token);
    std::string handleBotApiSendMessage(const std::string& bot_token, const std::string& body);
    std::string handleBotApiEditMessageText(const std::string& bot_token, const std::string& body);
    std::string handleBotApiDeleteMessage(const std::string& bot_token, const std::string& body);
    std::string handleBotApiForwardMessage(const std::string& bot_token, const std::string& body);
    std::string handleBotApiCopyMessage(const std::string& bot_token, const std::string& body);
    std::string handleBotApiSendPhoto(const std::string& bot_token, const std::string& body);
    std::string handleBotApiSendAudio(const std::string& bot_token, const std::string& body);
    std::string handleBotApiSendVideo(const std::string& bot_token, const std::string& body);
    std::string handleBotApiSendDocument(const std::string& bot_token, const std::string& body);
    std::string handleBotApiSendVoice(const std::string& bot_token, const std::string& body);
    
    // Utility
    const std::string* findHeaderValueCI(const std::map<std::string, std::string>& headers,
                                         const std::string& name) const;
    std::string extractTokenFromHeaders(const std::map<std::string, std::string>& headers);
    std::map<std::string, std::string> parseFormData(const std::string& body);
    std::string handleAdminGetReports(const std::string& body, const std::map<std::string, std::string>& headers);
    std::string handleAdminUpdateReport(const std::string& body, const std::map<std::string, std::string>& headers);
    bool deleteMessageForModeration(const std::string& message_id,
                                    const std::string& message_type,
                                    std::string* out_type = nullptr);

    bool checkChannelPermission(const std::string& user_id,
                                const std::string& channel_id,
                                ChannelPermission permission,
                                DatabaseManager::ChannelMember* out_member = nullptr,
                                std::string* error_message = nullptr);

    // Advanced channels/supergroups (Phase 2)
    bool checkPermission(const std::string& user_id, const std::string& chat_id, uint64_t required_mask, std::string* error = nullptr);
    bool enforceSlowMode(const std::string& chat_id, const std::string& user_id, int slow_mode_sec, int limit_per_window, std::string* error = nullptr);
    bool checkRestriction(const std::string& chat_id, const std::string& user_id, bool want_media, bool want_links, bool want_invite, std::string* error = nullptr);
    
    // In-memory fallback for rate-limit (used if Redis is not wired)
    static std::unordered_map<std::string, std::vector<int64_t>> slow_mode_buckets_;
    static std::mutex slow_mode_mutex_;

    std::function<void(const std::string&, const std::string&)> ws_sender_;
    RateLimiter report_rate_limiter_;
};

} // namespace xipher

#endif // REQUEST_HANDLER_HPP
