#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include "../include/database/db_manager.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace xipher {

static std::string normalizeHandle(std::string s) {
    // trim spaces
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) s.pop_back();
    if (!s.empty() && s[0] == '@') s = s.substr(1);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string RequestHandler::handleResolveUsername(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data.count("token") ? data["token"] : "";
    std::string username = data.count("username") ? data["username"] : "";

    if (token.empty() || username.empty()) {
        return JsonParser::createErrorResponse("token and username required");
    }

    std::string requester_id = auth_manager_.getUserIdFromToken(token);
    if (requester_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    const std::string handle = normalizeHandle(username);
    if (handle.empty()) {
        return JsonParser::createErrorResponse("Invalid username");
    }

    // 1) Try channel by username (v2 unified)
    Chat chat = db_manager_.getChatByUsernameV2(handle);
    if (!chat.id.empty() && chat.type == "channel") {
        std::map<std::string, std::string> out;
        out["type"] = "channel";
        out["id"] = chat.id;
        out["title"] = chat.title;
        out["username"] = chat.username;
        return JsonParser::createSuccessResponse("OK", out);
    }

    // 2) Try legacy channel custom link
    auto legacy = db_manager_.getChannelByCustomLink(handle);
    if (!legacy.id.empty()) {
        std::map<std::string, std::string> out;
        out["type"] = "channel";
        out["id"] = legacy.id;
        out["title"] = legacy.name;
        out["username"] = legacy.custom_link;
        return JsonParser::createSuccessResponse("OK", out);
    }

    // 3) Try user/bot
    User u = db_manager_.getUserByUsername(handle);
    if (!u.id.empty()) {
        std::string avatar_url;
        std::string uname;
        db_manager_.getUserPublic(u.id, uname, avatar_url);
        const bool is_friend = (u.id == requester_id) ? true : db_manager_.areFriends(requester_id, u.id);

        std::map<std::string, std::string> out;
        out["type"] = u.is_bot ? "bot" : "user";
        out["id"] = u.id;
        out["username"] = u.username;
        out["avatar_url"] = avatar_url;
        out["is_friend"] = is_friend ? "true" : "false";
        return JsonParser::createSuccessResponse("OK", out);
    }

    return JsonParser::createErrorResponse("Not found");
}

std::string RequestHandler::handleGetFriends(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto friends = db_manager_.getFriends(user_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"friends\":[";
    
    bool first = true;
    for (const auto& friend_ : friends) {
        if (!first) oss << ",";
        first = false;
        bool is_online = db_manager_.isUserOnline(friend_.id, 300);  // 5 minutes threshold
        std::string last_activity = db_manager_.getUserLastActivity(friend_.id);
        oss << "{\"id\":\"" << JsonParser::escapeJson(friend_.id) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(friend_.username) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(friend_.created_at) << "\","
            << "\"online\":" << (is_online ? "true" : "false") << ","
            << "\"last_activity\":\"" << JsonParser::escapeJson(last_activity) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetChats(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto friends = db_manager_.getFriends(user_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"chats\":[";
    
    bool first = true;
    for (const auto& friend_ : friends) {
        if (!first) oss << ",";
        first = false;
        
        // Получаем последнее сообщение
        auto lastMsg = db_manager_.getLastMessage(user_id, friend_.id);
        int unread = db_manager_.getUnreadCount(user_id, friend_.id);
        
        // Форматируем время
        std::string time = "Нет сообщений";
        if (!lastMsg.id.empty()) {
            // Парсим время из PostgreSQL формата
            std::string created_at = lastMsg.created_at;
            if (created_at.length() >= 16) {
                time = created_at.substr(11, 5); // HH:MM
            }
        }
        
        bool is_online = db_manager_.isUserOnline(friend_.id, 300);  // 5 minutes threshold
        std::string last_activity = db_manager_.getUserLastActivity(friend_.id);
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(friend_.id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(friend_.username) << "\","
            << "\"avatar\":\"" << friend_.username.substr(0, 1).c_str() << "\","
            << "\"lastMessage\":\"" << JsonParser::escapeJson(lastMsg.content) << "\","
            << "\"time\":\"" << time << "\","
            << "\"unread\":" << unread << ","
            << "\"online\":" << (is_online ? "true" : "false") << ","
            << "\"last_activity\":\"" << JsonParser::escapeJson(last_activity) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetMessages(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string friend_id = data["friend_id"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    if (friend_id.empty()) {
        return JsonParser::createErrorResponse("Friend ID required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем, что они друзья
    if (!db_manager_.areFriends(user_id, friend_id)) {
        return JsonParser::createErrorResponse("Not friends");
    }
    
    // Получаем сообщения
    auto messages = db_manager_.getMessages(user_id, friend_id, 50);

    if (user_id != friend_id) {
        db_manager_.markMessagesAsDelivered(user_id, friend_id);
    }
    // Помечаем как прочитанные (чтобы очищать счетчики)
    db_manager_.markMessagesAsRead(user_id, friend_id);

    bool peer_allows_read_receipts = true;
    if (user_id != friend_id) {
        UserPrivacySettings peer_privacy;
        if (db_manager_.getUserPrivacy(friend_id, peer_privacy)) {
            peer_allows_read_receipts = peer_privacy.send_read_receipts;
        }
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"messages\":[";
    
    bool first = true;
    for (const auto& msg : messages) {
        if (!first) oss << ",";
        first = false;
        
        std::string time = msg.created_at;
        if (time.length() >= 16) {
            time = time.substr(11, 5); // HH:MM
        }
        
        bool isSent = (msg.sender_id == user_id);
        bool canExposeRead = !isSent || peer_allows_read_receipts;
        std::string status;
        if (isSent) {
            if (msg.is_read && peer_allows_read_receipts) {
                status = "read";
            } else if (msg.is_delivered) {
                status = "delivered";
            } else {
                status = "sent";
            }
        }

        oss << "{\"id\":\"" << JsonParser::escapeJson(msg.id) << "\","
            << "\"sender_id\":\"" << JsonParser::escapeJson(msg.sender_id) << "\","
            << "\"content\":\"" << JsonParser::escapeJson(msg.content) << "\","
            << "\"time\":\"" << time << "\","
            << "\"sent\":" << (isSent ? "true" : "false") << ","
            << "\"status\":\"" << JsonParser::escapeJson(status) << "\","
            << "\"is_read\":" << ((canExposeRead && msg.is_read) ? "true" : "false") << ","
            << "\"is_delivered\":" << (msg.is_delivered ? "true" : "false") << "}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSendMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    std::string content = data["content"];
    std::string temp_id = data.count("temp_id") ? data["temp_id"] : "";
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    if (receiver_id.empty() || content.empty()) {
        return JsonParser::createErrorResponse("Receiver ID and content required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Разрешаем отправку в «Избранные» (к самому себе) без проверки дружбы
    const bool is_saved_messages = sender_id == receiver_id;
    if (!is_saved_messages) {
        // Проверяем, что они друзья для обычных чатов
        if (!db_manager_.areFriends(sender_id, receiver_id)) {
            return JsonParser::createErrorResponse("Not friends");
        }
    }
    
    // Обновляем активность отправителя
    db_manager_.updateLastActivity(sender_id);
    
    // Отправляем сообщение
    if (db_manager_.sendMessage(sender_id, receiver_id, content)) {
        Message lastMessage = db_manager_.getLastMessage(sender_id, receiver_id);
        if (ws_sender_ && !lastMessage.id.empty()) {
            const bool is_saved_messages = sender_id == receiver_id;
            const std::string chat_type = is_saved_messages ? "saved_messages" : "chat";
            const std::string time = lastMessage.created_at.length() >= 16 ? lastMessage.created_at.substr(11, 5) : "";
            const std::string status = lastMessage.is_read ? "read" : (lastMessage.is_delivered ? "delivered" : "sent");
            auto send_ws_payload = [&](const std::string& target_user_id, const std::string& chat_id) {
                if (target_user_id.empty()) return;
                std::ostringstream ws_payload;
                ws_payload << "{\"type\":\"new_message\","
                           << "\"chat_type\":\"" << chat_type << "\","
                           << "\"chat_id\":\"" << JsonParser::escapeJson(chat_id) << "\","
                           << "\"id\":\"" << JsonParser::escapeJson(lastMessage.id) << "\","
                           << "\"message_id\":\"" << JsonParser::escapeJson(lastMessage.id) << "\","
                           << "\"temp_id\":\"" << JsonParser::escapeJson(temp_id) << "\","
                           << "\"sender_id\":\"" << JsonParser::escapeJson(sender_id) << "\","
                           << "\"receiver_id\":\"" << JsonParser::escapeJson(receiver_id) << "\","
                           << "\"content\":\"" << JsonParser::escapeJson(lastMessage.content) << "\","
                           << "\"message_type\":\"" << JsonParser::escapeJson(lastMessage.message_type) << "\","
                           << "\"file_path\":\"" << JsonParser::escapeJson(lastMessage.file_path) << "\","
                           << "\"file_name\":\"" << JsonParser::escapeJson(lastMessage.file_name) << "\","
                           << "\"file_size\":" << lastMessage.file_size << ","
                           << "\"reply_to_message_id\":\"" << JsonParser::escapeJson(lastMessage.reply_to_message_id) << "\","
                           << "\"created_at\":\"" << JsonParser::escapeJson(lastMessage.created_at) << "\","
                           << "\"time\":\"" << JsonParser::escapeJson(time) << "\","
                           << "\"status\":\"" << JsonParser::escapeJson(status) << "\","
                           << "\"is_read\":" << (lastMessage.is_read ? "true" : "false") << ","
                           << "\"is_delivered\":" << (lastMessage.is_delivered ? "true" : "false") << "}";
                ws_sender_(target_user_id, ws_payload.str());
            };

            if (is_saved_messages) {
                send_ws_payload(sender_id, sender_id);
            } else {
                send_ws_payload(receiver_id, sender_id);
                send_ws_payload(sender_id, receiver_id);
            }
        }

        // If user is messaging a bot user, auto-reply (minimal TG-like interaction).
        auto bot = db_manager_.getBotBuilderBotByUserId(receiver_id);
        if (!bot.id.empty() && bot.is_active) {
            std::string reply = "Я бот \"" + bot.bot_name + "\".\n\n"
                                "Пока я умею только отвечать этим сообщением. "
                                "Подключи Bot API и логику — и я оживу 🙂";
            db_manager_.sendMessage(receiver_id, sender_id, reply);
        }
        return JsonParser::createSuccessResponse("Message sent");
    } else {
        return JsonParser::createErrorResponse("Failed to send message");
    }
}

std::string RequestHandler::handleDeleteMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];

    if (token.empty() || message_id.empty()) {
        return JsonParser::createErrorResponse("Token and message_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto msg = db_manager_.getMessageById(message_id);
    if (msg.id.empty()) {
        return JsonParser::createErrorResponse("Message not found");
    }

    if (msg.sender_id != user_id) {
        return JsonParser::createErrorResponse("Permission denied");
    }

    if (!db_manager_.deleteMessageById(message_id)) {
        return JsonParser::createErrorResponse("Failed to delete message");
    }

    // Уведомляем второго участника через WebSocket, если есть
    if (ws_sender_) {
        const std::string peer_id = (msg.sender_id == user_id) ? msg.receiver_id : msg.sender_id;
        if (!peer_id.empty()) {
            std::string payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                "\",\"chat_type\":\"direct\",\"chat_id\":\"" + JsonParser::escapeJson(peer_id) + "\"}";
            ws_sender_(peer_id, payload);
        }
    }

    return JsonParser::createSuccessResponse("Message deleted");
}

std::string RequestHandler::handleFriendRequest(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string username = data["username"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    if (username.empty()) {
        return JsonParser::createErrorResponse("Username required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Получаем пользователя по username
    auto user = db_manager_.getUserByUsername(username);
    if (user.id.empty()) {
        return JsonParser::createErrorResponse("User not found");
    }
    
    if (user.id == sender_id) {
        return JsonParser::createErrorResponse("Cannot send request to yourself");
    }
    
    // Проверяем, не друзья ли уже
    if (db_manager_.areFriends(sender_id, user.id)) {
        return JsonParser::createErrorResponse("Already friends");
    }
    
    // Создаем запрос
    if (db_manager_.createFriendRequest(sender_id, user.id)) {
        return JsonParser::createSuccessResponse("Friend request sent");
    } else {
        return JsonParser::createErrorResponse("Failed to send request or request already exists");
    }
}

std::string RequestHandler::handleAcceptFriend(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string request_id = data["request_id"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    if (request_id.empty()) {
        return JsonParser::createErrorResponse("Request ID required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Security: BOLA fix - Verify that this friend request belongs to the current user
    auto friend_request = db_manager_.getFriendRequestById(request_id);
    if (friend_request.receiver_id.empty()) {
        return JsonParser::createErrorResponse("Friend request not found");
    }
    if (friend_request.receiver_id != user_id) {
        Logger::getInstance().warning("BOLA attempt: User " + user_id + " tried to accept friend request " + request_id + " belonging to " + friend_request.receiver_id);
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    // Accept the request
    if (db_manager_.acceptFriendRequest(request_id)) {
        return JsonParser::createSuccessResponse("Friend request accepted");
    } else {
        return JsonParser::createErrorResponse("Failed to accept request");
    }
}

} // namespace xipher
