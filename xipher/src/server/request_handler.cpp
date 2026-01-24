#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include "../include/bots/lite_bot_runtime.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <random>
#include <mutex>
#include <boost/filesystem.hpp>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <vector>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <curl/curl.h>
#include <unordered_set>
namespace fs = boost::filesystem;

namespace {

std::string htmlEscape(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"': output += "&quot;"; break;
            case '\'': output += "&#39;"; break;
            default: output += ch; break;
        }
    }
    return output;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::string urlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '+') {
            result.push_back(' ');
            continue;
        }
        if (c == '%' && i + 2 < value.size()) {
            int high = hexValue(value[i + 1]);
            int low = hexValue(value[i + 2]);
            if (high >= 0 && low >= 0) {
                result.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        result.push_back(c);
    }
    return result;
}

std::string toLowerCopy(std::string input) {
    std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return input;
}

std::string toUpperCopy(std::string input) {
    std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return input;
}

std::string trimString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }
    return value.substr(start, end - start);
}

std::string hashSessionToken(const std::string& token) {
    if (token.empty()) {
        return "";
    }
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(), hash);
    std::ostringstream oss;
    for (unsigned char b : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

std::string extractJsonValueForKey(const std::string& body, const std::string& key) {
    if (body.empty()) return "";
    const std::string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
        pos++;
    }
    if (pos >= body.size()) return "";

    const char first = body[pos];
    if (first == '"') {
        std::string raw;
        bool escape = false;
        for (size_t i = pos + 1; i < body.size(); i++) {
            char c = body[i];
            if (escape) {
                raw.push_back(c);
                escape = false;
                continue;
            }
            if (c == '\\') {
                raw.push_back(c);
                escape = true;
                continue;
            }
            if (c == '"') {
                return xipher::JsonParser::unescapeJson(raw);
            }
            raw.push_back(c);
        }
        return "";
    }

    if (first == '{' || first == '[') {
        std::vector<char> stack;
        bool inString = false;
        bool escape = false;
        for (size_t i = pos; i < body.size(); i++) {
            char c = body[i];
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == '"') {
                inString = !inString;
                continue;
            }
            if (inString) continue;

            if (c == '{' || c == '[') {
                stack.push_back(c);
            } else if (c == '}' || c == ']') {
                if (!stack.empty()) {
                    char open = stack.back();
                    if ((open == '{' && c == '}') || (open == '[' && c == ']')) {
                        stack.pop_back();
                        if (stack.empty()) {
                            return body.substr(pos, i - pos + 1);
                        }
                    }
                }
            }
        }
        return "";
    }

    size_t end = pos;
    while (end < body.size() && body[end] != ',' && body[end] != '}' && !std::isspace(static_cast<unsigned char>(body[end]))) {
        end++;
    }
    return body.substr(pos, end - pos);
}

std::string sha1Hex(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char byte : hash) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

// Security: Validate filename to prevent path traversal attacks
bool isValidFilename(const std::string& filename) {
    if (filename.empty() || filename.length() > 255) {
        return false;
    }
    // Block path traversal attempts
    if (filename.find("..") != std::string::npos) {
        return false;
    }
    // Block directory separators
    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
        return false;
    }
    // Block null bytes
    if (filename.find('\0') != std::string::npos) {
        return false;
    }
    // Block hidden files
    if (filename[0] == '.') {
        return false;
    }
    return true;
}

// Security: Validate URL to prevent SSRF attacks
bool isValidWebhookUrl(const std::string& url) {
    if (url.empty() || url.length() > 2048) {
        return false;
    }
    // Must start with https:// (http:// not allowed for webhooks)
    if (url.find("https://") != 0) {
        return false;
    }
    // Extract hostname
    size_t host_start = 8; // After "https://"
    size_t host_end = url.find('/', host_start);
    if (host_end == std::string::npos) {
        host_end = url.length();
    }
    size_t port_pos = url.find(':', host_start);
    if (port_pos != std::string::npos && port_pos < host_end) {
        host_end = port_pos;
    }
    std::string hostname = url.substr(host_start, host_end - host_start);
    
    // Block internal/private IPs and hostnames
    if (hostname == "localhost" || hostname == "127.0.0.1" || hostname == "::1") {
        return false;
    }
    // Block private IP ranges
    if (hostname.find("10.") == 0 || hostname.find("192.168.") == 0) {
        return false;
    }
    if (hostname.find("172.") == 0) {
        // Check 172.16.0.0 - 172.31.255.255
        try {
            int second_octet = std::stoi(hostname.substr(4, hostname.find('.', 4) - 4));
            if (second_octet >= 16 && second_octet <= 31) {
                return false;
            }
        } catch (...) {}
    }
    // Block link-local
    if (hostname.find("169.254.") == 0) {
        return false;
    }
    // Block cloud metadata endpoints
    if (hostname == "169.254.169.254" || hostname == "metadata.google.internal") {
        return false;
    }
    // Block internal hostnames
    if (hostname.find(".internal") != std::string::npos || 
        hostname.find(".local") != std::string::npos) {
        return false;
    }
    return true;
}

// Security: Allowed file extensions for uploads
const std::unordered_set<std::string> kAllowedFileExtensions = {
    ".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".ico", ".svg",
    ".mp4", ".webm", ".mov", ".avi", ".mkv", ".m4v",
    ".mp3", ".wav", ".ogg", ".m4a", ".aac", ".flac", ".opus", ".weba",
    ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".txt", ".rtf",
    ".zip", ".rar", ".7z", ".tar", ".gz",
    ".json", ".xml", ".csv"
};

bool isAllowedFileExtension(const std::string& extension) {
    std::string ext_lower = extension;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
    return kAllowedFileExtensions.count(ext_lower) > 0;
}

std::string statusReasonPhrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

std::string buildHttpResponse(int status, const std::string& body, const std::string& content_type) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << statusReasonPhrase(status) << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

bool shouldDeletePushToken(const std::string& error_code) {
    if (error_code.empty()) return false;
    return error_code == "UNREGISTERED"
        || error_code == "SENDER_ID_MISMATCH"
        || error_code == "INVALID_ARGUMENT"
        || error_code == "NOT_FOUND";
}

bool isRuStorePlatform(const std::string& platform) {
    if (platform.empty()) return false;
    std::string lower = toLowerCopy(platform);
    return lower == "rustore" || lower == "ru_store";
}

struct PushPlatformSummary {
    bool has_fcm = false;
    bool has_rustore = false;
};

PushPlatformSummary summarizePushPlatforms(const std::vector<xipher::PushTokenInfo>& tokens) {
    PushPlatformSummary summary;
    for (const auto& token_info : tokens) {
        if (isRuStorePlatform(token_info.platform)) {
            summary.has_rustore = true;
        } else {
            summary.has_fcm = true;
        }
    }
    return summary;
}

void sendPushTokensForUser(xipher::DatabaseManager& db_manager,
                           xipher::FcmClient& fcm_client,
                           xipher::RuStoreClient& rustore_client,
                           const std::string& user_id,
                           const std::vector<xipher::PushTokenInfo>& tokens,
                           const std::string& title,
                           const std::string& body,
                           const std::map<std::string, std::string>& payload,
                           const std::string& channel_id,
                           const std::string& priority,
                           bool fcm_ready,
                           bool rustore_ready) {
    xipher::Logger::getInstance().info("sendPushTokensForUser: user=" + user_id + ", tokens=" + std::to_string(tokens.size()) + ", fcm_ready=" + (fcm_ready ? "true" : "false") + ", rustore_ready=" + (rustore_ready ? "true" : "false"));
    for (const auto& token_info : tokens) {
        if (token_info.device_token.empty()) continue;
        xipher::Logger::getInstance().info("Processing token: platform=" + token_info.platform + ", token_len=" + std::to_string(token_info.device_token.length()));
        if (isRuStorePlatform(token_info.platform)) {
            if (!rustore_ready) {
                xipher::Logger::getInstance().info("Skipping RuStore token: not ready");
                continue;
            }
            xipher::Logger::getInstance().info("Sending via RuStore...");
            std::string error_code;
            bool ok = rustore_client.sendMessage(token_info.device_token, title, body, payload,
                                                 channel_id, &error_code);
            xipher::Logger::getInstance().info("RuStore send result: " + std::string(ok ? "OK" : "FAILED") + ", error=" + error_code);
            if (!ok && shouldDeletePushToken(error_code)) {
                db_manager.deletePushToken(user_id, token_info.device_token);
            }
        } else {
            if (!fcm_ready) continue;
            std::string error_code;
            bool ok = fcm_client.sendMessage(token_info.device_token, title, body, payload,
                                             channel_id, priority, &error_code);
            if (!ok && shouldDeletePushToken(error_code)) {
                db_manager.deletePushToken(user_id, token_info.device_token);
            }
        }
    }
}

constexpr const char* kSessionTokenCookieName = "xipher_token";
const std::string kSessionTokenPlaceholder = "cookie";
constexpr int kSessionTtlSeconds = 60 * 60 * 24 * 30;

std::string extractCookieValue(const std::string& cookie_header, const std::string& name) {
    const std::string needle = name + "=";
    size_t pos = cookie_header.find(needle);
    if (pos == std::string::npos) {
        return "";
    }
    size_t start = pos + needle.size();
    size_t end = cookie_header.find(';', start);
    if (end == std::string::npos) {
        end = cookie_header.size();
    }
    return cookie_header.substr(start, end - start);
}

bool envFlagEnabled(const char* raw) {
    if (!raw) return false;
    std::string value = raw;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

std::string trimEnvValue(std::string value) {
    const char* whitespace = " \t\n\r";
    auto start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return std::string();
    }
    auto end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::vector<std::string> splitEnvList(const std::vector<const char*>& keys) {
    std::vector<std::string> items;
    for (const auto* key : keys) {
        const char* raw = std::getenv(key);
        if (!raw || std::string(raw).empty()) continue;
        std::stringstream ss(raw);
        std::string part;
        while (std::getline(ss, part, ',')) {
            part = trimEnvValue(part);
            if (!part.empty()) {
                items.push_back(part);
            }
        }
        if (!items.empty()) break;
    }
    return items;
}

std::string normalizeAiApiBase(std::string value) {
    value = trimEnvValue(value);
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::vector<std::string> getAllowedAiBases() {
    std::vector<std::string> allowed = splitEnvList({ "XIPHER_AI_ALLOWED_API_BASES" });
    if (allowed.empty()) {
        allowed.push_back("https://api.openai.com/v1");
    }
    for (auto& base : allowed) {
        base = normalizeAiApiBase(base);
    }
    return allowed;
}

bool isAllowedAiBase(const std::string& base) {
    const auto allowed = getAllowedAiBases();
    for (const auto& item : allowed) {
        if (!item.empty() && base == item) {
            return true;
        }
    }
    return false;
}

std::string resolveAiApiBase(const std::string& raw) {
    std::string base = normalizeAiApiBase(raw);
    if (base.empty()) {
        const char* env_base = std::getenv("XIPHER_AI_DEFAULT_API_BASE");
        if (env_base) {
            base = normalizeAiApiBase(env_base);
        }
    }
    if (base.empty()) {
        base = "https://api.openai.com/v1";
    }
    if (!isAllowedAiBase(base)) {
        return "";
    }
    return base;
}

bool ensurePremiumAccessLocal(xipher::DatabaseManager& db_manager, const std::string& user_id,
                              xipher::User& out_user, std::string& error) {
    out_user = db_manager.getUserById(user_id);
    if (out_user.id.empty()) {
        error = "User not found";
        return false;
    }
    if (!out_user.is_admin && !out_user.is_premium) {
        error = "Premium required";
        return false;
    }
    return true;
}

uint64_t fnv1a64(const std::string& input) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : input) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

size_t sfuWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

std::string sfuExtractJsonNumber(const std::string& body, const std::string& key) {
    size_t pos = body.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();
    while (pos < body.size() && (body[pos] == ':' || body[pos] == ' ')) {
        pos++;
    }
    size_t start = pos;
    while (pos < body.size() && std::isdigit(static_cast<unsigned char>(body[pos]))) {
        pos++;
    }
    if (start == pos) return "";
    return body.substr(start, pos - start);
}

bool sfuHttpPostJson(const std::string& url, const std::string& payload, std::string* out_body, long* out_code) {
    if (!out_body) return false;
    out_body->clear();
    if (out_code) *out_code = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sfuWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_body);
    CURLcode res = curl_easy_perform(curl);
    if (out_code) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_code);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool aiHttpPostJson(const std::string& url, const std::string& payload, const std::string& token,
                    std::string* out_body, long* out_code) {
    if (!out_body) return false;
    out_body->clear();
    if (out_code) *out_code = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist* headers = nullptr;
    const std::string auth_header = "Authorization: Bearer " + token;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sfuWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_body);
    CURLcode res = curl_easy_perform(curl);
    if (out_code) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_code);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool aiHttpGet(const std::string& url, const std::string& token, std::string* out_body, long* out_code) {
    if (!out_body) return false;
    out_body->clear();
    if (out_code) *out_code = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist* headers = nullptr;
    const std::string auth_header = "Authorization: Bearer " + token;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sfuWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_body);
    CURLcode res = curl_easy_perform(curl);
    if (out_code) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_code);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool aiHttpPostForm(const std::string& url, const std::string& form_body,
                    std::string* out_body, long* out_code) {
    if (!out_body) return false;
    out_body->clear();
    if (out_code) *out_code = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(form_body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sfuWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_body);
    CURLcode res = curl_easy_perform(curl);
    if (out_code) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_code);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

std::string urlEncode(const std::string& input) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return encoded.str();
}

void appendFormField(std::string& body, const std::string& key, const std::string& value) {
    if (!body.empty()) {
        body += "&";
    }
    body += urlEncode(key);
    body += "=";
    body += urlEncode(value);
}

bool stripePostForm(const std::string& url, const std::string& secret_key,
                    const std::string& form_body, std::string* out_body, long* out_code) {
    if (!out_body) return false;
    out_body->clear();
    if (out_code) *out_code = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist* headers = nullptr;
    const std::string auth_header = "Authorization: Bearer " + secret_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(form_body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sfuWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_body);
    CURLcode res = curl_easy_perform(curl);
    if (out_code) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_code);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool httpGet(const std::string& url, std::string* out_body, long* out_code) {
    if (!out_body) return false;
    out_body->clear();
    if (out_code) *out_code = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sfuWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_body);
    CURLcode res = curl_easy_perform(curl);
    if (out_code) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_code);
    }
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool parseJsonNumberAt(const std::string& body, size_t pos, double& out) {
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
        pos++;
    }
    size_t end = pos;
    while (end < body.size()) {
        char c = body[end];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-') {
            end++;
            continue;
        }
        break;
    }
    if (end == pos) return false;
    try {
        out = std::stod(body.substr(pos, end - pos));
    } catch (...) {
        return false;
    }
    return true;
}

bool extractJsonStringValue(const std::string& body, const std::string& key, std::string& out) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
        pos++;
    }
    if (pos >= body.size() || body[pos] != '"') {
        return false;
    }
    pos++;
    std::string value;
    while (pos < body.size()) {
        char c = body[pos];
        if (c == '\\') {
            if (pos + 1 >= body.size()) break;
            char next = body[pos + 1];
            value.push_back(next);
            pos += 2;
            continue;
        }
        if (c == '"') {
            out = value;
            return true;
        }
        value.push_back(c);
        pos++;
    }
    return false;
}

bool extractJsonIntValue(const std::string& body, const std::string& key, int64_t& out) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;
    double temp = 0.0;
    if (!parseJsonNumberAt(body, pos, temp)) {
        return false;
    }
    out = static_cast<int64_t>(std::llround(temp));
    return true;
}

bool extractCbrRate(const std::string& body, const std::string& code, double& rate) {
    const std::string needle = "\"" + code + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;
    size_t value_pos = body.find("\"Value\"", pos);
    size_t nominal_pos = body.find("\"Nominal\"", pos);
    double value = 0.0;
    double nominal = 1.0;
    if (!parseJsonNumberAt(body, value_pos, value)) {
        return false;
    }
    if (nominal_pos != std::string::npos) {
        if (!parseJsonNumberAt(body, nominal_pos, nominal)) {
            return false;
        }
    }
    if (nominal <= 0.0) return false;
    rate = value / nominal;
    return true;
}

bool getCbrRate(const std::string& code, double& rate) {
    static std::mutex fx_mutex;
    static std::map<std::string, double> cached_rates;
    static int64_t fetched_at = 0;
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    const int64_t ttl_seconds = 6 * 60 * 60;
    const std::string upper = toUpperCopy(code);
    {
        std::lock_guard<std::mutex> lock(fx_mutex);
        auto it = cached_rates.find(upper);
        if (it != cached_rates.end() && (now - fetched_at) < ttl_seconds) {
            rate = it->second;
            return true;
        }
    }
    std::string body;
    long http_code = 0;
    if (!httpGet("https://www.cbr-xml-daily.ru/daily_json.js", &body, &http_code) ||
        http_code < 200 || http_code >= 300) {
        return false;
    }
    double fresh_rate = 0.0;
    if (!extractCbrRate(body, upper, fresh_rate)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(fx_mutex);
        cached_rates[upper] = fresh_rate;
        fetched_at = now;
    }
    rate = fresh_rate;
    return true;
}

int64_t stripeMinimumAmountCents(const std::string& currency) {
    static const std::unordered_map<std::string, int64_t> minimums = {
        {"usd", 50},
        {"eur", 50},
        {"gbp", 50},
        {"aud", 50},
        {"cad", 50},
        {"chf", 50},
        {"dkk", 50},
        {"hkd", 50},
        {"jpy", 50},
        {"mxn", 50},
        {"nok", 50},
        {"nzd", 50},
        {"sek", 50},
        {"sgd", 50}
    };
    const auto it = minimums.find(currency);
    return it == minimums.end() ? 50 : it->second;
}

bool applyStripeMinimum(const std::string& currency, int64_t& cents) {
    const int64_t min_cents = stripeMinimumAmountCents(currency);
    if (cents < min_cents) {
        cents = min_cents;
        return true;
    }
    return false;
}

bool parseAmountToCents(const std::string& raw, int64_t& cents) {
    cents = 0;
    if (raw.empty()) return false;
    std::string normalized = raw;
    std::replace(normalized.begin(), normalized.end(), ',', '.');
    std::string whole = normalized;
    std::string frac;
    size_t dot = normalized.find('.');
    if (dot != std::string::npos) {
        whole = normalized.substr(0, dot);
        frac = normalized.substr(dot + 1);
    }
    if (whole.empty()) whole = "0";
    if (!std::all_of(whole.begin(), whole.end(), ::isdigit)) {
        return false;
    }
    if (!frac.empty() && !std::all_of(frac.begin(), frac.end(), ::isdigit)) {
        return false;
    }
    if (frac.size() > 2) {
        frac = frac.substr(0, 2);
    }
    while (frac.size() < 2) {
        frac.push_back('0');
    }
    int64_t whole_val = 0;
    int64_t frac_val = 0;
    try {
        whole_val = std::stoll(whole);
        if (!frac.empty()) {
            frac_val = std::stoll(frac);
        }
    } catch (...) {
        return false;
    }
    cents = (whole_val * 100) + frac_val;
    return true;
}

bool convertRubToCurrencyCents(const std::string& rub_amount, const std::string& currency, int64_t& cents) {
    int64_t rub_cents = 0;
    if (!parseAmountToCents(rub_amount, rub_cents)) {
        return false;
    }
    if (currency == "rub") {
        cents = rub_cents;
        return true;
    }
    double rate = 0.0;
    if (!getCbrRate(currency, rate) || rate <= 0.0) {
        return false;
    }
    double rub_value = static_cast<double>(rub_cents) / 100.0;
    double converted = rub_value / rate;
    cents = static_cast<int64_t>(std::llround(converted * 100.0));
    if (cents < 1) cents = 1;
    return true;
}

struct GiftPlanConfig {
    int months = 0;
    int price = 0;
};

bool getGiftPlanConfig(const std::string& plan, GiftPlanConfig& out) {
    if (plan == "month") {
        out = {1, 99};
        return true;
    }
    if (plan == "half") {
        out = {6, 299};
        return true;
    }
    if (plan == "year") {
        out = {12, 499};
        return true;
    }
    return false;
}

std::string formatGiftAmount(const GiftPlanConfig& config) {
    std::ostringstream oss;
    oss << config.price << ".00";
    return oss.str();
}

const std::string kPremiumGiftPrefix = "[[XIPHER_PREMIUM_GIFT]]";

std::string hmacSha256Hex(const std::string& key, const std::string& message) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(),
         reinterpret_cast<const unsigned char*>(key.data()),
         static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(message.data()),
         message.size(),
         digest,
         &len);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

bool timingSafeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

bool parseStripeSignatureHeader(const std::string& header, std::string& timestamp, std::vector<std::string>& signatures) {
    timestamp.clear();
    signatures.clear();
    std::stringstream ss(header);
    std::string part;
    while (std::getline(ss, part, ',')) {
        part = trimEnvValue(part);
        if (part.rfind("t=", 0) == 0) {
            timestamp = part.substr(2);
        } else if (part.rfind("v1=", 0) == 0) {
            signatures.push_back(part.substr(3));
        }
    }
    return !timestamp.empty() && !signatures.empty();
}

bool verifyStripeSignature(const std::string& payload, const std::string& header,
                           const std::string& secret, int tolerance_seconds) {
    std::string timestamp;
    std::vector<std::string> signatures;
    if (!parseStripeSignatureHeader(header, timestamp, signatures)) {
        return false;
    }
    int64_t ts = 0;
    try {
        ts = std::stoll(timestamp);
    } catch (...) {
        return false;
    }
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    if (std::llabs(now - ts) > tolerance_seconds) {
        return false;
    }
    const std::string signed_payload = timestamp + "." + payload;
    const std::string expected = hmacSha256Hex(secret, signed_payload);
    for (const auto& sig : signatures) {
        if (timingSafeEqual(toLowerCopy(sig), toLowerCopy(expected))) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace xipher {

// Static storage for active calls (in-memory)
std::map<std::string, std::map<std::string, std::string>> RequestHandler::active_calls_;
std::map<std::string, std::string> RequestHandler::call_responses_;
std::map<std::string, std::string> RequestHandler::call_offers_;
std::map<std::string, std::string> RequestHandler::call_answers_;
std::map<std::string, std::vector<std::string>> RequestHandler::call_ice_candidates_;
std::map<std::string, int64_t> RequestHandler::call_signaling_timestamps_;
std::mutex RequestHandler::calls_mutex_;
std::unordered_map<std::string, std::vector<int64_t>> RequestHandler::slow_mode_buckets_;
std::mutex RequestHandler::slow_mode_mutex_;

void RequestHandler::storeCallOffer(const std::string& caller_id, const std::string& receiver_id, const std::string& offer_json) {
    std::lock_guard<std::mutex> lock(calls_mutex_);
    std::string call_key = caller_id + "_" + receiver_id;
    call_offers_[call_key] = offer_json;
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    call_signaling_timestamps_[call_key] = ts;
}

void RequestHandler::storeCallAnswer(const std::string& caller_id, const std::string& receiver_id, const std::string& answer_json) {
    std::lock_guard<std::mutex> lock(calls_mutex_);
    std::string call_key = caller_id + "_" + receiver_id;
    call_answers_[call_key] = answer_json;
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    call_signaling_timestamps_[call_key] = ts;
}

void RequestHandler::storeCallIce(const std::string& sender_id, const std::string& receiver_id, const std::string& candidate_json) {
    std::lock_guard<std::mutex> lock(calls_mutex_);
    std::string call_key1 = sender_id + "_" + receiver_id;
    std::string call_key2 = receiver_id + "_" + sender_id;
    call_ice_candidates_[call_key1].push_back(candidate_json);
    call_ice_candidates_[call_key2].push_back(candidate_json);
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    call_signaling_timestamps_[call_key1] = ts;
    call_signaling_timestamps_[call_key2] = ts;
}

namespace {

bool roleHasChannelPermission(const std::string& role, RequestHandler::ChannelPermission permission) {
    if (role == "creator") {
        return true; // creator has all permissions
    }

    if (role == "admin") {
        switch (permission) {
            case RequestHandler::ChannelPermission::ManageSettings:
            case RequestHandler::ChannelPermission::PostMessage:
            case RequestHandler::ChannelPermission::BanUser:
            case RequestHandler::ChannelPermission::ManageRequests:
            case RequestHandler::ChannelPermission::ManageReactions:
            case RequestHandler::ChannelPermission::PinMessage:
                return true;
            case RequestHandler::ChannelPermission::ManageAdmins:
                return false;
        }
    }

    return false; // subscribers and unknown roles have no elevated permissions
}

constexpr uint64_t kPermCanPostMessages = 1ull << 1;
constexpr uint64_t kPermCanChangeInfo   = 1ull << 0;
constexpr uint64_t kPermCanInvite       = 1ull << 4;
constexpr uint64_t kPermCanRestrict     = 1ull << 5;
constexpr uint64_t kPermCanPin          = 1ull << 6;
constexpr uint64_t kPermCanPromote      = 1ull << 7;
constexpr uint64_t kAllAdminPerms       = 0xFFull; // owner full permissions

bool isAllDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

std::string normalizeChatType(std::string type) {
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (type.empty() || type == "direct" || type == "message" || type == "chat") {
        return "chat";
    }
    if (type == "saved_messages" || type == "saved") {
        return "saved";
    }
    if (type == "group") {
        return "group";
    }
    if (type == "channel") {
        return "channel";
    }
    return type;
}

std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool parseBoolValue(const std::string& value, bool fallback) {
    if (value.empty()) {
        return fallback;
    }
    const std::string lower = toLowerCopy(value);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

bool isValidChatType(const std::string& type) {
    return type == "chat" || type == "group" || type == "channel" || type == "saved";
}

std::string trimWhitespace(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }
    return value.substr(start, end - start);
}

bool looksLikeJsonArray(const std::string& value) {
    std::string trimmed = trimWhitespace(value);
    return trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']';
}

bool looksLikeJsonObject(const std::string& value) {
    std::string trimmed = trimWhitespace(value);
    return trimmed.size() >= 2 && trimmed.front() == '{' && trimmed.back() == '}';
}

int countTopLevelJsonArrayItems(const std::string& value) {
    std::string trimmed = trimWhitespace(value);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        return -1;
    }
    bool in_string = false;
    bool escape_next = false;
    int depth = 0;
    bool in_item = false;
    int count = 0;
    for (size_t i = 1; i + 1 < trimmed.size(); ++i) {
        char c = trimmed[i];
        if (escape_next) {
            escape_next = false;
            continue;
        }
        if (c == '\\' && in_string) {
            escape_next = true;
            continue;
        }
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;

        if (c == '{' || c == '[') {
            depth++;
            continue;
        }
        if (c == '}' || c == ']') {
            if (depth > 0) depth--;
            continue;
        }
        if (depth == 0) {
            if (c == ',') {
                in_item = false;
                continue;
            }
            if (!std::isspace(static_cast<unsigned char>(c))) {
                if (!in_item) {
                    count++;
                    in_item = true;
                }
            }
        }
    }
    return count;
}

bool resolveChannelV2(DatabaseManager& db,
                      const std::string& channel_id_or_handle,
                      Chat* out_chat,
                      std::string* out_channel_id) {
    std::string key = channel_id_or_handle;
    Chat chat = db.getChatV2(key);
    if (chat.id.empty()) {
        std::string username = key;
        if (!username.empty() && username[0] == '@') {
            username = username.substr(1);
        }
        if (!username.empty()) {
            chat = db.getChatByUsernameV2(username);
        }
    }
    if (chat.id.empty() || chat.type != "channel") {
        return false;
    }
    if (out_chat) *out_chat = chat;
    if (out_channel_id) *out_channel_id = chat.id;
    return true;
}

} // namespace

RequestHandler::RequestHandler(DatabaseManager& db_manager, AuthManager& auth_manager)
    : db_manager_(db_manager),
      auth_manager_(auth_manager),
      admin_handler_(db_manager, auth_manager),
      fcm_client_(std::getenv("XIPHER_FCM_SERVICE_ACCOUNT") != nullptr
                  ? std::getenv("XIPHER_FCM_SERVICE_ACCOUNT")
                  : "/root/strotage/firebase-service-account.json"),
      rustore_client_(std::getenv("XIPHER_RUSTORE_PROJECT_ID") != nullptr
                      ? std::getenv("XIPHER_RUSTORE_PROJECT_ID")
                      : "",
                      std::getenv("XIPHER_RUSTORE_SERVICE_TOKEN") != nullptr
                      ? std::getenv("XIPHER_RUSTORE_SERVICE_TOKEN")
                      : "",
                      std::getenv("XIPHER_RUSTORE_BASE_URL") != nullptr
                      ? std::getenv("XIPHER_RUSTORE_BASE_URL")
                      : ""),
      report_rate_limiter_(5, 60, 60) {
}

void RequestHandler::setWebSocketSender(std::function<void(const std::string&, const std::string&)> sender) {
    ws_sender_ = std::move(sender);
}

bool RequestHandler::checkChannelPermission(const std::string& user_id,
                                            const std::string& channel_id,
                                            ChannelPermission permission,
                                            DatabaseManager::ChannelMember* out_member,
                                            std::string* error_message) {
    // Resolve potential @username into canonical chat/channel id for v2
    std::string resolved_channel_id = channel_id;
    if (!resolved_channel_id.empty() && resolved_channel_id[0] == '@') {
        resolved_channel_id = resolved_channel_id.substr(1);
    }
    // Try v2 unified chats first
    ChatPermissions perms_v2 = db_manager_.getChatPermissions(resolved_channel_id, user_id);
    if (perms_v2.found) {
        if (perms_v2.status == "kicked" || perms_v2.status == "left") {
            if (error_message) *error_message = "You are not a member of this channel";
            return false;
        }
        uint64_t required = 0;
        switch (permission) {
            case ChannelPermission::ManageSettings: required = kPermCanChangeInfo; break;
            case ChannelPermission::PostMessage: required = kPermCanPostMessages; break;
            case ChannelPermission::BanUser: required = kPermCanRestrict; break;
            case ChannelPermission::ManageAdmins: required = kPermCanPromote; break;
            case ChannelPermission::ManageRequests: required = kPermCanInvite; break;
            case ChannelPermission::ManageReactions: required = kPermCanChangeInfo; break;
            case ChannelPermission::PinMessage: required = kPermCanPin; break;
        }
        if ((perms_v2.perms & required) != required) {
            if (error_message) *error_message = "Insufficient channel permissions";
            return false;
        }
        return true;
    }

    // Legacy tables: also try resolving by custom link if @handle was provided
    DatabaseManager::ChannelMember member = db_manager_.getChannelMember(resolved_channel_id, user_id);
    DatabaseManager::Channel legacy_channel_meta;
    if (member.id.empty()) {
        legacy_channel_meta = db_manager_.getChannelById(resolved_channel_id);
        if (legacy_channel_meta.id.empty()) {
            // Maybe the caller passed a custom link instead of id
            std::string username_key = resolved_channel_id;
            if (!username_key.empty() && username_key[0] == '@') {
                username_key = username_key.substr(1);
            }
            legacy_channel_meta = db_manager_.getChannelByCustomLink(username_key);
            if (!legacy_channel_meta.id.empty()) {
                resolved_channel_id = legacy_channel_meta.id;
                member = db_manager_.getChannelMember(resolved_channel_id, user_id);
            }
        }
    }

    if (member.id.empty()) {
        // Fallback: allow creator (legacy channels) even if membership row missing
        if (legacy_channel_meta.id.empty()) {
            legacy_channel_meta = db_manager_.getChannelById(resolved_channel_id);
        }
        if (legacy_channel_meta.id == resolved_channel_id && legacy_channel_meta.creator_id == user_id) {
            if (out_member) {
                member.id = "creator-implicit";
                member.channel_id = resolved_channel_id;
                member.user_id = user_id;
                member.role = "creator";
                *out_member = member;
            }
            return true;
        }
        if (error_message) *error_message = "You are not a member of this channel";
        return false;
    }

    if (member.is_banned) {
        if (error_message) *error_message = "You are banned in this channel";
        return false;
    }

    if (!roleHasChannelPermission(member.role, permission)) {
        if (error_message) *error_message = "Insufficient channel permissions";
        return false;
    }

    if (out_member) {
        *out_member = member;
    }
    return true;
}

bool RequestHandler::checkPermission(const std::string& user_id,
                                     const std::string& chat_id,
                                     uint64_t required_mask,
                                     std::string* error) {
    ChatPermissions perms = db_manager_.getChatPermissions(chat_id, user_id);
    if (!perms.found) {
        Chat chat = db_manager_.getChatV2(chat_id);
        if (!chat.id.empty() && chat.created_by == user_id) {
            perms.found = true;
            perms.status = "owner";
            perms.perms = kAllAdminPerms;
        }
    }
    if (!perms.found) {
        if (error) *error = "not a participant";
        return false;
    }
    if (perms.status == "kicked" || perms.status == "left") {
        if (error) *error = "removed from chat";
        return false;
    }
    if ((perms.perms & required_mask) != required_mask) {
        if (error) *error = "insufficient permissions";
        return false;
    }
    return true;
}

bool RequestHandler::enforceSlowMode(const std::string& chat_id,
                                     const std::string& user_id,
                                     int slow_mode_sec,
                                     int limit_per_window,
                                     std::string* error) {
    if (slow_mode_sec <= 0 || limit_per_window <= 0) {
        return true;
    }
    const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const int64_t window_start = now - slow_mode_sec;
    const std::string key = chat_id + ":" + user_id;
    {
        std::lock_guard<std::mutex> lock(slow_mode_mutex_);
        auto& bucket = slow_mode_buckets_[key];
        bucket.erase(std::remove_if(bucket.begin(), bucket.end(),
                                    [&](int64_t ts){ return ts < window_start; }),
                     bucket.end());
        if (static_cast<int>(bucket.size()) >= limit_per_window) {
            if (error) *error = "slow mode active";
            return false;
        }
        bucket.push_back(now);
    }
    return true;
}

bool RequestHandler::checkRestriction(const std::string& chat_id,
                                      const std::string& user_id,
                                      bool want_media,
                                      bool want_links,
                                      bool want_invite,
                                      std::string* error) {
    Restriction r = db_manager_.getParticipantRestriction(chat_id, user_id);
    if (!r.found) {
        return true;
    }
    if (!r.can_send_messages) {
        if (error) *error = "sending blocked";
        return false;
    }
    if (want_media && !r.can_send_media) {
        if (error) *error = "media blocked";
        return false;
    }
    if (want_links && !r.can_add_links) {
        if (error) *error = "links blocked";
        return false;
    }
    if (want_invite && !r.can_invite_users) {
        if (error) *error = "invites blocked";
        return false;
    }
    return true;
}

std::string RequestHandler::handleRequest(const std::string& method,
                                         const std::string& path,
                                         const std::map<std::string, std::string>& headers,
                                         const std::string& body) {
    // Strip query string for routing
    std::string clean_path = path;
    size_t qpos = clean_path.find('?');
    if (qpos != std::string::npos) {
        clean_path = clean_path.substr(0, qpos);
    }

    // Handle CORS preflight requests
    if (method == "OPTIONS") {
        return handleOptions(clean_path, headers);
    } else if (method == "GET") {
        return handleGet(clean_path, headers);
    } else if (method == "POST") {
        return handlePost(clean_path, body, headers);
    } else {
        return JsonParser::createErrorResponse("Method not allowed");
    }
}

std::string RequestHandler::handleGet(const std::string& path, const std::map<std::string, std::string>& headers) {
    std::string route_path = path;
    if (route_path.size() > 1 && route_path.back() == '/') {
        route_path.pop_back();
    }

    auto extractSessionToken = [&]() -> std::string {
        return extractSessionTokenFromHeaders(headers);
    };

    auto isPrdSession = [&]() -> bool {
        const std::string token = extractSessionToken();
        if (token.empty() || !auth_manager_.validateSessionToken(token)) {
            return false;
        }
        const std::string user_id = auth_manager_.getUserIdFromToken(token);
        if (user_id.empty()) {
            return false;
        }
        const User user = db_manager_.getUserById(user_id);
        return !user.id.empty() && user.username == "prd";
    };

    auto serveAdminPage = [&](const std::string& file_path) -> std::string {
        if (!isPrdSession()) {
            return serveStaticFileWithStatus("/root/xipher/web/404.html", "HTTP/1.1 404 Not Found");
        }
        return serveStaticFile(file_path);
    };

    // Clean URL routing (without .html)
    if (route_path == "/" || route_path == "/index" || route_path == "/index.html") {
        return serveStaticFile("/root/xipher/web/index.html");
    } else if (route_path == "/xipher.ico" || route_path == "/favicon.ico") {
        return serveStaticFile("/root/xipher/web/xipher.ico");
    } else if (route_path == "/register" || route_path == "/register.html") {
        return serveStaticFile("/root/xipher/web/register.html");
    } else if (route_path == "/login" || route_path == "/login.html") {
        return serveStaticFile("/root/xipher/web/login.html");
    } else if (route_path == "/privacy" || route_path == "/privacy.html") {
        return serveStaticFile("/root/xipher/web/privacy.html");
    } else if (route_path == "/terms" || route_path == "/terms.html") {
        return serveStaticFile("/root/xipher/web/terms.html");
    } else if (route_path == "/admin" || route_path == "/admin.html") {
        return serveAdminPage("/root/xipher/web/admin-login.html");
    } else if (route_path == "/admin/panel" || route_path == "/admin/panel.html") {
        return serveAdminPage("/root/xipher/web/admin.html");
    } else if (route_path == "/admin/reports" || route_path == "/admin/reports.html") {
        return serveAdminPage("/root/xipher/web/admin-reports.html");
    } else if (route_path == "/chat" || route_path == "/chat.html") {
        // Serve chat shell; auth validation happens client-side via session cookie.
        return serveStaticFile("/root/xipher/web/chat.html");
    } else if (route_path.rfind("/chat/", 0) == 0) {
        // Deep links like /chat/<id> should load the chat shell.
        return serveStaticFile("/root/xipher/web/chat.html");
    } else if (route_path == "/firebase-messaging-sw.js") {
        // Firebase Cloud Messaging service worker must be served from the origin root.
        return serveStaticFile("/root/xipher/web/firebase-messaging-sw.js");
    } else if (route_path == "/bots" || route_path == "/bots.html") {
        // Страница управления ботами: требует авторизации как /chat
        std::string token = extractSessionTokenFromHeaders(headers);
        if (token.empty() || !auth_manager_.validateSessionToken(token)) {
            Logger::getInstance().warning("Unauthorized access to /bots. Token: " + (token.empty() ? "empty" : token.substr(0, 10) + "..."));
            return "HTTP/1.1 302 Found\r\nLocation: /login\r\n\r\n";
        }
        return serveStaticFile("/root/xipher/web/bots.html");
    } else if (route_path == "/api" || route_path == "/api.html") {
        return serveStaticFile("/root/xipher/web/api.html");
    } else if (route_path == "/api/ai-oauth-config") {
        std::map<std::string, std::string> cfg;
        const char* authorize_env = std::getenv("XIPHER_AI_OAUTH_AUTHORIZE_URL");
        const char* token_env = std::getenv("XIPHER_AI_OAUTH_TOKEN_URL");
        const char* client_env = std::getenv("XIPHER_AI_OAUTH_CLIENT_ID");
        const char* scopes_env = std::getenv("XIPHER_AI_OAUTH_SCOPES");
        const char* use_env = std::getenv("XIPHER_AI_OAUTH_DEFAULT");

        cfg["authorize_url"] = authorize_env ? trimEnvValue(authorize_env) : "";
        cfg["token_url"] = token_env ? trimEnvValue(token_env) : "";
        cfg["client_id"] = client_env ? trimEnvValue(client_env) : "";
        cfg["scopes"] = scopes_env ? trimEnvValue(scopes_env) : "";
        cfg["default_use_oauth"] = envFlagEnabled(use_env) ? "true" : "false";

        return buildHttpJson(JsonParser::createSuccessResponse("ok", cfg));
    } else if (route_path == "/api/public-stats") {
        std::map<std::string, std::string> stats;
        stats["users"] = std::to_string(db_manager_.countUsers());
        stats["active_24h"] = std::to_string(db_manager_.countActiveUsersSince(24 * 3600));
        stats["messages_today"] = std::to_string(db_manager_.countMessagesToday());
        return buildHttpJson(JsonParser::createSuccessResponse("public_stats", stats));
    } else if (route_path.find("/api/wallet/tokens") == 0) {
        // Proxy CoinGecko tokens API
        std::string query = "";
        size_t qpos = route_path.find('?');
        if (qpos != std::string::npos) {
            query = route_path.substr(qpos + 1);
        }
        return buildHttpJson(handleWalletTokens(query));
    } else if (route_path.find("/api/wallet/prices") == 0) {
        // Proxy CoinGecko prices API
        std::string query = "";
        size_t qpos = route_path.find('?');
        if (qpos != std::string::npos) {
            query = route_path.substr(qpos + 1);
        }
        return buildHttpJson(handleWalletPrices(query));
    } else if (route_path.find("/api/wallet/staking/products") == 0) {
        // Get staking products list
        return buildHttpJson(handleStakingProducts(""));
    } else if (route_path.find("/css/") == 0) {
        return serveStaticFile("/root/xipher/web" + route_path);
    } else if (route_path.find("/js/") == 0) {
        return serveStaticFile("/root/xipher/web" + route_path);
    } else if (route_path.find("/images/") == 0) {
        return serveStaticFile("/root/xipher/web" + route_path);
    } else if (route_path.rfind("/app/", 0) == 0) {
        if (route_path.find("..") != std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\n400 Bad Request";
        }
        return serveStaticFile("/root/strotage" + route_path);
    } else if (route_path.rfind("/join/", 0) == 0) {
        // Invite links route to chat UI; chat.js will process join code
        return serveStaticFile("/root/xipher/web/chat.html");
    } else if (route_path.find("/files/") == 0) {
        return handleGetFile(route_path);
    } else if (route_path.find("/avatars/") == 0) {
        return handleGetAvatar(route_path);
    } else if (route_path.length() > 1 && route_path[0] == '/' && route_path[1] == '@') {
        // Универсальные ссылки /@username: каналы + пользователи + боты.
        // Показываем красивую страницу предпросмотра
        return handleChannelUrl(route_path);
    } else if (route_path == "/bot-ide.html" || route_path == "/bot-ide") {
        // Serve bot IDE page (must check before /bot* Bot API routes)
        return serveStaticFile("/root/xipher/web/bot-ide.html");
    } else if (route_path.length() > 4 && route_path.substr(0, 4) == "/bot") {
        // Обработка Telegram Bot API запросов: /bot<token>/method_name (GET для getUpdates и getWebhookInfo)
        // But exclude /bot-ide.html which is handled above
        if (route_path.find("/bot-ide") == 0) {
            return serveStaticFile("/root/xipher/web/bot-ide.html");
        }
        return handleBotApiRequest(route_path, "GET", "", headers);
    } else {
        // Keep JSON for API calls, but show a nice 404 page for browser navigation.
        if (route_path.rfind("/api", 0) == 0) {
            return JsonParser::createErrorResponse("Not found");
        }
        return serveStaticFileWithStatus("/root/xipher/web/404.html", "HTTP/1.1 404 Not Found");
    }
}

std::string RequestHandler::handleOptions(const std::string& /*path*/, const std::map<std::string, std::string>& /*headers*/) {
    // Handle CORS preflight requests for Bot API
    std::ostringstream oss;
    oss << "HTTP/1.1 204 No Content\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "Access-Control-Max-Age: 86400\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "\r\n";
    return oss.str();
}

std::string RequestHandler::handlePost(const std::string& path, const std::string& raw_body, const std::map<std::string, std::string>& headers) {
    std::string body_storage;
    std::string content_type;
    auto content_type_it = headers.find("Content-Type");
    if (content_type_it == headers.end()) {
        content_type_it = headers.find("content-type");
    }
    if (content_type_it != headers.end()) {
        content_type = toLowerCopy(content_type_it->second);
    }
    bool should_inject = path.rfind("/api", 0) == 0 && content_type.find("application/json") != std::string::npos;
    if (path == "/api/login" || path == "/api/register") {
        should_inject = false;
    }
    if (path == "/api/ai-chat" || path == "/api/ai-models" || path == "/api/ai-oauth-exchange") {
        should_inject = false;
    }
    if (should_inject) {
        body_storage = maybeInjectSessionToken(raw_body, headers);
    }
    const std::string& body = should_inject ? body_storage : raw_body;

    if (path == "/api/register") {
        return handleRegister(body);
    } else if (path == "/api/login") {
        return handleLogin(body, headers);
    } else if (path == "/api/logout") {
        return handleLogout(body, headers);
    } else if (path == "/api/refresh-session") {
        return handleRefreshSession(body, headers);
    } else if (path == "/api/get-sessions") {
        return handleGetSessions(body);
    } else if (path == "/api/revoke-session") {
        return handleRevokeSession(body);
    } else if (path == "/api/revoke-selected-sessions") {
        return handleRevokeSelectedSessions(body);
    } else if (path == "/api/revoke-other-sessions") {
        return handleRevokeOtherSessions(body);
    } else if (path == "/api/admin/login") {
        return admin_handler_.handleLogin(headers, body);
    } else if (path == "/api/admin/action") {
        return admin_handler_.handleAction(headers, body);
    } else if (path == "/api/admin/reports/list") {
        return handleAdminGetReports(body, headers);
    } else if (path == "/api/admin/reports/update") {
        return handleAdminUpdateReport(body, headers);
    } else if (path == "/api/check-username") {
        return handleCheckUsername(body);
    } else if (path == "/api/register-push-token") {
        return handleRegisterPushToken(body);
    } else if (path == "/api/delete-push-token") {
        return handleDeletePushToken(body);
    } else if (path == "/api/friends") {
        return handleGetFriends(body);
    } else if (path == "/api/chats") {
        return handleGetChats(body);
    } else if (path == "/api/get-chat-pins") {
        return handleGetChatPins(body);
    } else if (path == "/api/pin-chat") {
        return handlePinChat(body);
    } else if (path == "/api/unpin-chat") {
        return handleUnpinChat(body);
    } else if (path == "/api/get-chat-folders") {
        return handleGetChatFolders(body);
    } else if (path == "/api/set-chat-folders") {
        return handleSetChatFolders(body);
    } else if (path == "/api/set-premium") {
        return handleSetPremium(body);
    } else if (path == "/api/premium/create-payment") {
        return handleCreatePremiumPayment(body);
    } else if (path == "/api/premium/create-gift-payment") {
        return handleCreatePremiumGiftPayment(body);
    } else if (path == "/stripe/webhook" || path == "/stripe/webhook/") {
        return handleStripeWebhook(body, headers);
    } else if (path == "/api/messages") {
        return handleGetMessages(body);
    } else if (path == "/api/send-message") {
        return handleSendMessage(body);
    } else if (path == "/api/delete-message") {
        return handleDeleteMessage(body);
    } else if (path == "/api/delete-group-message") {
        return handleDeleteGroupMessage(body);
    } else if (path == "/api/delete-channel-message") {
        return handleDeleteChannelMessage(body);
    } else if (path == "/api/reports") {
        return handleCreateReport(body);
    } else if (path == "/api/unpin-message") {
        return handleUnpinMessage(body);
    } else if (path == "/api/friend-request") {
        return handleFriendRequest(body);
    } else if (path == "/api/accept-friend") {
        return handleAcceptFriend(body);
    } else if (path == "/api/reject-friend") {
        return handleRejectFriend(body);
    } else if (path == "/api/friend-requests") {
        return handleGetFriendRequests(body);
    } else if (path == "/api/find-user") {
        return handleFindUser(body);
    } else if (path == "/api/resolve-username") {
        return handleResolveUsername(body);
    } else if (path == "/api/bot-callback-query") {
        return handleBotCallbackQuery(body);
    } else if (path == "/api/get-user-profile") {
        return handleGetUserProfile(body);
    } else if (path == "/api/set-personal-channel") {
        return handleSetPersonalChannel(body);
    } else if (path == "/api/update-my-profile") {
        return handleUpdateMyProfile(body);
    } else if (path == "/api/update-my-privacy") {
        return handleUpdateMyPrivacy(body);
    } else if (path == "/api/set-business-hours") {
        return handleSetBusinessHours(body);
    } else if (path == "/api/validate-token") {
        return handleValidateToken(body);
    } else if (path == "/api/ai-models") {
        return handleAiProxyModels(body, headers);
    } else if (path == "/api/ai-chat") {
        return handleAiProxyChat(body, headers);
    } else if (path == "/api/ai-oauth-exchange") {
        return handleAiOauthExchange(body, headers);
    } else if (path == "/api/update-activity") {
        return handleUpdateActivity(body);
    } else if (path == "/api/user-status") {
        return handleUserStatus(body);
    } else if (path == "/api/turn-config") {
        return handleGetTurnConfig(body);
    } else if (path == "/api/sfu-config") {
        return handleGetSfuConfig(body);
    } else if (path == "/api/sfu-room") {
        return handleGetSfuRoom(body);
    } else if (path == "/api/upload-file") {
        return handleUploadFile(body, headers);
    } else if (path == "/api/upload-voice") {
        return handleUploadVoice(body, headers);
    } else if (path == "/api/upload-avatar" || path == "/api/upload_avatar") {
        return handleUploadAvatar(body, headers);
    } else if (path == "/api/upload-channel-avatar" || path == "/api/upload_channel_avatar") {
        return handleUploadChannelAvatar(body, headers);
    } else if (path == "/notification" || path == "/notification/" || path == "/callback" || path == "/callback/") {
        return handleYooMoneyNotification(body);
    } else if (path == "/api/call-notification") {
        return handleCallNotification(body);
    } else if (path == "/api/call-response") {
        return handleCallResponse(body);
    } else if (path == "/api/group-call-notification") {
        return handleGroupCallNotification(body);
    } else if (path == "/api/call-offer") {
        return handleCallOffer(body);
    } else if (path == "/api/call-answer") {
        return handleCallAnswer(body);
    } else if (path == "/api/call-ice") {
        return handleCallIce(body);
    } else if (path == "/api/call-end") {
        return handleCallEnd(body);
    } else if (path == "/api/check-incoming-calls") {
        return handleCheckIncomingCalls(body);
    } else if (path == "/api/check-call-response") {
        return handleCheckCallResponse(body);
    } else if (path == "/api/get-call-offer") {
        return handleGetCallOffer(body);
    } else if (path == "/api/get-call-answer") {
        return handleGetCallAnswer(body);
    } else if (path == "/api/get-call-ice") {
        return handleGetCallIce(body);
    } else if (path == "/api/create-group") {
        return handleCreateGroup(body);
    } else if (path == "/api/get-groups") {
        return handleGetGroups(body);
    } else if (path == "/api/get-group-members") {
        return handleGetGroupMembers(body);
    } else if (path == "/api/get-group-messages") {
        return handleGetGroupMessages(body);
    } else if (path == "/api/send-group-message") {
        return handleSendGroupMessage(body);
    } else if (path == "/api/create-group-invite") {
        return handleCreateGroupInvite(body);
    } else if (path == "/api/leave-group") {
        return handleLeaveGroup(body);
    } else if (path == "/api/join-group") {
        return handleJoinGroup(body);
    } else if (path == "/api/update-group-name") {
        return handleUpdateGroupName(body);
    } else if (path == "/api/update-group-description") {
        return handleUpdateGroupDescription(body);
    } else if (path == "/api/pin-message") {
        return handlePinMessage(body);
    } else if (path == "/api/ban-member") {
        return handleBanMember(body);
    } else if (path == "/api/mute-member") {
        return handleMuteMember(body);
    } else if (path == "/api/forward-message") {
        return handleForwardMessage(body);
    } else if (path == "/api/kick-member") {
        return handleKickMember(body);
    } else if (path == "/api/add-friend-to-group") {
        return handleAddFriendToGroup(body);
    } else if (path == "/api/set-admin-permissions") {
        return handleSetAdminPermissions(body);
    } else if (path == "/api/delete-group") {
        return handleDeleteGroup(body);
    } else if (path == "/api/search-group-messages") {
        return handleSearchGroupMessages(body);
    } else if (path == "/api/update-group-permissions") {
        return handleUpdateGroupPermissions(body);
    // Group Topics (Forum mode) routes
    } else if (path == "/api/set-group-forum-mode") {
        return handleSetGroupForumMode(body);
    } else if (path == "/api/create-group-topic") {
        return handleCreateGroupTopic(body);
    } else if (path == "/api/update-group-topic") {
        return handleUpdateGroupTopic(body);
    } else if (path == "/api/delete-group-topic") {
        return handleDeleteGroupTopic(body);
    } else if (path == "/api/get-group-topics") {
        return handleGetGroupTopics(body);
    } else if (path == "/api/pin-group-topic") {
        return handlePinGroupTopic(body);
    } else if (path == "/api/unpin-group-topic") {
        return handleUnpinGroupTopic(body);
    } else if (path == "/api/get-topic-messages") {
        return handleGetTopicMessages(body);
    } else if (path == "/api/send-topic-message") {
        return handleSendTopicMessage(body);
    } else if (path == "/api/hide-general-topic") {
        return handleHideGeneralTopic(body);
    } else if (path == "/api/toggle-topic-closed") {
        return handleToggleTopicClosed(body);
    } else if (path == "/api/reorder-pinned-topics") {
        return handleReorderPinnedTopics(body);
    } else if (path == "/api/search-topics") {
        return handleSearchTopics(body);
    } else if (path == "/api/set-topic-notifications") {
        return handleSetTopicNotifications(body);
    } else if (path == "/api/create-channel") {
        return handleCreateChannel(body);
    } else if (path == "/api/get-channels") {
        return handleGetChannels(body);
    } else if (path == "/api/public-directory") {
        return handlePublicDirectory(body);
    } else if (path == "/api/request-verification") {
        return handleRequestVerification(body);
    } else if (path == "/api/get-verification-requests") {
        return handleGetVerificationRequests(body);
    } else if (path == "/api/review-verification") {
        return handleReviewVerification(body);
    } else if (path == "/api/get-my-verification-requests") {
        return handleGetMyVerificationRequests(body);
    } else if (path == "/api/set-group-public") {
        return handleSetGroupPublic(body);
    } else if (path == "/api/set-channel-public") {
        return handleSetChannelPublic(body);
    } else if (path == "/api/get-channel-messages") {
        return handleGetChannelMessages(body);
    } else if (path == "/api/send-channel-message") {
        return handleSendChannelMessage(body);
    } else if (path == "/api/subscribe-channel") {
        return handleSubscribeChannel(body);
    } else if (path == "/api/create-channel-invite") {
        return handleCreateChannelInvite(body);
    } else if (path == "/api/revoke-channel-invite") {
        return handleRevokeChannelInvite(body);
    } else if (path == "/api/join-channel-by-invite") {
        return handleJoinChannelByInvite(body);
    } else if (path == "/api/add-message-reaction") {
        return handleAddMessageReaction(body);
    } else if (path == "/api/remove-message-reaction") {
        return handleRemoveMessageReaction(body);
    } else if (path == "/api/get-message-reactions") {
        return handleGetMessageReactions(body);
    } else if (path == "/api/read-channel") {
        return handleReadChannel(body);
    } else if (path == "/api/create-poll") {
        return handleCreatePoll(body);
    } else if (path == "/api/vote-poll") {
        return handleVotePoll(body);
    } else if (path == "/api/get-poll") {
        return handleGetPoll(body);
    } else if (path == "/api/add-message-view") {
        return handleAddMessageView(body);
    } else if (path == "/api/update-channel-name") {
        return handleUpdateChannelName(body);
    } else if (path == "/api/update-channel-description") {
        return handleUpdateChannelDescription(body);
    } else if (path == "/api/set-channel-custom-link") {
        return handleSetChannelCustomLink(body);
    } else if (path == "/api/set-channel-privacy") {
        return handleSetChannelPrivacy(body);
    } else if (path == "/api/set-channel-show-author") {
        return handleSetChannelShowAuthor(body);
    } else if (path == "/api/add-allowed-reaction") {
        return handleAddAllowedReaction(body);
    } else if (path == "/api/remove-allowed-reaction") {
        return handleRemoveAllowedReaction(body);
    } else if (path == "/api/get-channel-allowed-reactions") {
        return handleGetChannelAllowedReactions(body);
    } else if (path == "/api/get-channel-allowed-reactions") {
        return handleGetChannelAllowedReactions(body);
    } else if (path == "/api/get-channel-join-requests") {
        return handleGetChannelJoinRequests(body);
    } else if (path == "/api/accept-channel-join-request") {
        return handleAcceptChannelJoinRequest(body);
    } else if (path == "/api/reject-channel-join-request") {
        return handleRejectChannelJoinRequest(body);
    } else if (path == "/api/set-channel-admin-permissions") {
        return handleSetChannelAdminPermissions(body);
    } else if (path == "/api/ban-channel-member") {
        return handleBanChannelMember(body);
    } else if (path == "/api/get-channel-members") {
        return handleGetChannelMembers(body);
    } else if (path == "/api/search-channel") {
        return handleSearchChannel(body);
    } else if (path == "/api/get-channel-info") {
        return handleGetChannelInfo(body);
    } else if (path == "/api/unsubscribe-channel") {
        return handleUnsubscribeChannel(body);
    } else if (path == "/api/delete-channel") {
        return handleDeleteChannel(body);
    } else if (path == "/api/link-channel-discussion") {
        return handleLinkChannelDiscussion(body);
    } else if (path == "/api/create-store-item") {
        return handleCreateStoreItem(body);
    } else if (path == "/api/edit-store-item") {
        return handleEditStoreItem(body);
    } else if (path == "/api/delete-store-item") {
        return handleDeleteStoreItem(body);
    } else if (path == "/api/get-store-items") {
        return handleGetStoreItems(body);
    } else if (path == "/api/purchase-product") {
        return handlePurchaseProduct(body);
    } else if (path == "/api/create-integration") {
        return handleCreateIntegration(body);
    } else if (path == "/api/get-integrations") {
        return handleGetIntegrations(body);
    } else if (path == "/api/delete-integration") {
        return handleDeleteIntegration(body);
    } else if (path.find("/api/oauth-callback/") == 0) {
        return handleOAuthCallback(path, body);
    } else if (path == "/api/create-bot") {
        return handleCreateBot(body);
    } else if (path == "/api/update-bot-profile") {
        return handleUpdateBotProfile(body);
    } else if (path == "/api/update-bot-flow") {
        return handleUpdateBotFlow(body);
    } else if (path == "/api/get-user-bots") {
        return handleGetUserBots(body);
    } else if (path == "/api/deploy-bot") {
        return handleDeployBot(body);
    } else if (path == "/api/delete-bot") {
        return handleDeleteBot(body);
    } else if (path == "/api/upload-bot-avatar" || path == "/api/upload_bot_avatar") {
        return handleUploadBotAvatar(body, headers);
    } else if (path == "/api/get-bot-logs") {
        return handleGetBotLogs(body);
    } else if (path == "/api/replay-webhook") {
        return handleReplayWebhook(body);
    } else if (path == "/api/reveal-bot-token") {
        return handleRevealBotToken(body);
    } else if (path == "/api/get-bot-script") {
        return handleGetBotScript(body);
    } else if (path == "/api/update-bot-script") {
        return handleUpdateBotScript(body);
    } else if (path == "/api/add-bot-developer") {
        return handleAddBotDeveloper(body);
    } else if (path == "/api/remove-bot-developer") {
        return handleRemoveBotDeveloper(body);
    } else if (path == "/api/get-bot-developers") {
        return handleGetBotDevelopers(body);
    } else if (path == "/api/list-bot-files") {
        return handleListBotFiles(body);
    } else if (path == "/api/get-bot-file") {
        return handleGetBotFile(body);
    } else if (path == "/api/save-bot-file") {
        return handleSaveBotFile(body);
    } else if (path == "/api/delete-bot-file") {
        return handleDeleteBotFile(body);
    } else if (path == "/api/build-bot-project") {
        return handleBuildBotProject(body);
    } else if (path == "/api/create-trigger-rule") {
        return handleCreateTriggerRule(body);
    } else if (path == "/api/update-trigger-rule") {
        return handleUpdateTriggerRule(body);
    } else if (path == "/api/delete-trigger-rule") {
        return handleDeleteTriggerRule(body);
    } else if (path == "/api/get-trigger-rules") {
        return handleGetTriggerRules(body);
    } else if (path == "/api/create-event-webhook") {
        return handleCreateEventWebhook(body);
    // E2EE (End-to-End Encryption) API
    } else if (path == "/api/e2ee/register-key") {
        return handleE2EERegisterKey(body);
    } else if (path == "/api/e2ee/get-public-key") {
        return handleE2EEGetPublicKey(body);
    } else if (path == "/api/e2ee/get-public-keys") {
        return handleE2EEGetPublicKeys(body);
    } else if (path == "/api/e2ee/send-encrypted") {
        return handleE2EESendMessage(body);
    } else if (path == "/api/e2ee/status") {
        return handleE2EEStatus(body);
    // Wallet API
    } else if (path == "/api/wallet/register") {
        return handleWalletRegister(body);
    } else if (path == "/api/wallet/get-address") {
        return handleWalletGetAddress(body);
    } else if (path == "/api/wallet/balance") {
        return handleWalletBalance(body);
    } else if (path == "/api/wallet/relay-tx") {
        return handleWalletRelayTx(body);
    } else if (path == "/api/wallet/history") {
        return handleWalletHistory(body);
    } else if (path == "/api/wallet/send-to-user") {
        return handleWalletSendToUser(body);
    // Wallet sync API (server-side wallet storage)
    } else if (path == "/api/wallet/save") {
        return handleWalletSave(body);
    } else if (path == "/api/wallet/get") {
        return handleWalletGet(body);
    } else if (path == "/api/wallet/update") {
        return handleWalletUpdate(body);
    } else if (path == "/api/wallet/delete") {
        return handleWalletDelete(body);
    // P2P Marketplace API
    } else if (path == "/api/wallet/p2p/offers") {
        return handleP2POffers(body);
    } else if (path == "/api/wallet/p2p/create-offer") {
        return handleP2PCreateOffer(body);
    } else if (path == "/api/wallet/p2p/start-trade") {
        return handleP2PStartTrade(body);
    } else if (path == "/api/wallet/p2p/my-trades") {
        return handleP2PMyTrades(body);
    } else if (path == "/api/wallet/p2p/update-trade") {
        return handleP2PUpdateTrade(body);
    // Vouchers API
    } else if (path == "/api/wallet/vouchers/create") {
        return handleVoucherCreate(body);
    } else if (path == "/api/wallet/vouchers/claim") {
        return handleVoucherClaim(body);
    } else if (path == "/api/wallet/vouchers/my") {
        return handleVouchersMy(body);
    // Staking API
    } else if (path == "/api/wallet/staking/stake") {
        return handleStakingStake(body);
    } else if (path == "/api/wallet/staking/my-stakes") {
        return handleStakingMyStakes(body);
    } else if (path == "/api/wallet/staking/unstake") {
        return handleStakingUnstake(body);
    // NFT API
    } else if (path == "/api/wallet/nft/my") {
        return handleNFTMy(body);
    } else if (path == "/api/wallet/nft/refresh") {
        return handleNFTRefresh(body);
    // Security API
    } else if (path == "/api/wallet/security/get") {
        return handleSecurityGet(body);
    } else if (path == "/api/wallet/security/update") {
        return handleSecurityUpdate(body);
    } else if (path.length() > 1 && path[0] == '/' && path[1] == '@') {
        // Обработка URL вида /@username для прямого доступа к каналу
        return handleChannelUrl(path);
    } else if (path.length() > 4 && path.substr(0, 4) == "/bot") {
        // Обработка Telegram Bot API запросов: /bot<token>/method_name
        // But exclude /bot-ide.html which should be GET only
        if (path.find("/bot-ide") == 0) {
            return serveStaticFile("/root/xipher/web/bot-ide.html");
        }
        return handleBotApiRequest(path, "POST", body, headers);
    } else {
        return JsonParser::createErrorResponse("Not found");
    }
}

std::string RequestHandler::handleRegister(const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("username") == data.end() || data.find("password") == data.end()) {
        return JsonParser::createErrorResponse("Username and password are required");
    }
    
    std::string username = data["username"];
    std::string password = data["password"];
    
    auto result = auth_manager_.registerUser(username, password);
    
    if (result["success"] == "true") {
        return JsonParser::createSuccessResponse(result["message"]);
    } else {
        return JsonParser::createErrorResponse(result["message"]);
    }
}

std::string RequestHandler::handleLogin(const std::string& body, const std::map<std::string, std::string>& headers) {
    auto data = JsonParser::parse(body);
    
    if (data.find("username") == data.end() || data.find("password") == data.end()) {
        return JsonParser::createErrorResponse("Username and password are required");
    }
    
    std::string username = data["username"];
    std::string password = data["password"];
    
    auto result = auth_manager_.login(username, password);
    
    if (result["success"] == "true") {
        std::map<std::string, std::string> response_data;
        response_data["user_id"] = result["user_id"];
        response_data["token"] = result["token"];
        response_data["username"] = result["username"];
        User user = db_manager_.getUserById(result["user_id"]);
        response_data["is_premium"] = user.is_premium ? "true" : "false";
        response_data["premium_plan"] = user.premium_plan;
        response_data["premium_expires_at"] = user.premium_expires_at;
        Logger::getInstance().info("Login successful for user: " + username);

        std::ostringstream cookie;
        cookie << kSessionTokenCookieName << "=" << result["token"]
               << "; HttpOnly; SameSite=Strict; Path=/; Max-Age=" << kSessionTtlSeconds;
        if (isSecureRequest(headers)) {
            cookie << "; Secure";
        }

        std::map<std::string, std::string> headers_out;
        headers_out["Set-Cookie"] = cookie.str();

        const std::string* user_agent_header = findHeaderValueCI(headers, "User-Agent");
        if (user_agent_header && !user_agent_header->empty()) {
            db_manager_.updateUserSessionUserAgent(result["token"], *user_agent_header);
        }

        return buildHttpJson(JsonParser::createSuccessResponse(result["message"], response_data), headers_out);
    } else {
        Logger::getInstance().warning("Login failed for user: " + username + ", reason: " + result["message"]);
        return JsonParser::createErrorResponse(result["message"]);
    }
}

std::string RequestHandler::handleLogout(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::string token = extractSessionTokenFromHeaders(headers);
    if (token.empty()) {
        auto data = JsonParser::parse(body);
        token = data.count("token") ? data["token"] : "";
        if (token == kSessionTokenPlaceholder) {
            token.clear();
        }
    }

    if (!token.empty()) {
        auth_manager_.revokeSessionToken(token);
    }

    std::ostringstream cookie;
    cookie << kSessionTokenCookieName << "=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0";
    if (isSecureRequest(headers)) {
        cookie << "; Secure";
    }

    std::map<std::string, std::string> headers_out;
    headers_out["Set-Cookie"] = cookie.str();

    return buildHttpJson(JsonParser::createSuccessResponse("Logged out"), headers_out);
}

std::string RequestHandler::handleRefreshSession(const std::string& body, const std::map<std::string, std::string>& headers) {
    std::string token = extractSessionTokenFromHeaders(headers);
    if (token.empty()) {
        auto data = JsonParser::parse(body);
        token = data.count("token") ? data["token"] : "";
        if (token == kSessionTokenPlaceholder) {
            token.clear();
        }
    }

    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }
    if (token.empty()) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }
    if (token.empty()) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string new_token = auth_manager_.rotateSessionToken(token);
    if (new_token.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    std::ostringstream cookie;
    cookie << kSessionTokenCookieName << "=" << new_token
           << "; HttpOnly; SameSite=Strict; Path=/; Max-Age=" << kSessionTtlSeconds;
    if (isSecureRequest(headers)) {
        cookie << "; Secure";
    }

    std::map<std::string, std::string> headers_out;
    headers_out["Set-Cookie"] = cookie.str();

    const std::string* user_agent_header = findHeaderValueCI(headers, "User-Agent");
    if (user_agent_header && !user_agent_header->empty()) {
        db_manager_.updateUserSessionUserAgent(new_token, *user_agent_header);
    }

    return buildHttpJson(JsonParser::createSuccessResponse("Session refreshed"), headers_out);
}

std::string RequestHandler::handleGetSessions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto sessions = db_manager_.getUserSessions(user_id);

    std::ostringstream oss;
    oss << "{\"success\":true,\"sessions\":[";

    bool first = true;
    for (const auto& session : sessions) {
        if (session.token == token) {
            continue;
        }
        if (!first) oss << ",";
        first = false;

        oss << "{"
            << "\"session_id\":\"" << JsonParser::escapeJson(hashSessionToken(session.token)) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(session.created_at) << "\","
            << "\"last_seen\":\"" << JsonParser::escapeJson(session.last_seen) << "\","
            << "\"user_agent\":\"" << JsonParser::escapeJson(session.user_agent) << "\""
            << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleRevokeSession(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string session_id = data["session_id"];

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    if (session_id.empty()) {
        return JsonParser::createErrorResponse("Session id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto sessions = db_manager_.getUserSessions(user_id);
    for (const auto& session : sessions) {
        if (hashSessionToken(session.token) != session_id) {
            continue;
        }
        if (session.token == token) {
            return JsonParser::createErrorResponse("Cannot revoke current session");
        }
        if (!auth_manager_.revokeSessionToken(session.token)) {
            return JsonParser::createErrorResponse("Failed to revoke session");
        }
        return JsonParser::createSuccessResponse("Session revoked");
    }

    return JsonParser::createErrorResponse("Session not found");
}

std::string RequestHandler::handleRevokeSelectedSessions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string session_ids_raw = data["session_ids"];

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    if (session_ids_raw.empty()) {
        return JsonParser::createErrorResponse("Session ids required");
    }

    std::vector<std::string> session_ids = JsonParser::parseStringArray(session_ids_raw);
    if (session_ids.empty()) {
        return JsonParser::createErrorResponse("No valid session ids provided");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto sessions = db_manager_.getUserSessions(user_id);
    int revoked = 0;
    
    for (const std::string& target_id : session_ids) {
        for (const auto& session : sessions) {
            if (hashSessionToken(session.token) != target_id) {
                continue;
            }
            if (session.token == token) {
                continue; // Cannot revoke current session
            }
            if (auth_manager_.revokeSessionToken(session.token)) {
                revoked++;
            }
            break;
        }
    }

    std::map<std::string, std::string> response;
    response["revoked"] = std::to_string(revoked);
    return JsonParser::createSuccessResponse("Sessions revoked", response);
}

std::string RequestHandler::handleRevokeOtherSessions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto sessions = db_manager_.getUserSessions(user_id);
    int revoked = 0;
    for (const auto& session : sessions) {
        if (session.token == token) {
            continue;
        }
        if (auth_manager_.revokeSessionToken(session.token)) {
            revoked++;
        }
    }

    std::map<std::string, std::string> response;
    response["revoked"] = std::to_string(revoked);
    return JsonParser::createSuccessResponse("Sessions revoked", response);
}

std::string RequestHandler::handleCheckUsername(const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("username") == data.end()) {
        return JsonParser::createErrorResponse("Username is required");
    }
    
    std::string username = data["username"];
    
    if (!auth_manager_.validateUsername(username)) {
        return JsonParser::createErrorResponse("Invalid username format");
    }
    
    bool exists = db_manager_.usernameExists(username);
    
    if (exists) {
        return JsonParser::createErrorResponse("Username already taken");
    } else {
        return JsonParser::createSuccessResponse("Username available");
    }
}

std::string RequestHandler::handleRegisterPushToken(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data.find("token") != data.end() ? data["token"] : "";
    std::string device_token = data.find("device_token") != data.end() ? data["device_token"] : "";
    std::string platform = data.find("platform") != data.end() ? data["platform"] : "android";

    if (token.empty() || device_token.empty()) {
        return JsonParser::createErrorResponse("Token and device_token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User user = db_manager_.getUserById(user_id);

    if (!db_manager_.upsertPushToken(user_id, device_token, platform)) {
        return JsonParser::createErrorResponse("Failed to save push token");
    }

    Logger::getInstance().info("Push token registered for user " + user_id + " (" + platform + ")");
    return JsonParser::createSuccessResponse("Push token registered");
}

std::string RequestHandler::handleDeletePushToken(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data.find("token") != data.end() ? data["token"] : "";
    std::string device_token = data.find("device_token") != data.end() ? data["device_token"] : "";

    if (token.empty() || device_token.empty()) {
        return JsonParser::createErrorResponse("Token and device_token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    if (!db_manager_.deletePushToken(user_id, device_token)) {
        return JsonParser::createErrorResponse("Failed to delete push token");
    }

    Logger::getInstance().info("Push token deleted for user " + user_id);
    return JsonParser::createSuccessResponse("Push token deleted");
}

std::string RequestHandler::serveStaticFile(const std::string& path) {
    // Prevent serving directories or special files (and keep behavior deterministic).
    try {
        fs::path p(path);
        if (!fs::exists(p) || !fs::is_regular_file(p)) {
            Logger::getInstance().warning("Static file not found or not a regular file: " + path);
            return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
        }
    } catch (...) {
        // If filesystem probing fails, fall back to trying to open the file.
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::getInstance().warning("File not found: " + path);
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    std::string file_content = content.str();
    
    // Determine MIME type (safe even when there's no extension)
    std::string extension;
    try {
        extension = fs::path(path).extension().string();
    } catch (...) {
        extension.clear();
    }
    std::string mime_type = getMimeType(extension);
    
    // Build HTTP response
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << mime_type << "\r\n";
    response << "Content-Length: " << file_content.length() << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << file_content;
    
    return response.str();
}

std::string RequestHandler::serveStaticFileWithStatus(const std::string& path, const std::string& status_line) {
    // Prevent serving directories or special files (and keep behavior deterministic).
    try {
        fs::path p(path);
        if (!fs::exists(p) || !fs::is_regular_file(p)) {
            Logger::getInstance().warning("Static file not found or not a regular file: " + path);
            return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
        }
    } catch (...) {
        // If filesystem probing fails, fall back to trying to open the file.
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::getInstance().warning("File not found: " + path);
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
    }

    std::ostringstream content;
    content << file.rdbuf();
    std::string file_content = content.str();

    // Determine MIME type (safe even when there's no extension)
    std::string extension;
    try {
        extension = fs::path(path).extension().string();
    } catch (...) {
        extension.clear();
    }
    std::string mime_type = getMimeType(extension);

    // Build HTTP response
    std::ostringstream response;
    response << status_line << "\r\n";
    response << "Content-Type: " << mime_type << "\r\n";
    response << "Content-Length: " << file_content.length() << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << file_content;

    return response.str();
}

std::string RequestHandler::getMimeType(const std::string& extension) {
    if (extension == ".html") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".json") return "application/json";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".webp") return "image/webp";
    if (extension == ".ogg") return "audio/ogg";
    if (extension == ".mp3") return "audio/mpeg";
    if (extension == ".wav") return "audio/wav";
    if (extension == ".apk") return "application/vnd.android.package-archive";
    if (extension == ".pdf") return "application/pdf";
    if (extension == ".zip") return "application/zip";
    if (extension == ".doc" || extension == ".docx") return "application/msword";
    return "application/octet-stream";
}

const std::string* RequestHandler::findHeaderValueCI(const std::map<std::string, std::string>& headers,
                                                     const std::string& name) const {
    auto it = headers.find(name);
    if (it != headers.end()) {
        return &it->second;
    }
    const std::string target = toLowerCopy(name);
    for (const auto& kv : headers) {
        if (toLowerCopy(kv.first) == target) {
            return &kv.second;
        }
    }
    return nullptr;
}

std::string RequestHandler::extractTokenFromHeaders(const std::map<std::string, std::string>& headers) {
    const std::string* auth_value = findHeaderValueCI(headers, "Authorization");
    if (auth_value) {
        std::string auth = *auth_value;
        if (auth.find("Bearer ") == 0) {
            return auth.substr(7);
        }
    }
    return "";
}

std::string RequestHandler::extractSessionTokenFromHeaders(const std::map<std::string, std::string>& headers) {
    const std::string* cookie_value = findHeaderValueCI(headers, "Cookie");
    if (cookie_value) {
        std::string cookie_token = extractCookieValue(*cookie_value, kSessionTokenCookieName);
        if (!cookie_token.empty()) {
            return cookie_token;
        }
    }

    std::string bearer = extractTokenFromHeaders(headers);
    if (bearer == kSessionTokenPlaceholder) {
        return "";
    }
    return bearer;
}

std::string RequestHandler::maybeInjectSessionToken(const std::string& body,
                                                    const std::map<std::string, std::string>& headers) {
    std::map<std::string, std::string> data = JsonParser::parse(body);
    std::string token = data.count("token") ? data["token"] : "";
    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }

    const std::string header_token = extractSessionTokenFromHeaders(headers);
    if (!header_token.empty()) {
        data["token"] = header_token;
        return JsonParser::stringify(data);
    }

    if (token.empty()) {
        return body;
    }

    data["token"] = token;
    return JsonParser::stringify(data);
}

bool RequestHandler::isSecureRequest(const std::map<std::string, std::string>& headers) const {
    const std::string* proto = findHeaderValueCI(headers, "X-Forwarded-Proto");
    if (proto && toLowerCopy(*proto).find("https") != std::string::npos) {
        return true;
    }
    const std::string* scheme = findHeaderValueCI(headers, "X-Forwarded-Scheme");
    if (scheme && toLowerCopy(*scheme).find("https") != std::string::npos) {
        return true;
    }
    const std::string* ssl = findHeaderValueCI(headers, "X-Forwarded-SSL");
    if (ssl) {
        const std::string value = toLowerCopy(*ssl);
        if (value == "on" || value == "1" || value == "true") {
            return true;
        }
    }
    const std::string* forwarded = findHeaderValueCI(headers, "Forwarded");
    if (forwarded && toLowerCopy(*forwarded).find("proto=https") != std::string::npos) {
        return true;
    }
    const std::string* origin = findHeaderValueCI(headers, "Origin");
    if (origin && toLowerCopy(*origin).rfind("https://", 0) == 0) {
        return true;
    }
    return false;
}

std::string RequestHandler::buildHttpJson(const std::string& body,
                                          const std::map<std::string, std::string>& extra_headers) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Cache-Control: no-store\r\n";
    for (const auto& kv : extra_headers) {
        oss << kv.first << ": " << kv.second << "\r\n";
    }
    oss << "Content-Length: " << body.size() << "\r\n\r\n";
    oss << body;
    return oss.str();
}

std::map<std::string, std::string> RequestHandler::parseFormData(const std::string& body) {
    std::map<std::string, std::string> result;
    std::istringstream iss(body);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, pos));
            std::string value = urlDecode(pair.substr(pos + 1));
            result[key] = value;
        }
    }
    
    return result;
}

std::string RequestHandler::handleAdminGetReports(const std::string& body, const std::map<std::string, std::string>& headers) {
    auto data = JsonParser::parse(body);
    std::string token = data.count("token") ? data.at("token") : "";
    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }
    if (token.empty()) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty()) {
        return JsonParser::createErrorResponse("Admin token required");
    }

    std::string admin_id = auth_manager_.getUserIdFromToken(token);
    if (admin_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    auto admin = db_manager_.getUserById(admin_id);
    if (admin.id.empty() || (admin.role != "admin" && admin.role != "owner" && !admin.is_admin)) {
        return JsonParser::createErrorResponse("Admin role required");
    }
    if (admin.username != "prd") {
        return JsonParser::createErrorResponse("Not authorized");
    }

    int page = 1;
    int page_size = 20;
    std::string status_filter = data.count("status") ? data.at("status") : "";
    try {
        if (data.count("page")) page = std::stoi(data.at("page"));
        if (data.count("page_size")) page_size = std::stoi(data.at("page_size"));
    } catch (...) {
        page = 1;
        page_size = 20;
    }
    if (page < 1) page = 1;
    if (page_size < 1) page_size = 20;

    auto reports = db_manager_.getReportsPaginated(page, page_size, status_filter);
    long long total = db_manager_.countReports(status_filter);
    int total_pages = static_cast<int>(std::ceil(total / static_cast<double>(page_size)));

    std::ostringstream oss;
    oss << "{\"success\":true,"
        << "\"page\":" << page << ","
        << "\"page_size\":" << page_size << ","
        << "\"total\":" << total << ","
        << "\"total_pages\":" << (total_pages < 1 ? 1 : total_pages) << ","
        << "\"items\":[";

    bool first = true;
    for (const auto& r : reports) {
        if (!first) oss << ",";
        first = false;
        const std::string context = r.context_json.empty() ? "[]" : r.context_json;
        oss << "{"
            << "\"id\":\"" << JsonParser::escapeJson(r.id) << "\","
            << "\"status\":\"" << JsonParser::escapeJson(r.status) << "\","
            << "\"reason\":\"" << JsonParser::escapeJson(r.reason) << "\","
            << "\"message_id\":\"" << JsonParser::escapeJson(r.message_id) << "\","
            << "\"message_type\":\"" << JsonParser::escapeJson(r.message_type) << "\","
            << "\"message_content\":\"" << JsonParser::escapeJson(r.message_content) << "\","
            << "\"reported_user_id\":\"" << JsonParser::escapeJson(r.reported_user_id) << "\","
            << "\"reported_username\":\"" << JsonParser::escapeJson(r.reported_username) << "\","
            << "\"reporter_id\":\"" << JsonParser::escapeJson(r.reporter_id) << "\","
            << "\"reporter_username\":\"" << JsonParser::escapeJson(r.reporter_username) << "\","
            << "\"comment\":\"" << JsonParser::escapeJson(r.comment) << "\","
            << "\"resolution_note\":\"" << JsonParser::escapeJson(r.resolution_note) << "\","
            << "\"resolved_by\":\"" << JsonParser::escapeJson(r.resolved_by) << "\","
            << "\"resolved_at\":\"" << JsonParser::escapeJson(r.resolved_at) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(r.created_at) << "\","
            << "\"context\":" << context
            << "}";
    }

    oss << "]}";
    return oss.str();
}

bool RequestHandler::deleteMessageForModeration(const std::string& message_id,
                                                const std::string& message_type,
                                                std::string* out_type) {
    if (message_id.empty()) {
        return false;
    }

    const std::string hint = toLowerCopy(message_type);
    auto setType = [&](const std::string& type) {
        if (out_type) {
            *out_type = type;
        }
    };

    auto deleteDirect = [&]() -> bool {
        auto msg = db_manager_.getMessageById(message_id);
        if (msg.id.empty()) {
            return false;
        }
        if (!db_manager_.deleteMessageById(message_id)) {
            return false;
        }
        if (ws_sender_) {
            if (!msg.sender_id.empty() && !msg.receiver_id.empty()) {
                std::string sender_payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"chat_type\":\"direct\",\"chat_id\":\"" + JsonParser::escapeJson(msg.receiver_id) + "\"}";
                std::string receiver_payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"chat_type\":\"direct\",\"chat_id\":\"" + JsonParser::escapeJson(msg.sender_id) + "\"}";
                ws_sender_(msg.sender_id, sender_payload);
                if (msg.receiver_id != msg.sender_id) {
                    ws_sender_(msg.receiver_id, receiver_payload);
                }
            }
        }
        setType("direct");
        return true;
    };

    auto deleteGroup = [&]() -> bool {
        std::string group_id;
        std::string sender_id;
        if (!db_manager_.getGroupMessageMeta(message_id, group_id, sender_id)) {
            return false;
        }
        if (!db_manager_.deleteGroupMessage(group_id, message_id)) {
            return false;
        }
        if (ws_sender_) {
            auto members = db_manager_.getGroupMembers(group_id);
            std::string payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                "\",\"chat_type\":\"group\",\"chat_id\":\"" + JsonParser::escapeJson(group_id) + "\"}";
            for (const auto& m : members) {
                if (!m.user_id.empty()) ws_sender_(m.user_id, payload);
            }
        }
        setType("group");
        return true;
    };

    auto deleteChannelLegacy = [&]() -> bool {
        std::string channel_id;
        std::string sender_id;
        if (!db_manager_.getChannelMessageMetaLegacy(message_id, channel_id, sender_id)) {
            return false;
        }
        if (!db_manager_.deleteChannelMessageLegacy(channel_id, message_id)) {
            return false;
        }
        if (ws_sender_) {
            std::string payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                "\",\"chat_type\":\"channel\",\"chat_id\":\"" + JsonParser::escapeJson(channel_id) + "\"}";
            auto members = db_manager_.getChannelMembers(channel_id);
            for (const auto& m : members) {
                if (!m.user_id.empty()) ws_sender_(m.user_id, payload);
            }
        }
        setType("channel");
        return true;
    };

    auto deleteChannelV2 = [&]() -> bool {
        std::string chat_id;
        std::string sender_id;
        if (!db_manager_.getChatMessageMetaV2(message_id, chat_id, sender_id)) {
            return false;
        }
        if (!db_manager_.deleteChatMessageV2(chat_id, message_id)) {
            return false;
        }
        if (ws_sender_) {
            std::string payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                "\",\"chat_type\":\"channel\",\"chat_id\":\"" + JsonParser::escapeJson(chat_id) + "\"}";
            auto subs = db_manager_.getChannelSubscriberIds(chat_id);
            for (const auto& uid : subs) {
                if (!uid.empty()) ws_sender_(uid, payload);
            }
        }
        setType("chat_v2");
        return true;
    };

    const bool hint_is_direct = hint.empty() || hint == "direct" || hint == "chat" || hint == "message" ||
        hint == "text" || hint == "voice" || hint == "file" || hint == "image" || hint == "video" || hint == "audio";

    if (hint == "group" && deleteGroup()) return true;
    if (hint == "channel" && deleteChannelLegacy()) return true;
    if (hint == "chat_v2" && deleteChannelV2()) return true;
    if (hint_is_direct && deleteDirect()) return true;

    if (deleteDirect()) return true;
    if (deleteGroup()) return true;
    if (deleteChannelLegacy()) return true;
    if (deleteChannelV2()) return true;

    return false;
}

std::string RequestHandler::handleAdminUpdateReport(const std::string& body, const std::map<std::string, std::string>& headers) {
    auto data = JsonParser::parse(body);
    std::string token = data.count("token") ? data.at("token") : "";
    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }
    if (token.empty()) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty()) {
        return JsonParser::createErrorResponse("Admin token required");
    }

    std::string admin_id = auth_manager_.getUserIdFromToken(token);
    if (admin_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    auto admin = db_manager_.getUserById(admin_id);
    if (admin.id.empty() || (admin.role != "admin" && admin.role != "owner" && !admin.is_admin)) {
        return JsonParser::createErrorResponse("Admin role required");
    }
    if (admin.username != "prd") {
        return JsonParser::createErrorResponse("Not authorized");
    }

    std::string report_id = data["report_id"];
    if (report_id.empty()) {
        return JsonParser::createErrorResponse("report_id required");
    }

    std::string action = data.count("action") ? data.at("action") : "resolve";
    std::string resolution_note = data.count("resolution_note") ? data.at("resolution_note") : "";
    auto report = db_manager_.getReportById(report_id);
    if (report.id.empty()) {
        return JsonParser::createErrorResponse("Report not found");
    }

    std::string status_to_set = data.count("status") ? data.at("status") : "resolved";
    std::transform(status_to_set.begin(), status_to_set.end(), status_to_set.begin(), ::tolower);
    std::transform(action.begin(), action.end(), action.begin(), ::tolower);

    if (action == "dismiss") {
        status_to_set = "dismissed";
        if (resolution_note.empty()) resolution_note = "Dismissed";
    } else if (action == "ban_user") {
        if (!db_manager_.setUserActive(report.reported_user_id, false)) {
            return JsonParser::createErrorResponse("Failed to ban user");
        }
        db_manager_.deleteUserSessionsByUser(report.reported_user_id);
        status_to_set = "resolved";
        if (resolution_note.empty()) resolution_note = "User banned";
    } else if (action == "delete_message") {
        std::string deleted_type;
        if (!deleteMessageForModeration(report.message_id, report.message_type, &deleted_type)) {
            return JsonParser::createErrorResponse("Failed to delete message");
        }
        status_to_set = "resolved";
        if (resolution_note.empty()) resolution_note = "Message deleted";
    } else if (status_to_set != "pending" && status_to_set != "resolved" && status_to_set != "dismissed") {
        return JsonParser::createErrorResponse("Invalid status");
    }

    if (!db_manager_.updateReportStatus(report_id, status_to_set, admin_id, resolution_note)) {
        return JsonParser::createErrorResponse("Failed to update report");
    }

    std::ostringstream oss;
    oss << "{\"success\":true,"
        << "\"report_id\":\"" << JsonParser::escapeJson(report_id) << "\","
        << "\"status\":\"" << JsonParser::escapeJson(status_to_set) << "\","
        << "\"resolution_note\":\"" << JsonParser::escapeJson(resolution_note) << "\"}";
    return oss.str();
}

// API handlers implementations
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
        
        // Получаем полную информацию о пользователе для is_bot
        auto user = db_manager_.getUserById(friend_.id);
        bool is_online = db_manager_.isUserOnline(friend_.id, 300);
        std::string last_activity = db_manager_.getUserLastActivity(friend_.id);
        UserProfile profile;
        db_manager_.getUserProfile(friend_.id, profile);
        std::string display_name = profile.first_name;
        if (!profile.last_name.empty()) {
            if (!display_name.empty()) display_name += " ";
            display_name += profile.last_name;
        }
        if (display_name.empty()) display_name = friend_.username;
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(friend_.id) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(friend_.username) << "\","
            << "\"display_name\":\"" << JsonParser::escapeJson(display_name) << "\","
            << "\"avatar_url\":\"" << JsonParser::escapeJson(friend_.avatar_url) << "\","
            << "\"is_bot\":" << (user.is_bot ? "true" : "false") << ","
            << "\"is_premium\":" << (user.is_premium ? "true" : "false") << ","
            << "\"online\":" << (is_online ? "true" : "false") << ","
            << "\"last_activity\":\"" << JsonParser::escapeJson(last_activity) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(friend_.created_at) << "\"}";
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
    
    // Получаем всех пользователей, с которыми есть переписка (не только друзей)
    auto chatPartners = db_manager_.getChatPartners(user_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"chats\":[";
    
    bool first = true;
    
    // Всегда добавляем избранные сообщения в начало списка
    auto lastSavedMsg = db_manager_.getLastMessage(user_id, user_id);
    std::string time = "Нет сообщений";
    std::string lastMessage = "Нет сообщений";
    if (!lastSavedMsg.id.empty() && lastSavedMsg.created_at.length() >= 16) {
        time = lastSavedMsg.created_at.substr(11, 5);
        lastMessage = lastSavedMsg.content;
    }
    
    oss << "{\"id\":\"" << JsonParser::escapeJson(user_id) << "\","
        << "\"name\":\"Избранные\","
        << "\"display_name\":\"Избранные\","
        << "\"avatar\":\"⭐\","
        << "\"avatar_url\":\"\","
        << "\"lastMessage\":\"" << JsonParser::escapeJson(lastMessage) << "\","
        << "\"time\":\"" << time << "\","
        << "\"unread\":0,"
        << "\"online\":false,"
        << "\"last_activity\":\"\","
        << "\"is_saved_messages\":true}";
    first = false;
    
    for (const auto& partner : chatPartners) {
        // Пропускаем собственный ID, чтобы не дублировать чат "Избранные"
        if (partner.id == user_id) {
            continue;
        }
        if (!first) oss << ",";
        first = false;
        
        auto lastMsg = db_manager_.getLastMessage(user_id, partner.id);
        int unread = db_manager_.getUnreadCount(user_id, partner.id);
        
        std::string time = "Нет сообщений";
        if (!lastMsg.id.empty() && lastMsg.created_at.length() >= 16) {
            time = lastMsg.created_at.substr(11, 5);
        }
        
        bool is_online = db_manager_.isUserOnline(partner.id, 300);  // 5 minutes threshold
        std::string last_activity = db_manager_.getUserLastActivity(partner.id);
        
        // Получаем полную информацию о пользователе для is_bot
        auto user = db_manager_.getUserById(partner.id);
        UserProfile profile;
        db_manager_.getUserProfile(partner.id, profile);
        std::string display_name = profile.first_name;
        if (!profile.last_name.empty()) {
            if (!display_name.empty()) display_name += " ";
            display_name += profile.last_name;
        }
        if (display_name.empty()) display_name = partner.username;
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(partner.id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(partner.username) << "\","
            << "\"display_name\":\"" << JsonParser::escapeJson(display_name) << "\","
            << "\"avatar\":\"" << (partner.username.empty() ? "U" : std::string(1, partner.username[0])) << "\","
            << "\"avatar_url\":\"" << JsonParser::escapeJson(partner.avatar_url) << "\","
            << "\"lastMessage\":\"" << JsonParser::escapeJson(lastMsg.content) << "\","
            << "\"time\":\"" << time << "\","
            << "\"unread\":" << unread << ","
            << "\"is_bot\":" << (user.is_bot ? "true" : "false") << ","
            << "\"is_premium\":" << (user.is_premium ? "true" : "false") << ","
            << "\"online\":" << (is_online ? "true" : "false") << ","
            << "\"last_activity\":\"" << JsonParser::escapeJson(last_activity) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetChatPins(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto pins = db_manager_.getChatPins(user_id);
    std::ostringstream oss;
    oss << "{\"success\":true,\"pins\":[";
    bool first = true;
    for (const auto& pin : pins) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"chat_type\":\"" << JsonParser::escapeJson(pin.chat_type) << "\","
            << "\"chat_id\":\"" << JsonParser::escapeJson(pin.chat_id) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handlePinChat(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string chat_id = data["chat_id"];
    std::string chat_type = "";
    auto it = data.find("chat_type");
    if (it != data.end()) {
        chat_type = it->second;
    } else {
        it = data.find("type");
        if (it != data.end()) {
            chat_type = it->second;
        }
    }
    chat_type = normalizeChatType(chat_type);

    if (token.empty() || chat_id.empty()) {
        return JsonParser::createErrorResponse("Token and chat_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    if (!isValidChatType(chat_type)) {
        return JsonParser::createErrorResponse("Invalid chat_type");
    }

    auto pins = db_manager_.getChatPins(user_id);
    bool already_pinned = false;
    for (const auto& pin : pins) {
        if (pin.chat_id == chat_id && pin.chat_type == chat_type) {
            already_pinned = true;
            break;
        }
    }
    if (!already_pinned) {
        User user = db_manager_.getUserById(user_id);
        const int limit = user.is_premium ? 10 : 3;
        if (static_cast<int>(pins.size()) >= limit) {
            return JsonParser::createErrorResponse("Pinned chats limit reached");
        }
    }

    if (db_manager_.pinChat(user_id, chat_type, chat_id)) {
        return JsonParser::createSuccessResponse("Chat pinned");
    }
    return JsonParser::createErrorResponse("Failed to pin chat");
}

std::string RequestHandler::handleUnpinChat(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string chat_id = data["chat_id"];
    std::string chat_type = "";
    auto it = data.find("chat_type");
    if (it != data.end()) {
        chat_type = it->second;
    } else {
        it = data.find("type");
        if (it != data.end()) {
            chat_type = it->second;
        }
    }
    chat_type = normalizeChatType(chat_type);

    if (token.empty() || chat_id.empty()) {
        return JsonParser::createErrorResponse("Token and chat_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    if (!isValidChatType(chat_type)) {
        return JsonParser::createErrorResponse("Invalid chat_type");
    }

    if (db_manager_.unpinChat(user_id, chat_type, chat_id)) {
        return JsonParser::createSuccessResponse("Chat unpinned");
    }
    return JsonParser::createErrorResponse("Failed to unpin chat");
}

std::string RequestHandler::handleGetChatFolders(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    std::string folders = db_manager_.getChatFoldersJson(user_id);
    if (!looksLikeJsonArray(folders)) {
        folders = "[]";
    }
    std::ostringstream oss;
    oss << "{\"success\":true,\"folders\":" << folders << "}";
    return oss.str();
}

std::string RequestHandler::handleSetChatFolders(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string folders = data["folders"];

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    if (folders.empty()) {
        folders = "[]";
    }
    if (!looksLikeJsonArray(folders)) {
        return JsonParser::createErrorResponse("Invalid folders payload");
    }
    if (folders.size() > 200000) {
        return JsonParser::createErrorResponse("Folders payload too large");
    }

    User user = db_manager_.getUserById(user_id);
    const int limit = user.is_premium ? 20 : 3;
    int folder_count = countTopLevelJsonArrayItems(folders);
    if (folder_count < 0) {
        return JsonParser::createErrorResponse("Invalid folders payload");
    }
    if (folder_count > limit) {
        return JsonParser::createErrorResponse("Folder limit reached");
    }

    if (db_manager_.setChatFoldersJson(user_id, folders)) {
        return JsonParser::createSuccessResponse("Chat folders updated");
    }
    return JsonParser::createErrorResponse("Failed to update chat folders");
}

std::string RequestHandler::handleSetPremium(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    const bool active = parseBoolValue(data["active"], false);
    std::string plan = data.find("plan") != data.end() ? data["plan"] : "";

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor = db_manager_.getUserById(user_id);
    if (actor.id.empty()) {
        return JsonParser::createErrorResponse("User not found");
    }
    if (!actor.is_admin) {
        return JsonParser::createErrorResponse("Premium update not allowed");
    }

    if (!active) {
        plan.clear();
    } else if (plan != "month" && plan != "year" && plan != "trial") {
        return JsonParser::createErrorResponse("Invalid plan");
    }

    if (!db_manager_.setUserPremium(user_id, active, plan)) {
        return JsonParser::createErrorResponse("Failed to update premium status");
    }

    User updated = db_manager_.getUserById(user_id);
    std::map<std::string, std::string> response;
    response["is_premium"] = updated.is_premium ? "true" : "false";
    response["premium_plan"] = updated.premium_plan;
    response["premium_expires_at"] = updated.premium_expires_at;
    return JsonParser::createSuccessResponse("Premium updated", response);
}

std::string RequestHandler::handleCreatePremiumPayment(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string plan = data.find("plan") != data.end() ? data["plan"] : "";
    std::string provider = data.find("provider") != data.end() ? data["provider"] : "";
    provider = toLowerCopy(provider);
    if (provider.empty()) {
        provider = "yoomoney";
    }

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    if (plan != "month" && plan != "year" && plan != "trial") {
        return JsonParser::createErrorResponse("Invalid plan");
    }
    if (provider != "yoomoney" && provider != "stripe") {
        return JsonParser::createErrorResponse("Invalid payment provider");
    }

    if (plan == "trial") {
        User user = db_manager_.getUserById(user_id);
        if (user.id.empty()) {
            return JsonParser::createErrorResponse("User not found");
        }
        if (user.is_premium) {
            return JsonParser::createErrorResponse("Trial not available for active premium");
        }
        if (db_manager_.hasTrialPayment(user_id)) {
            return JsonParser::createErrorResponse("Trial already used. Choose monthly plan.");
        }
    }

    const std::string amount = (plan == "year")
        ? "499.00"
        : (plan == "trial" ? "9.00" : "99.00");

    const char* base_url_env = std::getenv("XIPHER_PUBLIC_URL");
    if (!base_url_env || std::string(base_url_env).empty()) {
        base_url_env = std::getenv("PUBLIC_URL");
    }
    std::string success_url;
    if (base_url_env && *base_url_env) {
        success_url = base_url_env;
        if (!success_url.empty() && success_url.back() == '/') {
            success_url.pop_back();
        }
        success_url += "/chat?premium=success";
    }

    int64_t amount_cents = 0;
    std::string currency = "eur";
    std::string stripe_secret;
    std::string receiver;
    if (provider == "stripe") {
        const char* secret_env = std::getenv("XIPHER_STRIPE_SECRET_KEY");
        if (!secret_env || std::string(secret_env).empty()) {
            secret_env = std::getenv("STRIPE_SECRET_KEY");
        }
        if (!secret_env || std::string(secret_env).empty()) {
            return JsonParser::createErrorResponse("Stripe secret not configured");
        }
        const char* currency_env = std::getenv("XIPHER_STRIPE_CURRENCY");
        if (!currency_env || std::string(currency_env).empty()) {
            currency_env = std::getenv("STRIPE_CURRENCY");
        }
        currency = currency_env && *currency_env ? toLowerCopy(trimEnvValue(currency_env)) : "eur";
        if (!convertRubToCurrencyCents(amount, currency, amount_cents)) {
            return JsonParser::createErrorResponse("Failed to convert amount");
        }
        bool min_applied = applyStripeMinimum(currency, amount_cents);
        if (min_applied) {
            Logger::getInstance().info("Stripe amount raised to minimum for currency " + currency);
        }
        if (success_url.empty()) {
            return JsonParser::createErrorResponse("Public URL not configured");
        }
        stripe_secret = trimEnvValue(secret_env);
    } else {
        const char* receiver_env = std::getenv("XIPHER_YOOMONEY_RECEIVER");
        if (!receiver_env || std::string(receiver_env).empty()) {
            receiver_env = std::getenv("YOOMONEY_WALLET_ID");
        }
        if (!receiver_env || std::string(receiver_env).empty()) {
            return JsonParser::createErrorResponse("YooMoney receiver not configured");
        }
        receiver = receiver_env;
    }

    PremiumPayment payment;
    if (!db_manager_.createPremiumPayment(user_id, plan, amount, payment)) {
        return JsonParser::createErrorResponse("Failed to create payment");
    }

    std::map<std::string, std::string> response;
    response["label"] = payment.label;
    response["sum"] = amount;
    response["plan"] = plan;
    response["provider"] = provider;
    if (provider == "stripe") {
        std::string cancel_url = success_url;
        size_t premium_pos = cancel_url.find("?premium=success");
        if (premium_pos != std::string::npos) {
            cancel_url.replace(premium_pos, std::string("?premium=success").size(), "?premium=cancel");
        }
        std::string plan_label = "Monthly";
        if (plan == "year") {
            plan_label = "Yearly";
        } else if (plan == "trial") {
            plan_label = "Trial";
        }
        std::string product_name = "Xipher Premium " + plan_label;
        std::string form;
        appendFormField(form, "mode", "payment");
        appendFormField(form, "client_reference_id", payment.label);
        appendFormField(form, "success_url", success_url);
        appendFormField(form, "cancel_url", cancel_url);
        appendFormField(form, "payment_method_types[0]", "card");
        appendFormField(form, "line_items[0][quantity]", "1");
        appendFormField(form, "line_items[0][price_data][currency]", currency);
        appendFormField(form, "line_items[0][price_data][unit_amount]", std::to_string(amount_cents));
        appendFormField(form, "line_items[0][price_data][product_data][name]", product_name);
        appendFormField(form, "metadata[label]", payment.label);
        appendFormField(form, "metadata[plan]", plan);
        appendFormField(form, "metadata[user_id]", user_id);
        appendFormField(form, "payment_intent_data[metadata][label]", payment.label);
        appendFormField(form, "payment_intent_data[metadata][plan]", plan);
        appendFormField(form, "payment_intent_data[metadata][user_id]", user_id);

        std::string stripe_body;
        long stripe_code = 0;
        bool ok = stripePostForm("https://api.stripe.com/v1/checkout/sessions",
                                 stripe_secret, form, &stripe_body, &stripe_code);
        if (!ok || stripe_code < 200 || stripe_code >= 300) {
            Logger::getInstance().warning("Stripe session create failed: HTTP " + std::to_string(stripe_code));
            return JsonParser::createErrorResponse("Failed to create Stripe session");
        }
        auto stripe_data = JsonParser::parse(stripe_body);
        std::string checkout_url = stripe_data["url"];
        if (checkout_url.empty()) {
            return JsonParser::createErrorResponse("Stripe session missing URL");
        }
        response["checkout_url"] = checkout_url;
        response["currency"] = currency;
        return JsonParser::createSuccessResponse("Premium payment created", response);
    }

    response["form_action"] = "https://yoomoney.ru/quickpay/confirm";
    response["receiver"] = receiver;
    if (!success_url.empty()) {
        response["success_url"] = success_url;
    }
    return JsonParser::createSuccessResponse("Premium payment created", response);
}

std::string RequestHandler::handleCreatePremiumGiftPayment(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string plan = data.find("plan") != data.end() ? data["plan"] : "";
    std::string provider = data.find("provider") != data.end() ? data["provider"] : "";
    std::string receiver_id = data.find("receiver_id") != data.end() ? data["receiver_id"] : "";
    plan = toLowerCopy(plan);
    provider = toLowerCopy(provider);
    if (provider.empty()) {
        provider = "yoomoney";
    }

    if (token.empty() || receiver_id.empty()) {
        return JsonParser::createErrorResponse("Token and receiver_id required");
    }

    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    if (sender_id == receiver_id) {
        return JsonParser::createErrorResponse("Cannot gift premium to yourself");
    }

    GiftPlanConfig gift_config;
    if (!getGiftPlanConfig(plan, gift_config)) {
        return JsonParser::createErrorResponse("Invalid plan");
    }
    if (provider != "yoomoney" && provider != "stripe") {
        return JsonParser::createErrorResponse("Invalid payment provider");
    }

    User recipient = db_manager_.getUserById(receiver_id);
    if (recipient.id.empty()) {
        return JsonParser::createErrorResponse("Recipient not found");
    }

    const std::string amount = formatGiftAmount(gift_config);

    const char* base_url_env = std::getenv("XIPHER_PUBLIC_URL");
    if (!base_url_env || std::string(base_url_env).empty()) {
        base_url_env = std::getenv("PUBLIC_URL");
    }
    std::string success_url;
    if (base_url_env && *base_url_env) {
        success_url = base_url_env;
        if (!success_url.empty() && success_url.back() == '/') {
            success_url.pop_back();
        }
        success_url += "/chat?gift=success";
    }

    int64_t amount_cents = 0;
    std::string currency = "eur";
    std::string stripe_secret;
    std::string receiver;
    if (provider == "stripe") {
        const char* secret_env = std::getenv("XIPHER_STRIPE_SECRET_KEY");
        if (!secret_env || std::string(secret_env).empty()) {
            secret_env = std::getenv("STRIPE_SECRET_KEY");
        }
        if (!secret_env || std::string(secret_env).empty()) {
            return JsonParser::createErrorResponse("Stripe secret not configured");
        }
        const char* currency_env = std::getenv("XIPHER_STRIPE_CURRENCY");
        if (!currency_env || std::string(currency_env).empty()) {
            currency_env = std::getenv("STRIPE_CURRENCY");
        }
        currency = currency_env && *currency_env ? toLowerCopy(trimEnvValue(currency_env)) : "eur";
        if (!convertRubToCurrencyCents(amount, currency, amount_cents)) {
            return JsonParser::createErrorResponse("Failed to convert amount");
        }
        bool min_applied = applyStripeMinimum(currency, amount_cents);
        if (min_applied) {
            Logger::getInstance().info("Stripe amount raised to minimum for currency " + currency);
        }
        if (success_url.empty()) {
            return JsonParser::createErrorResponse("Public URL not configured");
        }
        stripe_secret = trimEnvValue(secret_env);
    } else {
        const char* receiver_env = std::getenv("XIPHER_YOOMONEY_RECEIVER");
        if (!receiver_env || std::string(receiver_env).empty()) {
            receiver_env = std::getenv("YOOMONEY_WALLET_ID");
        }
        if (!receiver_env || std::string(receiver_env).empty()) {
            return JsonParser::createErrorResponse("YooMoney receiver not configured");
        }
        receiver = receiver_env;
    }

    PremiumPayment payment;
    if (!db_manager_.createPremiumGiftPayment(sender_id, receiver_id, plan, amount, payment)) {
        return JsonParser::createErrorResponse("Failed to create payment");
    }

    std::map<std::string, std::string> response;
    response["label"] = payment.label;
    response["sum"] = amount;
    response["plan"] = plan;
    response["provider"] = provider;
    response["receiver_id"] = receiver_id;
    if (provider == "stripe") {
        std::string cancel_url = success_url;
        size_t gift_pos = cancel_url.find("?gift=success");
        if (gift_pos != std::string::npos) {
            cancel_url.replace(gift_pos, std::string("?gift=success").size(), "?gift=cancel");
        }
        std::string plan_label = plan == "year" ? "1 year" : (plan == "half" ? "6 months" : "1 month");
        std::string product_name = "Xipher Premium Gift " + plan_label;
        std::string form;
        appendFormField(form, "mode", "payment");
        appendFormField(form, "client_reference_id", payment.label);
        appendFormField(form, "success_url", success_url);
        appendFormField(form, "cancel_url", cancel_url);
        appendFormField(form, "payment_method_types[0]", "card");
        appendFormField(form, "line_items[0][quantity]", "1");
        appendFormField(form, "line_items[0][price_data][currency]", currency);
        appendFormField(form, "line_items[0][price_data][unit_amount]", std::to_string(amount_cents));
        appendFormField(form, "line_items[0][price_data][product_data][name]", product_name);
        appendFormField(form, "metadata[label]", payment.label);
        appendFormField(form, "metadata[plan]", plan);
        appendFormField(form, "metadata[user_id]", sender_id);
        appendFormField(form, "metadata[gift_receiver_id]", receiver_id);
        appendFormField(form, "payment_intent_data[metadata][label]", payment.label);
        appendFormField(form, "payment_intent_data[metadata][plan]", plan);
        appendFormField(form, "payment_intent_data[metadata][user_id]", sender_id);
        appendFormField(form, "payment_intent_data[metadata][gift_receiver_id]", receiver_id);

        std::string stripe_body;
        long stripe_code = 0;
        bool ok = stripePostForm("https://api.stripe.com/v1/checkout/sessions",
                                 stripe_secret, form, &stripe_body, &stripe_code);
        if (!ok || stripe_code < 200 || stripe_code >= 300) {
            Logger::getInstance().warning("Stripe session create failed: HTTP " + std::to_string(stripe_code));
            return JsonParser::createErrorResponse("Failed to create Stripe session");
        }
        auto stripe_data = JsonParser::parse(stripe_body);
        std::string checkout_url = stripe_data["url"];
        if (checkout_url.empty()) {
            return JsonParser::createErrorResponse("Stripe session missing URL");
        }
        response["checkout_url"] = checkout_url;
        response["currency"] = currency;
        return JsonParser::createSuccessResponse("Premium gift payment created", response);
    }

    response["form_action"] = "https://yoomoney.ru/quickpay/confirm";
    response["receiver"] = receiver;
    if (!success_url.empty()) {
        response["success_url"] = success_url;
    }
    return JsonParser::createSuccessResponse("Premium gift payment created", response);
}

bool RequestHandler::sendPremiumGiftMessage(const PremiumPayment& payment) {
    if (payment.gift_receiver_id.empty()) return true;

    GiftPlanConfig gift_config;
    if (!getGiftPlanConfig(payment.plan, gift_config)) {
        Logger::getInstance().warning("Invalid gift plan for payment " + payment.label);
        return false;
    }

    const std::string& sender_id = payment.user_id;
    const std::string& receiver_id = payment.gift_receiver_id;
    User sender = db_manager_.getUserById(sender_id);
    User receiver = db_manager_.getUserById(receiver_id);

    std::ostringstream payload;
    payload << "{\"plan\":\"" << JsonParser::escapeJson(payment.plan) << "\","
            << "\"months\":" << gift_config.months << ","
            << "\"price\":" << gift_config.price << ","
            << "\"from\":\"" << JsonParser::escapeJson(sender.username) << "\","
            << "\"to\":\"" << JsonParser::escapeJson(receiver.username) << "\"}";
    const std::string content = kPremiumGiftPrefix + payload.str();

    bool sendResult = db_manager_.sendMessage(sender_id, receiver_id, content, "text");
    if (!sendResult) {
        Logger::getInstance().warning("Failed to send premium gift message for payment " + payment.label);
        return false;
    }

    Message lastMessage = db_manager_.getLastMessage(sender_id, receiver_id);
    if (!lastMessage.id.empty() && ws_sender_) {
        const bool is_saved_messages = sender_id == receiver_id;
        const std::string chat_type = is_saved_messages ? "saved_messages" : "chat";
        const std::string status = lastMessage.is_read ? "read" : (lastMessage.is_delivered ? "delivered" : "sent");
        const std::string time = lastMessage.created_at.length() >= 16 ? lastMessage.created_at.substr(11, 5) : "";
        auto send_ws_payload = [&](const std::string& target_user_id, const std::string& chat_id) {
            if (target_user_id.empty()) return;
            std::ostringstream ws_payload;
            ws_payload << "{\"type\":\"new_message\","
                       << "\"chat_type\":\"" << chat_type << "\","
                       << "\"chat_id\":\"" << JsonParser::escapeJson(chat_id) << "\","
                       << "\"id\":\"" << JsonParser::escapeJson(lastMessage.id) << "\","
                       << "\"message_id\":\"" << JsonParser::escapeJson(lastMessage.id) << "\","
                       << "\"temp_id\":\"\","
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

    if (receiver_id != sender_id) {
        try {
            auto tokens = db_manager_.getPushTokensForUser(receiver_id);
            if (!tokens.empty()) {
                bool fcm_ready = fcm_client_.isReady();
                bool rustore_ready = rustore_client_.isReady();
                auto summary = summarizePushPlatforms(tokens);
                if ((summary.has_fcm && fcm_ready) || (summary.has_rustore && rustore_ready)) {
                    std::string sender_name = !sender.username.empty() ? sender.username : "Xipher";
                    const std::string body_text = "Premium в подарок";
                    std::map<std::string, std::string> push_payload;
                    push_payload["type"] = "message";
                    push_payload["chat_id"] = sender_id;
                    push_payload["chat_title"] = sender_name;
                    push_payload["sender_id"] = sender_id;
                    push_payload["chat_type"] = "chat";
                    push_payload["message_id"] = lastMessage.id;
                    push_payload["message_type"] = "text";
                    push_payload["title"] = sender_name;
                    push_payload["body"] = body_text;

                    sendPushTokensForUser(db_manager_, fcm_client_, rustore_client_, receiver_id, tokens,
                                          sender_name, body_text, push_payload, "channel_messages", "HIGH",
                                          fcm_ready, rustore_ready);
                }
            }
        } catch (...) {
            Logger::getInstance().warning("Failed to send premium gift push notification");
        }
    }

    return true;
}

std::string RequestHandler::handleYooMoneyNotification(const std::string& body) {
    auto data = parseFormData(body);
    auto getField = [&](const std::string& key) -> std::string {
        auto it = data.find(key);
        return it == data.end() ? "" : it->second;
    };

    const std::string notification_type = getField("notification_type");
    const std::string operation_id = getField("operation_id");
    const std::string amount = getField("amount");
    const std::string withdraw_amount = getField("withdraw_amount");
    const std::string currency = getField("currency");
    const std::string datetime = getField("datetime");
    const std::string sender = getField("sender");
    const std::string codepro = getField("codepro");
    const std::string label = getField("label");
    const std::string sha1_hash = getField("sha1_hash");

    const char* secret_env = std::getenv("XIPHER_YOOMONEY_NOTIFICATION_SECRET");
    if (!secret_env || std::string(secret_env).empty()) {
        secret_env = std::getenv("YOOMONEY_NOTIFICATION_SECRET");
    }
    if (!secret_env || std::string(secret_env).empty()) {
        return buildHttpResponse(500, JsonParser::createErrorResponse("Notification secret not configured"), "application/json");
    }

    if (notification_type != "p2p-incoming" && notification_type != "card-incoming") {
        return buildHttpResponse(400, JsonParser::createErrorResponse("Unsupported notification type"), "application/json");
    }
    if (currency != "643") {
        return buildHttpResponse(400, JsonParser::createErrorResponse("Unsupported currency"), "application/json");
    }
    if (label.empty() || sha1_hash.empty()) {
        return buildHttpResponse(400, JsonParser::createErrorResponse("Missing notification fields"), "application/json");
    }

    const std::string signature =
        notification_type + "&" + operation_id + "&" + amount + "&" + currency + "&" + datetime + "&" +
        sender + "&" + codepro + "&" + std::string(secret_env) + "&" + label;
    const std::string computed_hash = sha1Hex(signature);
    if (toLowerCopy(sha1_hash) != computed_hash) {
        Logger::getInstance().warning("YooMoney notification signature mismatch for label " + label);
        return buildHttpResponse(403, JsonParser::createErrorResponse("Invalid signature"), "application/json");
    }

    PremiumPayment payment;
    if (!db_manager_.getPremiumPaymentByLabel(label, payment)) {
        Logger::getInstance().warning("YooMoney notification with unknown label: " + label);
        return buildHttpResponse(200, JsonParser::createSuccessResponse("Unknown label"), "application/json");
    }
    if (payment.status == "paid") {
        return buildHttpResponse(200, JsonParser::createSuccessResponse("Already processed"), "application/json");
    }

    auto parseAmount = [](const std::string& raw, double& out) -> bool {
        if (raw.empty()) return false;
        std::string normalized = raw;
        std::replace(normalized.begin(), normalized.end(), ',', '.');
        char* end = nullptr;
        out = std::strtod(normalized.c_str(), &end);
        return end && *end == '\0';
    };

    const std::string paid_amount_str = withdraw_amount.empty() ? amount : withdraw_amount;
    double paid_amount = 0.0;
    double expected_amount = 0.0;
    if (!parseAmount(paid_amount_str, paid_amount) || !parseAmount(payment.amount, expected_amount)) {
        return buildHttpResponse(400, JsonParser::createErrorResponse("Invalid amount"), "application/json");
    }
    if (paid_amount + 0.001 < expected_amount) {
        Logger::getInstance().warning("YooMoney amount mismatch for label " + label);
        return buildHttpResponse(400, JsonParser::createErrorResponse("Amount mismatch"), "application/json");
    }

    if (!db_manager_.markPremiumPaymentPaid(label, operation_id, notification_type)) {
        PremiumPayment refreshed;
        if (db_manager_.getPremiumPaymentByLabel(label, refreshed) && refreshed.status == "paid") {
            return buildHttpResponse(200, JsonParser::createSuccessResponse("Already processed"), "application/json");
        }
        return buildHttpResponse(500, JsonParser::createErrorResponse("Failed to update payment"), "application/json");
    }

    const std::string recipient_id = payment.gift_receiver_id.empty() ? payment.user_id : payment.gift_receiver_id;
    if (!db_manager_.setUserPremium(recipient_id, true, payment.plan)) {
        Logger::getInstance().warning("Failed to activate premium after payment: " + label);
        db_manager_.resetPremiumPaymentStatus(label);
        return buildHttpResponse(500, JsonParser::createErrorResponse("Failed to activate premium"), "application/json");
    }
    if (!payment.gift_receiver_id.empty()) {
        if (!sendPremiumGiftMessage(payment)) {
            Logger::getInstance().warning("Failed to send premium gift message after payment: " + label);
        }
    }

    return buildHttpResponse(200, JsonParser::createSuccessResponse("OK"), "application/json");
}

std::string RequestHandler::handleStripeWebhook(const std::string& body, const std::map<std::string, std::string>& headers) {
    const char* secret_env = std::getenv("XIPHER_STRIPE_WEBHOOK_SECRET");
    if (!secret_env || std::string(secret_env).empty()) {
        secret_env = std::getenv("STRIPE_WEBHOOK_SECRET");
    }
    if (!secret_env || std::string(secret_env).empty()) {
        return buildHttpResponse(500, JsonParser::createErrorResponse("Stripe webhook secret not configured"), "application/json");
    }
    const std::string* sig_header = findHeaderValueCI(headers, "Stripe-Signature");
    if (!sig_header || sig_header->empty()) {
        return buildHttpResponse(400, JsonParser::createErrorResponse("Missing Stripe-Signature"), "application/json");
    }
    if (!verifyStripeSignature(body, *sig_header, trimEnvValue(secret_env), 300)) {
        return buildHttpResponse(403, JsonParser::createErrorResponse("Invalid signature"), "application/json");
    }

    auto data = JsonParser::parse(body);
    std::string event_type;
    extractJsonStringValue(body, "type", event_type);
    if (event_type.empty()) {
        event_type = data["type"];
    }
    if (event_type != "checkout.session.completed" && event_type != "checkout.session.async_payment_succeeded") {
        return buildHttpResponse(200, JsonParser::createSuccessResponse("Ignored"), "application/json");
    }
    std::string payment_status;
    extractJsonStringValue(body, "payment_status", payment_status);
    if (payment_status.empty()) {
        payment_status = data["payment_status"];
    }
    payment_status = toLowerCopy(payment_status);
    std::string status;
    extractJsonStringValue(body, "status", status);
    if (status.empty()) {
        status = data["status"];
    }
    status = toLowerCopy(status);
    if (!payment_status.empty() && payment_status != "paid") {
        return buildHttpResponse(200, JsonParser::createSuccessResponse("Pending"), "application/json");
    }
    if (payment_status.empty() && !status.empty() && status != "complete") {
        return buildHttpResponse(200, JsonParser::createSuccessResponse("Pending"), "application/json");
    }

    std::string label;
    extractJsonStringValue(body, "client_reference_id", label);
    if (label.empty()) {
        extractJsonStringValue(body, "label", label);
    }
    if (label.empty()) {
        label = data["client_reference_id"];
    }
    if (label.empty()) {
        label = data["label"];
    }
    if (label.empty()) {
        return buildHttpResponse(400, JsonParser::createErrorResponse("Missing payment label"), "application/json");
    }

    PremiumPayment payment;
    if (!db_manager_.getPremiumPaymentByLabel(label, payment)) {
        Logger::getInstance().warning("Stripe webhook with unknown label: " + label);
        return buildHttpResponse(200, JsonParser::createSuccessResponse("Unknown label"), "application/json");
    }
    if (payment.status == "paid") {
        return buildHttpResponse(200, JsonParser::createSuccessResponse("Already processed"), "application/json");
    }

    const char* currency_env = std::getenv("XIPHER_STRIPE_CURRENCY");
    if (!currency_env || std::string(currency_env).empty()) {
        currency_env = std::getenv("STRIPE_CURRENCY");
    }
    std::string expected_currency = currency_env && *currency_env
        ? toLowerCopy(trimEnvValue(currency_env))
        : "eur";
    std::string currency;
    extractJsonStringValue(body, "currency", currency);
    if (currency.empty()) {
        currency = data["currency"];
    }
    currency = toLowerCopy(currency);
    if (!currency.empty() && currency != expected_currency) {
        Logger::getInstance().warning("Stripe currency mismatch for label " + label);
        return buildHttpResponse(400, JsonParser::createErrorResponse("Currency mismatch"), "application/json");
    }
    int64_t expected_cents = 0;
    int64_t paid_cents = 0;
    std::string amount_total = data["amount_total"];
    int64_t parsed_total = 0;
    if (extractJsonIntValue(body, "amount_total", parsed_total)) {
        paid_cents = parsed_total;
    } else if (!amount_total.empty()) {
        try {
            paid_cents = std::stoll(amount_total);
        } catch (...) {
            return buildHttpResponse(400, JsonParser::createErrorResponse("Invalid amount"), "application/json");
        }
    }
    if (!convertRubToCurrencyCents(payment.amount, expected_currency, expected_cents)) {
        return buildHttpResponse(500, JsonParser::createErrorResponse("Failed to convert amount"), "application/json");
    }
    applyStripeMinimum(expected_currency, expected_cents);
    if (paid_cents > 0 && paid_cents + 1 < expected_cents) {
        Logger::getInstance().warning("Stripe amount mismatch for label " + label);
        return buildHttpResponse(400, JsonParser::createErrorResponse("Amount mismatch"), "application/json");
    }

    std::string operation_id;
    extractJsonStringValue(body, "payment_intent", operation_id);
    if (operation_id.empty()) {
        extractJsonStringValue(body, "id", operation_id);
    }
    if (operation_id.empty()) {
        operation_id = data["payment_intent"];
    }
    if (operation_id.empty()) {
        operation_id = data["id"];
    }

    if (!db_manager_.markPremiumPaymentPaid(label, operation_id, "stripe")) {
        PremiumPayment refreshed;
        if (db_manager_.getPremiumPaymentByLabel(label, refreshed) && refreshed.status == "paid") {
            return buildHttpResponse(200, JsonParser::createSuccessResponse("Already processed"), "application/json");
        }
        return buildHttpResponse(500, JsonParser::createErrorResponse("Failed to update payment"), "application/json");
    }

    const std::string recipient_id = payment.gift_receiver_id.empty() ? payment.user_id : payment.gift_receiver_id;
    if (!db_manager_.setUserPremium(recipient_id, true, payment.plan)) {
        Logger::getInstance().warning("Failed to activate premium after Stripe payment: " + label);
        db_manager_.resetPremiumPaymentStatus(label);
        return buildHttpResponse(500, JsonParser::createErrorResponse("Failed to activate premium"), "application/json");
    }
    if (!payment.gift_receiver_id.empty()) {
        if (!sendPremiumGiftMessage(payment)) {
            Logger::getInstance().warning("Failed to send premium gift message after Stripe payment: " + label);
        }
    }

    return buildHttpResponse(200, JsonParser::createSuccessResponse("OK"), "application/json");
}

std::string RequestHandler::handleGetMessages(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string friend_id = data["friend_id"];
    
    if (token.empty() || friend_id.empty()) {
        return JsonParser::createErrorResponse("Token and friend_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        Logger::getInstance().warning("Invalid token in handleGetMessages: " + token.substr(0, 10) + "...");
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Разрешаем получать сообщения с самим собой для избранных сообщений
    // if (user_id == friend_id) {
    //     return JsonParser::createErrorResponse("Cannot get messages with yourself");
    // }
    
    auto messages = db_manager_.getMessages(user_id, friend_id, 50);
    if (user_id != friend_id) {
        db_manager_.markMessagesAsDelivered(user_id, friend_id);
    }
    db_manager_.markMessagesAsRead(user_id, friend_id);

    bool peer_allows_read_receipts = true;
    if (user_id != friend_id) {
        UserPrivacySettings peer_privacy;
        if (db_manager_.getUserPrivacy(friend_id, peer_privacy)) {
            peer_allows_read_receipts = peer_privacy.send_read_receipts;
        }
    }

    // Build a map of message id -> message for reply lookups
    std::unordered_map<std::string, const Message*> messageMap;
    for (const auto& msg : messages) {
        messageMap[msg.id] = &msg;
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"messages\":[";
    
    bool first = true;
    for (const auto& msg : messages) {
        if (!first) oss << ",";
        first = false;
        
        std::string time = msg.created_at.length() >= 16 ? msg.created_at.substr(11, 5) : "";
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

        // Get reply message info if exists
        std::string reply_content = "";
        std::string reply_sender_name = "";
        if (!msg.reply_to_message_id.empty()) {
            auto it = messageMap.find(msg.reply_to_message_id);
            if (it != messageMap.end()) {
                const Message* replyMsg = it->second;
                reply_content = replyMsg->content.length() > 100 ? replyMsg->content.substr(0, 100) + "..." : replyMsg->content;
                // Get sender username
                User replyUser = db_manager_.getUserById(replyMsg->sender_id);
                if (!replyUser.id.empty()) {
                    reply_sender_name = replyUser.username;
                }
            } else {
                // Message not in current batch, fetch from DB
                Message replyMsg = db_manager_.getMessageById(msg.reply_to_message_id);
                if (!replyMsg.id.empty()) {
                    reply_content = replyMsg.content.length() > 100 ? replyMsg.content.substr(0, 100) + "..." : replyMsg.content;
                    User replyUser = db_manager_.getUserById(replyMsg.sender_id);
                    if (!replyUser.id.empty()) {
                        reply_sender_name = replyUser.username;
                    }
                }
            }
        }
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(msg.id) << "\","
            << "\"sender_id\":\"" << JsonParser::escapeJson(msg.sender_id) << "\","
            << "\"sent\":" << (isSent ? "true" : "false") << ","
            << "\"status\":\"" << JsonParser::escapeJson(status) << "\","
            << "\"is_read\":" << ((canExposeRead && msg.is_read) ? "true" : "false") << ","
            << "\"is_delivered\":" << (msg.is_delivered ? "true" : "false") << ","
            << "\"content\":\"" << JsonParser::escapeJson(msg.content) << "\","
            << "\"message_type\":\"" << JsonParser::escapeJson(msg.message_type) << "\","
            << "\"file_path\":\"" << JsonParser::escapeJson(msg.file_path) << "\","
            << "\"file_name\":\"" << JsonParser::escapeJson(msg.file_name) << "\","
            << "\"file_size\":" << msg.file_size << ","
            << "\"reply_to_message_id\":\"" << JsonParser::escapeJson(msg.reply_to_message_id) << "\","
            << "\"reply_content\":\"" << JsonParser::escapeJson(reply_content) << "\","
            << "\"reply_sender_name\":\"" << JsonParser::escapeJson(reply_sender_name) << "\","
            << "\"time\":\"" << time << "\","
            << "\"is_pinned\":" << (msg.is_pinned ? "true" : "false") << "}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSendMessage(const std::string& body) {
    Logger::getInstance().info("handleSendMessage called, body length: " + std::to_string(body.length()));
    auto data = JsonParser::parse(body);
    std::string token = data.find("token") != data.end() ? data["token"] : "";
    std::string receiver_id = data.find("receiver_id") != data.end() ? data["receiver_id"] : "";
    std::string content = data.find("content") != data.end() ? data["content"] : "";
    std::string message_type = data.find("message_type") != data.end() ? data["message_type"] : "text";
    std::string file_path = data.find("file_path") != data.end() ? data["file_path"] : "";
    std::string file_name = data.find("file_name") != data.end() ? data["file_name"] : "";
    std::string temp_id = data.find("temp_id") != data.end() ? data["temp_id"] : "";
    long long file_size = 0;
    if (data.find("file_size") != data.end()) {
        try {
            file_size = std::stoll(data["file_size"]);
        } catch (...) {
            file_size = 0;
        }
    }
    std::string reply_to_message_id = data.find("reply_to_message_id") != data.end() ? data["reply_to_message_id"] : "";
    std::string reply_markup = extractJsonValueForKey(body, "reply_markup");
    if (reply_markup.empty() && data.find("reply_markup") != data.end()) {
        std::string reply_markup_value = data["reply_markup"];
        if (!reply_markup_value.empty()) {
            reply_markup = reply_markup_value;
        }
    }
    reply_markup = trimString(reply_markup);
    if (!reply_markup.empty()) {
        if (reply_markup[0] == '[') {
            reply_markup = "{\"inline_keyboard\":" + reply_markup + "}";
        } else if (reply_markup[0] != '{') {
            reply_markup.clear();
        }
    }
    
    Logger::getInstance().info("Parsed data - token: " + (token.empty() ? "empty" : token.substr(0, 10) + "...") + 
                               ", receiver_id: " + receiver_id + ", content length: " + std::to_string(content.length()));
    
    if (token.empty() || receiver_id.empty() || content.empty()) {
        std::string token_status = token.empty() ? "empty" : "present";
        std::string receiver_status = receiver_id.empty() ? "empty" : "present";
        std::string content_status = content.empty() ? "empty" : "present";
        Logger::getInstance().warning("Missing required fields - token: " + token_status + 
                                      ", receiver_id: " + receiver_status + 
                                      ", content: " + content_status);
        return JsonParser::createErrorResponse("Token, receiver_id and content required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    Logger::getInstance().info("Token validation result - sender_id: " + (sender_id.empty() ? "empty (invalid token)" : sender_id));
    if (sender_id.empty()) {
        Logger::getInstance().warning("Invalid token in handleSendMessage: " + token.substr(0, 10) + "...");
        return JsonParser::createErrorResponse("Invalid token. Please login again.");
    }
    
    // Разрешаем отправку сообщений самому себе для избранных сообщений
    // if (sender_id == receiver_id) {
    //     return JsonParser::createErrorResponse("Cannot send message to yourself");
    // }
    
    // Обновляем активность отправителя
    db_manager_.updateLastActivity(sender_id);
    
    // Логируем информацию для отладки
    Logger::getInstance().info("Attempting to send message - sender_id: " + sender_id + ", receiver_id: " + receiver_id + 
                               ", is_saved_messages: " + (sender_id == receiver_id ? "true" : "false"));
    
    bool sendResult = db_manager_.sendMessage(sender_id, receiver_id, content, message_type, file_path, file_name, file_size, reply_to_message_id, "", "", "", reply_markup);
    
    if (sendResult) {
        Logger::getInstance().info("Message sent successfully");
        
        // Получаем только что сохраненное сообщение, чтобы вернуть корректный ID и время
        Message lastMessage = db_manager_.getLastMessage(sender_id, receiver_id);
        if (lastMessage.id.empty()) {
            return JsonParser::createSuccessResponse("Message sent");
        }

        std::string time = lastMessage.created_at.length() >= 16 ? lastMessage.created_at.substr(11, 5) : "";

        std::ostringstream oss;
        oss << "{\"success\":true,"
            << "\"message\":\"Message sent\","
            << "\"message_id\":\"" << JsonParser::escapeJson(lastMessage.id) << "\","
            << "\"temp_id\":\"" << JsonParser::escapeJson(temp_id) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(lastMessage.created_at) << "\","
            << "\"time\":\"" << JsonParser::escapeJson(time) << "\","
            << "\"status\":\"sent\","
            << "\"is_read\":false,"
            << "\"is_delivered\":false,"
            << "\"content\":\"" << JsonParser::escapeJson(lastMessage.content) << "\","
            << "\"message_type\":\"" << JsonParser::escapeJson(lastMessage.message_type) << "\","
            << "\"file_path\":\"" << JsonParser::escapeJson(lastMessage.file_path) << "\","
            << "\"file_name\":\"" << JsonParser::escapeJson(lastMessage.file_name) << "\","
            << "\"file_size\":" << lastMessage.file_size << ","
            << "\"reply_to_message_id\":\"" << JsonParser::escapeJson(lastMessage.reply_to_message_id) << "\"}";
        const std::string response = oss.str();

        // Lightweight bot runtime: if the receiver is a Bot Builder bot user, let it react.
        // IMPORTANT: run after we computed the response, otherwise getLastMessage() might return the bot reply.
        try {
            auto bot = db_manager_.getBotBuilderBotByUserId(receiver_id);
            if (!bot.id.empty()) {
                LiteBotRuntime::onDirectMessage(db_manager_, bot, sender_id, content);
            }
        } catch (...) {
            // Don't break message sending if bot runtime fails.
        }

        if (ws_sender_) {
            const bool is_saved_messages = sender_id == receiver_id;
            const std::string chat_type = is_saved_messages ? "saved_messages" : "chat";
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

        // Push notification for offline clients.
        if (receiver_id != sender_id) {
            try {
                auto tokens = db_manager_.getPushTokensForUser(receiver_id);
                if (tokens.empty()) {
                    Logger::getInstance().warning("Push skipped: no tokens for receiver " + receiver_id);
                } else {
                    bool fcm_ready = fcm_client_.isReady();
                    bool rustore_ready = rustore_client_.isReady();
                    auto summary = summarizePushPlatforms(tokens);
                    if (summary.has_fcm && !fcm_ready) {
                        Logger::getInstance().warning("Push skipped: FCM not ready for receiver " + receiver_id);
                    }
                    if (summary.has_rustore && !rustore_ready) {
                        Logger::getInstance().warning("Push skipped: RuStore not ready for receiver " + receiver_id);
                    }

                    User sender_user = db_manager_.getUserById(sender_id);
                    std::string sender_name = !sender_user.username.empty() ? sender_user.username : "Xipher";

                    std::string body_text = content;
                    if (message_type != "text") {
                        if (message_type == "voice") {
                            body_text = "Voice message";
                        } else if (message_type == "file") {
                            body_text = file_name.empty() ? "File" : ("File: " + file_name);
                        } else if (message_type == "image") {
                            body_text = "Photo";
                        } else {
                            body_text = "Message";
                        }
                    }

                    if (body_text.size() > 120) {
                        body_text = body_text.substr(0, 117) + "...";
                    }

                    std::map<std::string, std::string> payload;
                    payload["type"] = "message";
                    payload["chat_id"] = sender_id;
                    payload["chat_title"] = sender_name;
                    payload["sender_id"] = sender_id;
                    payload["chat_type"] = "chat";
                    payload["message_id"] = lastMessage.id;
                    payload["message_type"] = message_type;
                    payload["title"] = sender_name;
                    payload["body"] = body_text;

                    sendPushTokensForUser(db_manager_, fcm_client_, rustore_client_, receiver_id, tokens,
                                          sender_name, body_text, payload, "channel_messages", "HIGH",
                                          fcm_ready, rustore_ready);
                }
            } catch (...) {
                Logger::getInstance().warning("Failed to send push message notification");
            }
        }

        return response;
    } else {
        Logger::getInstance().error("Failed to send message - sender_id: " + sender_id + ", receiver_id: " + receiver_id);
        return JsonParser::createErrorResponse("Failed to send message. Please try again.");
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

    // Notify peer via WebSocket if available
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

std::string RequestHandler::handleDeleteGroupMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string group_id = data.find("group_id") != data.end() ? data["group_id"] : "";

    if (token.empty() || message_id.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("token, message_id and group_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    std::string msg_group_id;
    std::string sender_id;
    if (!db_manager_.getGroupMessageMeta(message_id, msg_group_id, sender_id) || msg_group_id != group_id) {
        return JsonParser::createErrorResponse("Message not found");
    }

    if (sender_id != user_id) {
        auto member = db_manager_.getGroupMember(group_id, user_id);
        if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
            return JsonParser::createErrorResponse("Permission denied");
        }
    }

    if (!db_manager_.deleteGroupMessage(group_id, message_id)) {
        return JsonParser::createErrorResponse("Failed to delete message");
    }

    if (ws_sender_) {
        auto members = db_manager_.getGroupMembers(group_id);
        std::string payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
            "\",\"chat_type\":\"group\",\"chat_id\":\"" + JsonParser::escapeJson(group_id) + "\"}";
        for (const auto& m : members) {
            if (!m.user_id.empty()) ws_sender_(m.user_id, payload);
        }
    }

    return JsonParser::createSuccessResponse("Message deleted");
}

std::string RequestHandler::handleDeleteChannelMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string channel_id = data.find("channel_id") != data.end() ? data["channel_id"] : "";

    if (token.empty() || message_id.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("token, message_id and channel_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat;
    std::string resolved_channel_id = channel_id;
    bool is_v2 = resolveChannelV2(db_manager_, channel_id, &chat, &resolved_channel_id);

    // Fetch sender + verify message belongs to this chat/channel
    std::string sender_id;
    std::string msg_chat_or_channel;
    if (is_v2) {
        if (!db_manager_.getChatMessageMetaV2(message_id, msg_chat_or_channel, sender_id) ||
            msg_chat_or_channel != resolved_channel_id) {
            return JsonParser::createErrorResponse("Message not found");
        }
    } else {
        if (!db_manager_.getChannelMessageMetaLegacy(message_id, msg_chat_or_channel, sender_id) ||
            msg_chat_or_channel != resolved_channel_id) {
            return JsonParser::createErrorResponse("Message not found");
        }
    }

    // Permission: author can delete own; admins/creator can delete any
    if (sender_id != user_id) {
        DatabaseManager::ChannelMember member;
        std::string perm_error;
        if (!checkChannelPermission(user_id, resolved_channel_id, ChannelPermission::ManageSettings, &member, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
    }

    bool deleted = false;
    if (is_v2) {
        deleted = db_manager_.deleteChatMessageV2(resolved_channel_id, message_id);
    } else {
        deleted = db_manager_.deleteChannelMessageLegacy(resolved_channel_id, message_id);
    }

    if (!deleted) {
        return JsonParser::createErrorResponse("Failed to delete message");
    }

    if (ws_sender_) {
        std::string payload = "{\"type\":\"message_deleted\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
            "\",\"chat_type\":\"channel\",\"chat_id\":\"" + JsonParser::escapeJson(resolved_channel_id) + "\"}";
        if (is_v2) {
            auto subs = db_manager_.getChannelSubscriberIds(resolved_channel_id);
            for (const auto& uid : subs) {
                if (!uid.empty()) ws_sender_(uid, payload);
            }
        } else {
            auto members = db_manager_.getChannelMembers(resolved_channel_id);
            for (const auto& m : members) {
                if (!m.user_id.empty()) ws_sender_(m.user_id, payload);
            }
        }
    }

    return JsonParser::createSuccessResponse("Message deleted");
}

std::string RequestHandler::handleCreateReport(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string reported_user_id = data["reported_user_id"];
    std::string message_content = data["message_content"];
    std::string reason = data["reason"];
    std::string comment = data.count("comment") ? data["comment"] : "";

    if (token.empty() || message_id.empty() || reason.empty()) {
        return JsonParser::createErrorResponse("Token, message_id and reason required");
    }

    std::string reporter_id = auth_manager_.getUserIdFromToken(token);
    if (reporter_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    std::transform(reason.begin(), reason.end(), reason.begin(), ::tolower);
    if (reason != "spam" && reason != "abuse" && reason != "other") {
        return JsonParser::createErrorResponse("Invalid reason");
    }

    std::string ban_reason;
    if (!report_rate_limiter_.allow(reporter_id, ban_reason)) {
        return JsonParser::createErrorResponse("Too many reports, please try again later");
    }

    std::string message_type = "direct";
    auto msg = db_manager_.getMessageById(message_id);
    if (!msg.id.empty()) {
        message_content = msg.content;
        if (reported_user_id.empty()) {
            reported_user_id = msg.sender_id;
        }
        message_type = msg.message_type.empty() ? "direct" : msg.message_type;
    } else {
        // Try group meta
        std::string meta_group_id;
        std::string meta_sender;
        if (db_manager_.getGroupMessageMeta(message_id, meta_group_id, meta_sender)) {
            if (reported_user_id.empty()) reported_user_id = meta_sender;
            message_type = "group";
        } else {
            // Try legacy channel
            std::string meta_channel_id;
            if (db_manager_.getChannelMessageMetaLegacy(message_id, meta_channel_id, meta_sender)) {
                if (reported_user_id.empty()) reported_user_id = meta_sender;
                message_type = "channel";
            } else {
                // Try v2 chat message
                std::string meta_chat_id;
                if (db_manager_.getChatMessageMetaV2(message_id, meta_chat_id, meta_sender)) {
                    if (reported_user_id.empty()) reported_user_id = meta_sender;
                    message_type = "chat_v2";
                }
            }
        }
    }

    if (message_content.empty()) {
        message_content = "[нет текста]";
    }
    if (reported_user_id.empty()) {
        return JsonParser::createErrorResponse("Message author not found");
    }

    // Collect lightweight context for reviewer
    std::string context_json = "[]";
    if (!msg.id.empty()) {
        auto context = db_manager_.getMessageContext(msg.sender_id, msg.receiver_id, msg.id, 7);
        std::ostringstream ctx;
        ctx << "[";
        bool first = true;
        for (const auto& c : context) {
            if (!first) ctx << ",";
            first = false;
            ctx << "{"
                << "\"id\":\"" << JsonParser::escapeJson(c.id) << "\","
                << "\"sender_id\":\"" << JsonParser::escapeJson(c.sender_id) << "\","
                << "\"receiver_id\":\"" << JsonParser::escapeJson(c.receiver_id) << "\","
                << "\"content\":\"" << JsonParser::escapeJson(c.content) << "\","
                << "\"created_at\":\"" << JsonParser::escapeJson(c.created_at) << "\""
                << "}";
        }
        ctx << "]";
        context_json = ctx.str();
    }

    std::string report_id;
    if (!db_manager_.createReport(reporter_id,
                                  reported_user_id,
                                  message_id,
                                  message_type,
                                  message_content,
                                  reason,
                                  comment,
                                  context_json,
                                  report_id)) {
        return JsonParser::createErrorResponse("Failed to submit report");
    }

    const bool auto_enabled = parseBoolValue(db_manager_.getAdminSetting("auto_mod_enabled"), false);
    if (auto_enabled) {
        auto parseIntOrDefault = [](const std::string& value, int fallback) {
            try {
                return std::stoi(value);
            } catch (...) {
                return fallback;
            }
        };
        auto clampInt = [](int value, int min_value, int max_value) {
            if (value < min_value) return min_value;
            if (value > max_value) return max_value;
            return value;
        };
        auto getSetting = [&](const std::string& key, const std::string& fallback) {
            std::string value = db_manager_.getAdminSetting(key);
            return value.empty() ? fallback : value;
        };

        const int window_minutes = clampInt(parseIntOrDefault(getSetting("auto_mod_window_minutes", "60"), 60), 5, 1440);
        const int spam_threshold = clampInt(parseIntOrDefault(getSetting("auto_mod_spam_threshold", "3"), 3), 1, 20);
        const int abuse_threshold = clampInt(parseIntOrDefault(getSetting("auto_mod_abuse_threshold", "2"), 2), 1, 20);
        const bool delete_on_threshold = parseBoolValue(getSetting("auto_mod_delete_enabled", "true"), true);
        const bool ban_on_threshold = parseBoolValue(getSetting("auto_mod_ban_enabled", "true"), true);
        const bool auto_resolve = parseBoolValue(getSetting("auto_mod_auto_resolve", "true"), true);

        const int threshold = (reason == "spam") ? spam_threshold : abuse_threshold;
        const std::string count_reason = (reason == "spam" ? "spam" : (reason == "abuse" ? "abuse" : ""));
        const long long report_count = db_manager_.countReportsForUserWindow(reported_user_id, count_reason, window_minutes);

        if (threshold > 0 && report_count >= threshold) {
            bool deleted = false;
            bool banned = false;
            if (delete_on_threshold) {
                deleted = deleteMessageForModeration(message_id, message_type, nullptr);
            }
            if (ban_on_threshold) {
                banned = db_manager_.setUserActive(reported_user_id, false);
                if (banned) {
                    db_manager_.deleteUserSessionsByUser(reported_user_id);
                }
            }

            if (auto_resolve) {
                std::ostringstream note;
                note << "Auto-moderation: threshold " << report_count << "/" << threshold;
                if (delete_on_threshold) {
                    note << (deleted ? " | message deleted" : " | message delete failed");
                }
                if (ban_on_threshold) {
                    note << (banned ? " | user banned" : " | ban failed");
                }
                db_manager_.updateReportStatus(report_id, "resolved", "", note.str());
            }
        }
    }

    std::ostringstream oss;
    oss << "{\"success\":true,\"report_id\":\"" << JsonParser::escapeJson(report_id) << "\"}";
    return oss.str();
}

std::string RequestHandler::handleFriendRequest(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string username = data["username"];
    
    if (token.empty() || username.empty()) {
        return JsonParser::createErrorResponse("Token and username required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        Logger::getInstance().warning("Invalid token in handleFriendRequest: " + token.substr(0, 10) + "...");
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto user = db_manager_.getUserByUsername(username);
    if (user.id.empty() || user.id == sender_id || db_manager_.areFriends(sender_id, user.id)) {
        return JsonParser::createErrorResponse("User not found, cannot send to yourself, or already friends");
    }
    
    if (db_manager_.createFriendRequest(sender_id, user.id)) {
        return JsonParser::createSuccessResponse("Friend request sent");
    }
    return JsonParser::createErrorResponse("Failed to send request");
}

std::string RequestHandler::handleAcceptFriend(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string request_id = data["request_id"];
    
    if (token.empty() || request_id.empty()) {
        return JsonParser::createErrorResponse("Token and request_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty() || !db_manager_.acceptFriendRequest(request_id)) {
        return JsonParser::createErrorResponse("Invalid token or failed to accept");
    }
    
    return JsonParser::createSuccessResponse("Friend request accepted");
}

std::string RequestHandler::handleRejectFriend(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string request_id = data["request_id"];
    
    if (token.empty() || request_id.empty()) {
        return JsonParser::createErrorResponse("Token and request_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty() || !db_manager_.rejectFriendRequest(request_id)) {
        return JsonParser::createErrorResponse("Invalid token or failed to reject");
    }
    
    return JsonParser::createSuccessResponse("Friend request rejected");
}

std::string RequestHandler::handleGetFriendRequests(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto requests = db_manager_.getFriendRequests(user_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"requests\":[";
    
    bool first = true;
    for (const auto& req : requests) {
        if (!first) oss << ",";
        first = false;
        
        // Получаем информацию об отправителе
        auto sender = db_manager_.getUserById(req.sender_id);
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(req.id) << "\","
            << "\"sender_id\":\"" << JsonParser::escapeJson(req.sender_id) << "\","
            << "\"sender_username\":\"" << JsonParser::escapeJson(sender.username) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(req.created_at) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleFindUser(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string username = data["username"];
    
    // Token не обязателен для поиска пользователя, но проверим если есть
    std::string current_user_id;
    if (!token.empty()) {
        current_user_id = auth_manager_.getUserIdFromToken(token);
        // Не возвращаем ошибку если токен невалиден - просто ищем пользователя
    }
    
    if (username.empty()) {
        return JsonParser::createErrorResponse("Username required");
    }
    
    auto user = db_manager_.getUserByUsername(username);
    if (user.id.empty()) {
        return JsonParser::createErrorResponse("User not found");
    }
    
    if (!current_user_id.empty() && user.id == current_user_id) {
        return JsonParser::createErrorResponse("Cannot chat with yourself");
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"user_id\":\"" << JsonParser::escapeJson(user.id) << "\","
        << "\"username\":\"" << JsonParser::escapeJson(user.username) << "\","
        << "\"is_premium\":" << (user.is_premium ? "true" : "false") << "}";
    return oss.str();
}

std::string RequestHandler::handleBotCallbackQuery(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data.count("token") ? data["token"] : "";
    std::string message_id = data.count("message_id") ? data["message_id"] : "";
    std::string callback_data = data.count("callback_data") ? data["callback_data"] : "";
    std::string from_user_id = data.count("from_user_id") ? data["from_user_id"] : "";
    
    if (token.empty() || message_id.empty() || callback_data.empty()) {
        return JsonParser::createErrorResponse("Token, message_id and callback_data required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Get message to find bot sender
    Message msg = db_manager_.getMessageById(message_id);
    if (msg.id.empty()) {
        return JsonParser::createErrorResponse("Message not found");
    }
    
    // Get bot info from sender_id
    auto bot = db_manager_.getBotBuilderBotByUserId(msg.sender_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Message is not from a bot");
    }
    
    // For now, just return success
    // In the future, this can trigger bot's callback handler
    std::ostringstream oss;
    oss << "{\"success\":true,\"response\":{"
        << "\"alert\":\"Кнопка нажата: " << JsonParser::escapeJson(callback_data) << "\""
        << "}}";
    return oss.str();
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

    // normalize handle: trim, remove leading '@', lowercase
    auto normalizeHandle = [](std::string s) -> std::string {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r')) s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty() && s[0] == '@') s = s.substr(1);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return s;
    };

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

std::string RequestHandler::handleValidateToken(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    if (auth_manager_.validateSessionToken(token)) {
        std::string user_id = auth_manager_.getUserIdFromToken(token);
        std::map<std::string, std::string> response_data;
        response_data["user_id"] = user_id;
        User user = db_manager_.getUserById(user_id);
        response_data["username"] = user.username;
        response_data["is_premium"] = user.is_premium ? "true" : "false";
        response_data["premium_plan"] = user.premium_plan;
        response_data["premium_expires_at"] = user.premium_expires_at;
        return JsonParser::createSuccessResponse("Token is valid", response_data);
    } else {
        return JsonParser::createErrorResponse("Invalid token");
    }
}

std::string RequestHandler::handleAiProxyModels(const std::string& body,
                                                const std::map<std::string, std::string>& headers) {
    std::string token = extractJsonValueForKey(body, "token");
    if (token.empty() || token == kSessionTokenPlaceholder) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty() || token == kSessionTokenPlaceholder) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    const std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccessLocal(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    std::string api_base = resolveAiApiBase(extractJsonValueForKey(body, "api_base"));
    if (api_base.empty()) {
        return JsonParser::createErrorResponse("Invalid api_base");
    }

    std::string auth_type = toLowerCopy(extractJsonValueForKey(body, "auth_type"));
    const std::string api_key = extractJsonValueForKey(body, "api_key");
    const std::string oauth_token = extractJsonValueForKey(body, "oauth_token");
    if (auth_type.empty()) auth_type = "api_key";
    const std::string bearer = auth_type == "oauth" ? oauth_token : api_key;
    if (bearer.empty()) {
        return JsonParser::createErrorResponse("Missing AI token");
    }

    std::string response_body;
    long status_code = 0;
    const std::string url = api_base + "/models";
    if (!aiHttpGet(url, bearer, &response_body, &status_code)) {
        return JsonParser::createErrorResponse("AI request failed");
    }
    if (status_code >= 400) {
        return JsonParser::createErrorResponse("AI error: " + std::to_string(status_code));
    }

    std::map<std::string, std::string> out;
    out["response_body"] = response_body;
    out["status_code"] = std::to_string(status_code);
    return JsonParser::createSuccessResponse("ok", out);
}

std::string RequestHandler::handleAiProxyChat(const std::string& body,
                                              const std::map<std::string, std::string>& headers) {
    std::string token = extractJsonValueForKey(body, "token");
    if (token.empty() || token == kSessionTokenPlaceholder) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty() || token == kSessionTokenPlaceholder) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    const std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccessLocal(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    std::string api_base = resolveAiApiBase(extractJsonValueForKey(body, "api_base"));
    if (api_base.empty()) {
        return JsonParser::createErrorResponse("Invalid api_base");
    }

    std::string auth_type = toLowerCopy(extractJsonValueForKey(body, "auth_type"));
    const std::string api_key = extractJsonValueForKey(body, "api_key");
    const std::string oauth_token = extractJsonValueForKey(body, "oauth_token");
    if (auth_type.empty()) auth_type = "api_key";
    const std::string bearer = auth_type == "oauth" ? oauth_token : api_key;
    if (bearer.empty()) {
        return JsonParser::createErrorResponse("Missing AI token");
    }

    std::string payload = extractJsonValueForKey(body, "payload");
    if (payload.empty()) {
        return JsonParser::createErrorResponse("Missing payload");
    }

    std::string response_body;
    long status_code = 0;
    const std::string url = api_base + "/chat/completions";
    if (!aiHttpPostJson(url, payload, bearer, &response_body, &status_code)) {
        return JsonParser::createErrorResponse("AI request failed");
    }
    if (status_code >= 400) {
        return JsonParser::createErrorResponse("AI error: " + std::to_string(status_code));
    }

    std::map<std::string, std::string> out;
    out["response_body"] = response_body;
    out["status_code"] = std::to_string(status_code);
    return JsonParser::createSuccessResponse("ok", out);
}

std::string RequestHandler::handleAiOauthExchange(const std::string& body,
                                                  const std::map<std::string, std::string>& headers) {
    std::string token = extractJsonValueForKey(body, "token");
    if (token.empty() || token == kSessionTokenPlaceholder) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty() || token == kSessionTokenPlaceholder) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    const std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccessLocal(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    const char* token_url_env = std::getenv("XIPHER_AI_OAUTH_TOKEN_URL");
    const char* client_id_env = std::getenv("XIPHER_AI_OAUTH_CLIENT_ID");
    if (!token_url_env || !client_id_env) {
        return JsonParser::createErrorResponse("OAuth config missing");
    }
    const std::string token_url = trimEnvValue(token_url_env);
    const std::string client_id = trimEnvValue(client_id_env);
    if (token_url.empty() || client_id.empty()) {
        return JsonParser::createErrorResponse("OAuth config missing");
    }

    std::string grant_type = extractJsonValueForKey(body, "grant_type");
    if (grant_type.empty()) {
        grant_type = "authorization_code";
    }

    std::string form_body;
    appendFormField(form_body, "grant_type", grant_type);
    appendFormField(form_body, "client_id", client_id);
    if (grant_type == "authorization_code") {
        const std::string code = extractJsonValueForKey(body, "code");
        const std::string verifier = extractJsonValueForKey(body, "code_verifier");
        const std::string redirect_uri = extractJsonValueForKey(body, "redirect_uri");
        if (code.empty() || verifier.empty() || redirect_uri.empty()) {
            return JsonParser::createErrorResponse("Missing OAuth code");
        }
        appendFormField(form_body, "code", code);
        appendFormField(form_body, "code_verifier", verifier);
        appendFormField(form_body, "redirect_uri", redirect_uri);
    } else if (grant_type == "refresh_token") {
        const std::string refresh_token = extractJsonValueForKey(body, "refresh_token");
        if (refresh_token.empty()) {
            return JsonParser::createErrorResponse("Missing refresh_token");
        }
        appendFormField(form_body, "refresh_token", refresh_token);
    } else {
        return JsonParser::createErrorResponse("Invalid grant_type");
    }

    std::string response_body;
    long status_code = 0;
    if (!aiHttpPostForm(token_url, form_body, &response_body, &status_code)) {
        return JsonParser::createErrorResponse("OAuth request failed");
    }
    if (status_code >= 400) {
        return JsonParser::createErrorResponse("OAuth error: " + std::to_string(status_code));
    }

    std::map<std::string, std::string> out;
    out["response_body"] = response_body;
    out["status_code"] = std::to_string(status_code);
    return JsonParser::createSuccessResponse("ok", out);
}

namespace {
bool isVisibilityValid(const std::string& v) {
    return v == "everyone" || v == "contacts" || v == "nobody";
}

bool canViewByVisibility(const std::string& visibility, bool is_self, bool is_friend) {
    if (is_self) return true;
    if (visibility == "everyone") return true;
    if (visibility == "contacts") return is_friend;
    return false;
}
} // namespace

std::string RequestHandler::handleGetUserProfile(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string target_user_id = data.find("user_id") != data.end() ? data["user_id"] : "";
    std::string target_username = data.find("username") != data.end() ? data["username"] : "";

    if (token.empty() || (target_user_id.empty() && target_username.empty())) {
        return JsonParser::createErrorResponse("token and user_id or username required");
    }

    std::string viewer_id = auth_manager_.getUserIdFromToken(token);
    if (viewer_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User target;
    if (!target_user_id.empty()) {
        target = db_manager_.getUserById(target_user_id);
    } else {
        if (!target_username.empty() && target_username[0] == '@') {
            target_username = target_username.substr(1);
        }
        target = db_manager_.getUserByUsername(target_username);
        target_user_id = target.id;
    }

    if (target.id.empty()) {
        return JsonParser::createErrorResponse("User not found");
    }

    const bool is_self = (viewer_id == target.id);
    const bool is_friend = db_manager_.areFriends(viewer_id, target.id);
    const bool is_online = db_manager_.isUserOnline(target.id);

    UserProfile profile;
    db_manager_.getUserProfile(target.id, profile);
    UserPrivacySettings privacy;
    db_manager_.getUserPrivacy(target.id, privacy);

    // Visibility gating
    const bool can_view_bio = canViewByVisibility(privacy.bio_visibility, is_self, is_friend);
    const bool can_view_birth = canViewByVisibility(privacy.birth_visibility, is_self, is_friend);
    const bool can_view_last_seen = canViewByVisibility(privacy.last_seen_visibility, is_self, is_friend);
    const bool can_view_avatar = canViewByVisibility(privacy.avatar_visibility, is_self, is_friend);

    std::string last_seen = "";
    if (can_view_last_seen) {
        last_seen = db_manager_.getUserLastActivity(target.id);
    }

    std::string display_name = profile.first_name;
    if (!profile.last_name.empty()) {
        if (!display_name.empty()) display_name += " ";
        display_name += profile.last_name;
    }
    if (display_name.empty()) display_name = target.username;

    std::string public_username = target.username;
    std::string public_avatar_url = "";
    // Pull avatar_url from users table (not in User struct)
    db_manager_.getUserPublic(target.id, public_username, public_avatar_url);

    std::string personal_channel_json;
    if (!profile.linked_channel_id.empty()) {
        Chat chat_v2 = db_manager_.getChatV2(profile.linked_channel_id);
        if (!chat_v2.id.empty() && chat_v2.type == "channel") {
            bool can_view_channel = chat_v2.is_public;
            if (!chat_v2.is_public) {
                if (chat_v2.created_by == viewer_id) {
                    can_view_channel = true;
                } else {
                    ChatPermissions perms = db_manager_.getChatPermissions(chat_v2.id, viewer_id);
                    if (perms.found && perms.status != "kicked" && perms.status != "left") {
                        can_view_channel = true;
                    }
                }
            }
            if (can_view_channel) {
                std::ostringstream pc;
                pc << "\"personal_channel\":{"
                   << "\"id\":\"" << JsonParser::escapeJson(chat_v2.id) << "\","
                   << "\"name\":\"" << JsonParser::escapeJson(chat_v2.title) << "\","
                   << "\"custom_link\":\"" << JsonParser::escapeJson(chat_v2.username) << "\","
                   << "\"is_private\":" << (chat_v2.is_public ? "false" : "true")
                   << "}";
                personal_channel_json = pc.str();
            }
        } else {
            auto channel = db_manager_.getChannelById(profile.linked_channel_id);
            if (!channel.id.empty()) {
                bool can_view_channel = !channel.is_private;
                if (channel.is_private) {
                    if (channel.creator_id == viewer_id) {
                        can_view_channel = true;
                    } else {
                        auto member = db_manager_.getChannelMember(channel.id, viewer_id);
                        if (!member.id.empty() && !member.is_banned) {
                            can_view_channel = true;
                        }
                    }
                }
                if (can_view_channel) {
                    std::ostringstream pc;
                    pc << "\"personal_channel\":{"
                       << "\"id\":\"" << JsonParser::escapeJson(channel.id) << "\","
                       << "\"name\":\"" << JsonParser::escapeJson(channel.name) << "\","
                       << "\"custom_link\":\"" << JsonParser::escapeJson(channel.custom_link) << "\","
                       << "\"is_private\":" << (channel.is_private ? "true" : "false")
                       << "}";
                    personal_channel_json = pc.str();
                }
            }
        }
    }

    std::ostringstream oss;
    oss << "{"
        << "\"success\":true,"
        << "\"user\":{"
        << "\"id\":\"" << JsonParser::escapeJson(target.id) << "\","
        << "\"username\":\"" << JsonParser::escapeJson(target.username) << "\","
        << "\"display_name\":\"" << JsonParser::escapeJson(display_name) << "\","
        << "\"is_premium\":" << (target.is_premium ? "true" : "false") << ","
        << "\"premium_plan\":\"" << JsonParser::escapeJson(target.premium_plan) << "\","
        << "\"premium_expires_at\":\"" << JsonParser::escapeJson(target.premium_expires_at) << "\","
        << "\"is_self\":" << (is_self ? "true" : "false") << ","
        << "\"is_friend\":" << (is_friend ? "true" : "false") << ","
        << "\"is_online\":" << (is_online ? "true" : "false") << ",";

    // avatar_url lives in users table; UI can still show fallback if not allowed
    oss << "\"avatar_url\":\"" << JsonParser::escapeJson(can_view_avatar ? public_avatar_url : "") << "\",";

    oss << "\"first_name\":\"" << JsonParser::escapeJson(profile.first_name) << "\","
        << "\"last_name\":\"" << JsonParser::escapeJson(profile.last_name) << "\","
        << "\"bio\":\"" << JsonParser::escapeJson(can_view_bio ? profile.bio : "") << "\","
        << "\"birth_day\":" << (can_view_birth ? profile.birth_day : 0) << ","
        << "\"birth_month\":" << (can_view_birth ? profile.birth_month : 0) << ","
        << "\"birth_year\":" << (can_view_birth ? profile.birth_year : 0) << ","
        << "\"last_seen\":\"" << JsonParser::escapeJson(last_seen) << "\","
        << "\"privacy\":{"
        << "\"bio_visibility\":\"" << JsonParser::escapeJson(privacy.bio_visibility) << "\","
        << "\"birth_visibility\":\"" << JsonParser::escapeJson(privacy.birth_visibility) << "\","
        << "\"last_seen_visibility\":\"" << JsonParser::escapeJson(privacy.last_seen_visibility) << "\","
        << "\"avatar_visibility\":\"" << JsonParser::escapeJson(privacy.avatar_visibility) << "\"";
    if (is_self) {
        oss << ",\"send_read_receipts\":" << (privacy.send_read_receipts ? "true" : "false");
    }
    oss << "}";

    const std::string business_hours = looksLikeJsonObject(profile.business_hours_json)
        ? profile.business_hours_json
        : "{}";
    oss << ",\"business_hours\":" << business_hours;

    if (!personal_channel_json.empty()) {
        oss << "," << personal_channel_json;
    }

    // Media stats for chat between viewer and target
    auto stats = db_manager_.getUserChatStats(viewer_id, target_user_id);
    oss << ",\"media_stats\":{"
        << "\"photos\":" << stats.photo_count << ","
        << "\"videos\":" << stats.video_count << ","
        << "\"files\":" << stats.file_count << ","
        << "\"audio\":" << stats.audio_count << ","
        << "\"voice\":" << stats.voice_count << ","
        << "\"links\":" << stats.link_count << ","
        << "\"gifs\":" << stats.gif_count << ","
        << "\"saved\":" << stats.saved_count
        << "}";

    // Common groups
    auto common_groups = db_manager_.getCommonGroups(viewer_id, target_user_id, 10);
    oss << ",\"common_groups\":[";
    for (size_t i = 0; i < common_groups.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{"
            << "\"id\":\"" << JsonParser::escapeJson(common_groups[i].id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(common_groups[i].name) << "\""
            << "}";
    }
    oss << "]";

    oss << "}"
        << "}";
    return oss.str();
}

std::string RequestHandler::handleSetPersonalChannel(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data.find("channel_id") != data.end() ? data["channel_id"] : "";

    if (token.empty()) {
        return JsonParser::createErrorResponse("token required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    if (channel_id == "null") {
        channel_id.clear();
    }

    if (channel_id.empty()) {
        if (db_manager_.setUserLinkedChannel(user_id, "")) {
            return JsonParser::createSuccessResponse("Personal channel cleared");
        }
        return JsonParser::createErrorResponse("Failed to clear personal channel");
    }

    if (!channel_id.empty() && channel_id[0] == '@') {
        channel_id = channel_id.substr(1);
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        if (chat_v2.created_by != user_id) {
            return JsonParser::createErrorResponse("Only channel owner can set personal channel");
        }
        if (db_manager_.setUserLinkedChannel(user_id, chat_v2.id)) {
            return JsonParser::createSuccessResponse("Personal channel updated");
        }
        return JsonParser::createErrorResponse("Failed to update personal channel");
    }

    auto channel = db_manager_.getChannelById(channel_id);
    if (channel.id.empty()) {
        channel = db_manager_.getChannelByCustomLink(channel_id);
    }
    if (channel.id.empty()) {
        return JsonParser::createErrorResponse("Channel not found");
    }
    if (channel.creator_id != user_id) {
        return JsonParser::createErrorResponse("Only channel owner can set personal channel");
    }

    if (db_manager_.setUserLinkedChannel(user_id, channel.id)) {
        return JsonParser::createSuccessResponse("Personal channel updated");
    }
    return JsonParser::createErrorResponse("Failed to update personal channel");
}

std::string RequestHandler::handleUpdateMyProfile(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    if (token.empty()) {
        return JsonParser::createErrorResponse("token required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) return JsonParser::createErrorResponse("Invalid token");

    UserProfile p;
    p.user_id = user_id;
    p.first_name = data.find("first_name") != data.end() ? data["first_name"] : "";
    p.last_name = data.find("last_name") != data.end() ? data["last_name"] : "";
    p.bio = data.find("bio") != data.end() ? data["bio"] : "";
    try { p.birth_day = data.find("birth_day") != data.end() ? std::stoi(data["birth_day"]) : 0; } catch (...) { p.birth_day = 0; }
    try { p.birth_month = data.find("birth_month") != data.end() ? std::stoi(data["birth_month"]) : 0; } catch (...) { p.birth_month = 0; }
    try { p.birth_year = data.find("birth_year") != data.end() ? std::stoi(data["birth_year"]) : 0; } catch (...) { p.birth_year = 0; }

    if (!db_manager_.upsertUserProfile(p)) {
        return JsonParser::createErrorResponse("Failed to update profile");
    }
    return JsonParser::createSuccessResponse("Profile updated");
}

std::string RequestHandler::handleUpdateMyPrivacy(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    if (token.empty()) {
        return JsonParser::createErrorResponse("token required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) return JsonParser::createErrorResponse("Invalid token");

    UserPrivacySettings s;
    s.bio_visibility = data.find("bio_visibility") != data.end() ? data["bio_visibility"] : s.bio_visibility;
    s.birth_visibility = data.find("birth_visibility") != data.end() ? data["birth_visibility"] : s.birth_visibility;
    s.last_seen_visibility = data.find("last_seen_visibility") != data.end() ? data["last_seen_visibility"] : s.last_seen_visibility;
    s.avatar_visibility = data.find("avatar_visibility") != data.end() ? data["avatar_visibility"] : s.avatar_visibility;
    if (data.find("send_read_receipts") != data.end()) {
        s.send_read_receipts = parseBoolValue(data["send_read_receipts"], s.send_read_receipts);
    }

    if (!isVisibilityValid(s.bio_visibility) || !isVisibilityValid(s.birth_visibility) ||
        !isVisibilityValid(s.last_seen_visibility) || !isVisibilityValid(s.avatar_visibility)) {
        return JsonParser::createErrorResponse("Invalid visibility value");
    }

    if (!db_manager_.upsertUserPrivacy(user_id, s)) {
        return JsonParser::createErrorResponse("Failed to update privacy");
    }
    return JsonParser::createSuccessResponse("Privacy updated");
}

std::string RequestHandler::handleSetBusinessHours(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string business_hours = data.find("business_hours") != data.end() ? data["business_hours"] : "";

    if (token.empty()) {
        return JsonParser::createErrorResponse("token required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) return JsonParser::createErrorResponse("Invalid token");

    User user = db_manager_.getUserById(user_id);
    if (!user.is_premium) {
        return JsonParser::createErrorResponse("Premium required");
    }

    if (business_hours == "null") {
        business_hours.clear();
    }

    if (!business_hours.empty()) {
        if (!looksLikeJsonObject(business_hours)) {
            return JsonParser::createErrorResponse("Invalid business_hours payload");
        }
        if (business_hours.size() > 20000) {
            return JsonParser::createErrorResponse("Business hours payload too large");
        }
    }

    if (!db_manager_.setUserBusinessHours(user_id, business_hours)) {
        return JsonParser::createErrorResponse("Failed to update business hours");
    }
    return JsonParser::createSuccessResponse("Business hours updated");
}

std::string RequestHandler::handleUpdateActivity(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    if (db_manager_.updateLastActivity(user_id)) {
        return JsonParser::createSuccessResponse("Activity updated");
    } else {
        return JsonParser::createErrorResponse("Failed to update activity");
    }
}

std::string RequestHandler::handleUserStatus(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string user_id_to_check = data["user_id"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string current_user_id = auth_manager_.getUserIdFromToken(token);
    if (current_user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    if (user_id_to_check.empty()) {
        user_id_to_check = current_user_id;  // If not specified, return current user's status
    }
    
    bool is_online = db_manager_.isUserOnline(user_id_to_check, 300);  // 5 minutes threshold
    std::string last_activity = db_manager_.getUserLastActivity(user_id_to_check);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"user_id\":\"" << JsonParser::escapeJson(user_id_to_check) << "\","
        << "\"online\":" << (is_online ? "true" : "false") << ","
        << "\"last_activity\":\"" << JsonParser::escapeJson(last_activity) << "\"}";
    return oss.str();
}

std::string RequestHandler::handleGetTurnConfig(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto trim = [](std::string value) {
        const char* whitespace = " \t\n\r";
        auto start = value.find_first_not_of(whitespace);
        if (start == std::string::npos) {
            return std::string();
        }
        auto end = value.find_last_not_of(whitespace);
        value = value.substr(start, end - start + 1);
        return value;
    };
    
    auto splitEnvList = [&](const std::vector<const char*>& keys) {
        std::vector<std::string> items;
        for (const auto* key : keys) {
            const char* raw = std::getenv(key);
            if (!raw || std::string(raw).empty()) continue;
            std::stringstream ss(raw);
            std::string part;
            while (std::getline(ss, part, ',')) {
                part = trim(part);
                if (!part.empty()) {
                    items.push_back(part);
                }
            }
        }
        return items;
    };
    
    // Prefer explicit TURN config from environment, fall back to defaults used on the frontend
    std::vector<std::string> stun_urls = splitEnvList({"XIPHER_STUN_URLS", "TURN_STUN_URLS"});
    std::vector<std::string> turn_urls = splitEnvList({"XIPHER_TURN_URLS", "TURN_URLS"});
    
    const char* turn_username_env = std::getenv("XIPHER_TURN_USERNAME");
    if (!turn_username_env) {
        turn_username_env = std::getenv("TURN_USERNAME");
    }
    const char* turn_credential_env = std::getenv("XIPHER_TURN_CREDENTIAL");
    if (!turn_credential_env) {
        turn_credential_env = std::getenv("TURN_PASSWORD");
    }
    
    // Sensible defaults to avoid empty configs
    if (stun_urls.empty()) {
        stun_urls.push_back("stun:turn.xipher.pro:3478");
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"ice_servers\":[";
    bool first = true;
    
    // STUN entries
    for (const auto& url : stun_urls) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"urls\":\"" << JsonParser::escapeJson(url) << "\"}";
    }
    
    // TURN entries (if fully configured)
    if (!turn_urls.empty() && turn_username_env && turn_credential_env) {
        oss << (first ? "" : ",") << "{\"urls\":[";
        for (size_t i = 0; i < turn_urls.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << JsonParser::escapeJson(turn_urls[i]) << "\"";
        }
        oss << "],\"username\":\"" << JsonParser::escapeJson(turn_username_env) << "\","
            << "\"credential\":\"" << JsonParser::escapeJson(turn_credential_env) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetSfuConfig(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    const bool enabled = envFlagEnabled(std::getenv("XIPHER_SFU_ENABLED"));
    const auto janus_urls = splitEnvList({"XIPHER_SFU_JANUS_URLS", "XIPHER_SFU_JANUS_URL"});
    if (!enabled || janus_urls.empty()) {
        return "{\"success\":true,\"enabled\":false}";
    }

    std::ostringstream oss;
    oss << "{\"success\":true,\"enabled\":true,\"janus_urls\":[";
    for (size_t i = 0; i < janus_urls.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << JsonParser::escapeJson(janus_urls[i]) << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetSfuRoom(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    const bool enabled = envFlagEnabled(std::getenv("XIPHER_SFU_ENABLED"));
    const auto janus_urls = splitEnvList({"XIPHER_SFU_JANUS_URLS", "XIPHER_SFU_JANUS_URL"});
    if (!enabled || janus_urls.empty()) {
        return "{\"success\":true,\"enabled\":false}";
    }

    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("User is not a member of this group");
    }

    constexpr uint64_t kRoomMin = 100000;
    constexpr uint64_t kRoomMax = 9007199254740991ULL;
    const uint64_t hash = fnv1a64(group_id);
    const uint64_t room_span = kRoomMax - kRoomMin;
    const uint64_t room_id = kRoomMin + (hash % (room_span > 0 ? room_span : 1));

    const size_t server_index = janus_urls.empty() ? 0 : static_cast<size_t>(hash % janus_urls.size());
    const std::string janus_url = janus_urls.empty() ? "" : janus_urls[server_index];

    const auto janus_api_urls = splitEnvList({"XIPHER_SFU_JANUS_API_URLS", "XIPHER_SFU_JANUS_API_URL"});
    const std::string janus_api_url = janus_api_urls.empty()
        ? ""
        : janus_api_urls[std::min(server_index, janus_api_urls.size() - 1)];
    const char* admin_key_env = std::getenv("XIPHER_SFU_JANUS_ADMIN_KEY");
    const std::string admin_key = admin_key_env ? admin_key_env : "";
    const char* api_secret_env = std::getenv("XIPHER_SFU_JANUS_API_SECRET");
    const std::string api_secret = api_secret_env ? api_secret_env : "";

    int max_publishers = 50;
    const char* publishers_env = std::getenv("XIPHER_SFU_MAX_PUBLISHERS");
    if (publishers_env) {
        try {
            max_publishers = std::max(5, std::stoi(publishers_env));
        } catch (...) {}
    }
    int max_bitrate = 6000000;
    const char* bitrate_env = std::getenv("XIPHER_SFU_MAX_BITRATE");
    if (bitrate_env) {
        try {
            max_bitrate = std::max(500000, std::stoi(bitrate_env));
        } catch (...) {}
    }

    bool room_ready = false;
    if (!janus_api_url.empty() && !admin_key.empty()) {
        const std::string api_secret_json = api_secret.empty()
            ? ""
            : ",\"apisecret\":\"" + JsonParser::escapeJson(api_secret) + "\"";

        std::string response;
        long status = 0;
        const std::string transaction = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::string create_payload = "{\"janus\":\"create\",\"transaction\":\"" + transaction + "\""
            + api_secret_json + "}";
        if (sfuHttpPostJson(janus_api_url, create_payload, &response, &status) && status >= 200 && status < 300) {
            const std::string session_id = sfuExtractJsonNumber(response, "\"id\"");
            if (!session_id.empty()) {
                const std::string attach_payload = "{\"janus\":\"attach\",\"plugin\":\"janus.plugin.videoroom\","
                    "\"transaction\":\"" + transaction + "\""
                    + api_secret_json + "}";
                const std::string attach_url = janus_api_url + "/" + session_id;
                if (sfuHttpPostJson(attach_url, attach_payload, &response, &status) && status >= 200 && status < 300) {
                    const std::string handle_id = sfuExtractJsonNumber(response, "\"id\"");
                    if (!handle_id.empty()) {
                        std::ostringstream body;
                        body << "{\"request\":\"create\",\"room\":" << room_id
                             << ",\"publishers\":" << max_publishers
                             << ",\"bitrate\":" << max_bitrate
                             << ",\"fir_freq\":10"
                             << ",\"audiocodec\":\"opus\""
                             << ",\"videocodec\":\"vp8\"}";
                        std::string message_payload = "{\"janus\":\"message\",\"transaction\":\"" + transaction + "\""
                            + api_secret_json
                            + ",\"admin_key\":\"" + JsonParser::escapeJson(admin_key) + "\""
                            + ",\"body\":" + body.str() + "}";
                        const std::string message_url = janus_api_url + "/" + session_id + "/" + handle_id;
                        if (sfuHttpPostJson(message_url, message_payload, &response, &status) && status >= 200 && status < 300) {
                            const bool already_exists = response.find("already exists") != std::string::npos
                                || response.find("\"error_code\":427") != std::string::npos;
                            const bool has_error = response.find("\"error\"") != std::string::npos
                                && response.find("\"error_code\"") != std::string::npos;
                            room_ready = !has_error || already_exists;
                        }
                    }
                }
                const std::string destroy_payload = "{\"janus\":\"destroy\",\"transaction\":\"" + transaction + "\""
                    + api_secret_json + "}";
                sfuHttpPostJson(janus_api_url + "/" + session_id, destroy_payload, &response, &status);
            }
        }
    }

    std::ostringstream oss;
    oss << "{\"success\":true,\"enabled\":true";
    if (!janus_url.empty()) {
        oss << ",\"janus_url\":\"" << JsonParser::escapeJson(janus_url) << "\"";
    }
    oss << ",\"room_id\":\"" << room_id << "\""
        << ",\"room_ready\":" << (room_ready ? "true" : "false")
        << ",\"server_index\":" << server_index
        << "}";
    return oss.str();
}

std::string RequestHandler::handleUploadFile(const std::string& body, const std::map<std::string, std::string>& /*headers*/) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string file_data = data["file_data"];  // base64 encoded
    std::string file_name = data["file_name"];
    
    if (token.empty() || file_data.empty() || file_name.empty()) {
        return JsonParser::createErrorResponse("Token, file_data and file_name required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Create uploads directory if it doesn't exist (secure way)
    std::string uploads_dir = "/root/xipher/uploads/files";
    try {
        fs::path dir_path(uploads_dir);
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to create uploads directory: " + std::string(e.what()));
        return JsonParser::createErrorResponse("Failed to create uploads directory");
    }
    
    // Validate and sanitize filename to prevent path traversal
    std::string sanitized_filename = file_name;
    // Remove path components
    size_t last_slash = sanitized_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        sanitized_filename = sanitized_filename.substr(last_slash + 1);
    }
    // Remove dangerous characters
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\0'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '/'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\\'), sanitized_filename.end());
    
    // Generate unique filename
    std::string file_ext = "";
    size_t dot_pos = sanitized_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        file_ext = sanitized_filename.substr(dot_pos);
        // Validate extension (basic check)
        if (file_ext.length() > 10) {
            file_ext = ""; // Invalid extension
        }
    }
    
    // Use secure random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::string unique_filename = user_id + "_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(dis(gen)) + file_ext;
    
    // Ensure filename doesn't contain path traversal
    fs::path base_path(uploads_dir);
    fs::path file_path = base_path / unique_filename;
    // Normalize path to prevent directory traversal
    try {
        if (fs::exists(base_path)) {
            fs::path canonical_base = fs::canonical(base_path);
            file_path = canonical_base / unique_filename;
            // Ensure the final path is still within the base directory
            std::string base_str = canonical_base.string();
            std::string file_str = file_path.string();
            if (file_str.find(base_str) != 0) {
                Logger::getInstance().error("Path traversal detected: " + file_str);
                return JsonParser::createErrorResponse("Invalid file path");
            }
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Path normalization error: " + std::string(e.what()));
        // Fallback to simple path construction
        file_path = base_path / unique_filename;
    }
    
    // Validate file size (10 GB limit)
    const long long MAX_FILE_SIZE = 10LL * 1024 * 1024 * 1024;
    size_t base64_size = file_data.length();
    // Base64 encoding increases size by ~33%, so estimate decoded size
    size_t estimated_size = (base64_size * 3) / 4;
    if (estimated_size > MAX_FILE_SIZE) {
        return JsonParser::createErrorResponse("File too large. Maximum size is 10 GB");
    }
    
    // Decode base64 and save file
    std::string decoded = base64Decode(file_data);
    if (decoded.length() > MAX_FILE_SIZE) {
        return JsonParser::createErrorResponse("File too large. Maximum size is 10 GB");
    }
    
    std::ofstream file(file_path.string(), std::ios::binary);
    if (!file.is_open()) {
        Logger::getInstance().error("Failed to open file for writing: " + file_path.string());
        return JsonParser::createErrorResponse("Failed to save file");
    }
    file.write(decoded.c_str(), decoded.length());
    file.close();
    
    // Get file size
    long long file_size = 0;
    try {
        if (fs::exists(file_path)) {
            file_size = fs::file_size(file_path);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to get file size: " + std::string(e.what()));
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"file_path\":\"/files/" << unique_filename << "\","
        << "\"file_name\":\"" << JsonParser::escapeJson(file_name) << "\","
        << "\"file_size\":" << file_size << "}";
    return oss.str();
}

std::string RequestHandler::handleUploadVoice(const std::string& body, const std::map<std::string, std::string>& /*headers*/) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string voice_data = data["voice_data"];  // base64 encoded
    
    if (token.empty() || voice_data.empty()) {
        return JsonParser::createErrorResponse("Token and voice_data required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Create uploads directory if it doesn't exist (secure way)
    std::string uploads_dir = "/root/xipher/uploads/voices";
    try {
        fs::path dir_path(uploads_dir);
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to create uploads directory: " + std::string(e.what()));
        return JsonParser::createErrorResponse("Failed to create uploads directory");
    }
    
    // Determine file extension based on MIME type if provided, default to .ogg
    std::string file_ext = ".ogg";
    if (data.find("mime_type") != data.end()) {
        std::string mime_type = data["mime_type"];
        if (mime_type.find("webm") != std::string::npos) {
            file_ext = ".webm";
        } else if (mime_type.find("mp4") != std::string::npos) {
            file_ext = ".mp4";
        }
    }
    
    // Validate voice data size (reasonable limit for voice messages: 100 MB)
    const long long MAX_VOICE_SIZE = 100LL * 1024 * 1024;
    size_t base64_size = voice_data.length();
    size_t estimated_size = (base64_size * 3) / 4;
    if (estimated_size > MAX_VOICE_SIZE) {
        return JsonParser::createErrorResponse("Voice message too large. Maximum size is 100 MB");
    }
    
    // Use secure random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    // Generate unique filename
    std::string unique_filename = user_id + "_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(dis(gen)) + file_ext;
    std::string file_name = "voice_" + std::to_string(std::time(nullptr)) + file_ext;
    
    // Ensure filename doesn't contain path traversal
    fs::path base_path(uploads_dir);
    fs::path file_path = base_path / unique_filename;
    // Normalize path to prevent directory traversal
    try {
        if (fs::exists(base_path)) {
            fs::path canonical_base = fs::canonical(base_path);
            file_path = canonical_base / unique_filename;
            // Ensure the final path is still within the base directory
            std::string base_str = canonical_base.string();
            std::string file_str = file_path.string();
            if (file_str.find(base_str) != 0) {
                Logger::getInstance().error("Path traversal detected: " + file_str);
                return JsonParser::createErrorResponse("Invalid file path");
            }
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Path normalization error: " + std::string(e.what()));
        // Fallback to simple path construction
        file_path = base_path / unique_filename;
    }
    
    // Decode base64 and save file
    std::string decoded = base64Decode(voice_data);
    if (decoded.empty()) {
        return JsonParser::createErrorResponse("Failed to decode voice data");
    }
    
    if (decoded.length() > MAX_VOICE_SIZE) {
        return JsonParser::createErrorResponse("Voice message too large. Maximum size is 100 MB");
    }
    
    std::ofstream file(file_path.string(), std::ios::binary);
    if (!file.is_open()) {
        Logger::getInstance().error("Failed to open voice file for writing: " + file_path.string());
        return JsonParser::createErrorResponse("Failed to save voice message");
    }
    file.write(decoded.c_str(), decoded.length());
    file.close();
    
    // Get file size
    long long file_size = 0;
    try {
        if (fs::exists(file_path)) {
            file_size = fs::file_size(file_path);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to get voice file size: " + std::string(e.what()));
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"file_path\":\"/files/" << unique_filename << "\","
        << "\"file_name\":\"" << file_name << "\","
        << "\"file_size\":" << file_size << "}";
    return oss.str();
}

std::string RequestHandler::handleGetFile(const std::string& path) {
    // Path format: /files/filename
    std::string filename = path.substr(7);  // Remove "/files/"
    
    // Security: Validate filename to prevent path traversal
    if (!isValidFilename(filename)) {
        Logger::getInstance().warning("Path traversal attempt blocked in handleGetFile: " + filename);
        return "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid filename";
    }
    
    std::string file_path = "/root/xipher/uploads/files/" + filename;
    bool is_voice_file = false;
    
    // Also check voices directory
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        file_path = "/root/xipher/uploads/voices/" + filename;
        file.open(file_path, std::ios::binary);
        if (file.is_open()) {
            is_voice_file = true;
        }
    }
    
    if (!file.is_open()) {
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    std::string file_content = content.str();
    file.close();
    
    // Determine MIME type
    std::string extension = "";
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        extension = file_path.substr(dot_pos);
    }
    std::string mime_type = getMimeType(extension);
    if (is_voice_file) {
        if (extension == ".webm" || extension == ".weba") {
            mime_type = "audio/webm";
        } else if (extension == ".mp4" || extension == ".m4a") {
            mime_type = "audio/mp4";
        } else if (extension == ".ogg" || extension == ".oga") {
            mime_type = "audio/ogg";
        } else if (extension == ".opus") {
            mime_type = "audio/opus";
        }
    }
    
    // Build HTTP response with CORS headers for images/videos
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << mime_type << "\r\n";
    response << "Content-Length: " << file_content.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Cache-Control: public, max-age=31536000\r\n";
    response << "Connection: close\r\n\r\n";
    response << file_content;
    
    return response.str();
}

std::string RequestHandler::handleGetAvatar(const std::string& path) {
    // Path format: /avatars/filename
    std::string filename = path.substr(9);  // Remove "/avatars/"
    
    // Security: Validate filename to prevent path traversal
    if (!isValidFilename(filename)) {
        Logger::getInstance().warning("Path traversal attempt blocked in handleGetAvatar: " + filename);
        return "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid filename";
    }
    
    // Try primary location first
    std::string file_path = "/var/www/xipher/uploads/avatars/" + filename;
    std::ifstream file(file_path, std::ios::binary);
    
    // Fallback to alternative location
    if (!file.is_open()) {
        file_path = "/root/xipher/uploads/avatars/" + filename;
        file.open(file_path, std::ios::binary);
    }
    
    if (!file.is_open()) {
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nAvatar not found";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    std::string file_content = content.str();
    file.close();
    
    // Determine MIME type
    std::string extension = "";
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        extension = file_path.substr(dot_pos);
    }
    std::string mime_type = getMimeType(extension);
    
    // Build HTTP response with CORS headers
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << mime_type << "\r\n";
    response << "Content-Length: " << file_content.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Cache-Control: public, max-age=31536000\r\n";
    response << "Connection: close\r\n\r\n";
    response << file_content;
    
    return response.str();
}

std::string RequestHandler::base64Decode(const std::string& encoded) {
    // Simple base64 decode implementation
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (c == '=') break;
        size_t pos = chars.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0) {
            decoded.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return decoded;
}

// Parse multipart/form-data and extract file data
namespace {
    struct MultipartPart {
        std::string name;
        std::string filename;
        std::string content_type;
        std::string data;
    };

    std::vector<MultipartPart> parseMultipartFormData(const std::string& body, const std::string& boundary) {
        std::vector<MultipartPart> parts;
        
        if (boundary.empty() || body.empty()) {
            return parts;
        }
        
        std::string delimiter = "--" + boundary;
        std::string end_delimiter = delimiter + "--";
        
        size_t pos = 0;
        while (pos < body.length()) {
            size_t delimiter_pos = body.find(delimiter, pos);
            if (delimiter_pos == std::string::npos) {
                break;
            }
            
            if (body.substr(delimiter_pos, end_delimiter.length()) == end_delimiter) {
                break;
            }
            
            pos = delimiter_pos + delimiter.length();
            
            if (pos < body.length() && body[pos] == '\r') pos++;
            if (pos < body.length() && body[pos] == '\n') pos++;
            
            size_t headers_end = body.find("\r\n\r\n", pos);
            if (headers_end == std::string::npos) {
                headers_end = body.find("\n\n", pos);
                if (headers_end == std::string::npos) break;
                headers_end += 2;
            } else {
                headers_end += 4;
            }
            
            std::string headers_str = body.substr(pos, headers_end - pos - 4);
            MultipartPart part;
            
            size_t cd_pos = headers_str.find("Content-Disposition:");
            if (cd_pos != std::string::npos) {
                size_t cd_end = headers_str.find("\r\n", cd_pos);
                if (cd_end == std::string::npos) cd_end = headers_str.find("\n", cd_pos);
                
                std::string cd_header = headers_str.substr(cd_pos, cd_end - cd_pos);
                
                size_t name_pos = cd_header.find("name=\"");
                if (name_pos != std::string::npos) {
                    name_pos += 6;
                    size_t name_end = cd_header.find("\"", name_pos);
                    if (name_end != std::string::npos) {
                        part.name = cd_header.substr(name_pos, name_end - name_pos);
                    }
                }
                
                size_t filename_pos = cd_header.find("filename=\"");
                if (filename_pos != std::string::npos) {
                    filename_pos += 10;
                    size_t filename_end = cd_header.find("\"", filename_pos);
                    if (filename_end != std::string::npos) {
                        part.filename = cd_header.substr(filename_pos, filename_end - filename_pos);
                    }
                }
            }
            
            size_t ct_pos = headers_str.find("Content-Type:");
            if (ct_pos != std::string::npos) {
                size_t ct_end = headers_str.find("\r\n", ct_pos);
                if (ct_end == std::string::npos) ct_end = headers_str.find("\n", ct_pos);
                
                std::string ct_header = headers_str.substr(ct_pos + 13, ct_end - ct_pos - 13);
                ct_header.erase(0, ct_header.find_first_not_of(" \t"));
                ct_header.erase(ct_header.find_last_not_of(" \t") + 1);
                part.content_type = ct_header;
            }
            
            pos = headers_end;
            size_t next_delimiter = body.find("\r\n" + delimiter, pos);
            if (next_delimiter == std::string::npos) {
                next_delimiter = body.find("\n" + delimiter, pos);
            }
            if (next_delimiter == std::string::npos) {
                next_delimiter = body.find(delimiter, pos);
            }
            
            if (next_delimiter != std::string::npos) {
                size_t data_end = next_delimiter;
                if (data_end > pos && body[data_end - 1] == '\n') data_end--;
                if (data_end > pos && body[data_end - 1] == '\r') data_end--;
                
                part.data = body.substr(pos, data_end - pos);
                pos = next_delimiter;
            } else {
                size_t end_delimiter_pos = body.find(end_delimiter, pos);
                if (end_delimiter_pos != std::string::npos) {
                    size_t data_end = end_delimiter_pos;
                    if (data_end > pos && body[data_end - 1] == '\n') data_end--;
                    if (data_end > pos && body[data_end - 1] == '\r') data_end--;
                    part.data = body.substr(pos, data_end - pos);
                }
                break;
            }
            
            if (!part.name.empty() || !part.filename.empty()) {
                parts.push_back(part);
            }
        }
        
        return parts;
    }
} // namespace

std::string RequestHandler::handleUploadAvatar(const std::string& body, const std::map<std::string, std::string>& headers) {
    // Extract Content-Type to determine if it's multipart/form-data
    auto content_type_it = headers.find("Content-Type");
    if (content_type_it == headers.end()) {
        content_type_it = headers.find("content-type");
    }
    
    std::string content_type = (content_type_it != headers.end()) ? content_type_it->second : "";
    std::string token;
    std::string file_data;
    std::string filename;
    
    // Check if it's multipart/form-data
    if (content_type.find("multipart/form-data") != std::string::npos) {
        // Extract boundary from Content-Type
        std::string boundary;
        size_t boundary_pos = content_type.find("boundary=");
        if (boundary_pos != std::string::npos) {
            boundary = content_type.substr(boundary_pos + 9);
            if (boundary.front() == '"' && boundary.back() == '"') {
                boundary = boundary.substr(1, boundary.length() - 2);
            } else {
                boundary.erase(0, boundary.find_first_not_of(" \t"));
                boundary.erase(boundary.find_last_not_of(" \t") + 1);
            }
        }
        
        if (boundary.empty()) {
            return JsonParser::createErrorResponse("Boundary not found in Content-Type");
        }
        
        // Parse multipart data
        auto parts = parseMultipartFormData(body, boundary);
        
        for (const auto& part : parts) {
            if (part.name == "token") {
                token = part.data;
                while (!token.empty() && (token.back() == '\r' || token.back() == '\n')) {
                    token.pop_back();
                }
            } else if (part.name == "avatar" && !part.filename.empty()) {
                filename = part.filename;
                file_data = part.data;
            }
        }
    } else {
        // Fallback to JSON with base64 (for compatibility)
        try {
            auto data = JsonParser::parse(body);
            token = data["token"];
            std::string base64_data = data["avatar_data"];
            filename = data.find("filename") != data.end() ? data["filename"] : "avatar.jpg";
            
            if (!base64_data.empty()) {
                size_t comma_pos = base64_data.find(',');
                if (comma_pos != std::string::npos) {
                    base64_data = base64_data.substr(comma_pos + 1);
                }
                file_data = base64Decode(base64_data);
            }
        } catch (...) {
            return JsonParser::createErrorResponse("Invalid request format");
        }
    }

    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }
    if (token.empty()) {
        token = extractSessionTokenFromHeaders(headers);
    }

    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User user = db_manager_.getUserById(user_id);
    
    if (file_data.empty()) {
        return JsonParser::createErrorResponse("Avatar file is required");
    }
    
    // Validate file size (10 MB limit for avatars)
    const long long MAX_AVATAR_SIZE = 10LL * 1024 * 1024;
    if (file_data.length() > MAX_AVATAR_SIZE) {
        return JsonParser::createErrorResponse("Avatar file too large. Maximum size is 10 MB");
    }
    
    // Create avatars directory if it doesn't exist
    std::string avatars_dir = "/var/www/xipher/uploads/avatars";
    try {
        fs::path dir_path(avatars_dir);
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to create avatars directory: " + std::string(e.what()));
        // Fallback to default uploads directory
        avatars_dir = "/root/xipher/uploads/avatars";
        try {
            fs::path dir_path(avatars_dir);
            if (!fs::exists(dir_path)) {
                fs::create_directories(dir_path);
            }
        } catch (const std::exception& e2) {
            Logger::getInstance().error("Failed to create fallback avatars directory: " + std::string(e2.what()));
            return JsonParser::createErrorResponse("Failed to create uploads directory");
        }
    }
    
    // Validate and sanitize filename
    std::string sanitized_filename = filename;
    size_t last_slash = sanitized_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        sanitized_filename = sanitized_filename.substr(last_slash + 1);
    }
    
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\0'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '/'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\\'), sanitized_filename.end());
    
    // Get file extension
    std::string file_ext = "";
    std::string ext_lower;
    bool is_gif = false;
    size_t dot_pos = sanitized_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        file_ext = sanitized_filename.substr(dot_pos);
        ext_lower = file_ext;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        is_gif = (ext_lower == ".gif");
        if (ext_lower != ".jpg" && ext_lower != ".jpeg" && ext_lower != ".png" && ext_lower != ".gif" && ext_lower != ".webp") {
            file_ext = ".jpg";
            is_gif = false;
        }
    } else {
        file_ext = ".jpg";
    }

    if (is_gif && !user.is_premium) {
        return JsonParser::createErrorResponse("Premium required for GIF avatars");
    }
    
    // Generate unique filename: user_id_timestamp.ext
    std::string unique_filename = user_id + "_" + std::to_string(std::time(nullptr)) + file_ext;
    
    // Ensure filename doesn't contain path traversal
    fs::path base_path(avatars_dir);
    fs::path file_path = base_path / unique_filename;
    
    try {
        if (fs::exists(base_path)) {
            fs::path canonical_base = fs::canonical(base_path);
            file_path = canonical_base / unique_filename;
            std::string base_str = canonical_base.string();
            std::string file_str = file_path.string();
            if (file_str.find(base_str) != 0) {
                Logger::getInstance().error("Path traversal detected: " + file_str);
                return JsonParser::createErrorResponse("Invalid file path");
            }
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Path normalization error: " + std::string(e.what()));
        file_path = base_path / unique_filename;
    }
    
    // Save file
    std::ofstream file(file_path.string(), std::ios::binary);
    if (!file.is_open()) {
        Logger::getInstance().error("Failed to open avatar file for writing: " + file_path.string());
        return JsonParser::createErrorResponse("Failed to save avatar");
    }
    file.write(file_data.c_str(), file_data.length());
    file.close();
    
    // Update database with avatar URL
    std::string avatar_url = "/avatars/" + unique_filename;
    
    if (!db_manager_.updateUserAvatar(user_id, avatar_url)) {
        Logger::getInstance().error("Failed to update avatar_url in database for user: " + user_id);
        try {
            if (fs::exists(file_path)) {
                fs::remove(file_path);
            }
        } catch (...) {
        }
        return JsonParser::createErrorResponse("Failed to update avatar in database");
    }
    
    // Broadcast avatar update via WebSocket
    if (ws_sender_) {
        std::ostringstream ws_message;
        ws_message << "{\"type\":\"avatar_updated\",\"user_id\":\"" << JsonParser::escapeJson(user_id) << "\","
                   << "\"avatar_url\":\"" << JsonParser::escapeJson(avatar_url) << "\"}";
        
        ws_sender_(user_id, ws_message.str());
    }
    
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"url\":\"" << JsonParser::escapeJson(avatar_url) << "\"}";
    return oss.str();
}

std::string RequestHandler::handleUploadChannelAvatar(const std::string& body, const std::map<std::string, std::string>& headers) {
    auto content_type_it = headers.find("Content-Type");
    if (content_type_it == headers.end()) {
        content_type_it = headers.find("content-type");
    }

    std::string content_type = (content_type_it != headers.end()) ? content_type_it->second : "";
    std::string token;
    std::string channel_id;
    std::string file_data;
    std::string filename;

    if (content_type.find("multipart/form-data") != std::string::npos) {
        std::string boundary;
        size_t boundary_pos = content_type.find("boundary=");
        if (boundary_pos != std::string::npos) {
            boundary = content_type.substr(boundary_pos + 9);
            if (boundary.front() == '"' && boundary.back() == '"') {
                boundary = boundary.substr(1, boundary.length() - 2);
            } else {
                boundary.erase(0, boundary.find_first_not_of(" \t"));
                boundary.erase(boundary.find_last_not_of(" \t") + 1);
            }
        }

        if (boundary.empty()) {
            return JsonParser::createErrorResponse("Boundary not found in Content-Type");
        }

        auto parts = parseMultipartFormData(body, boundary);
        for (const auto& part : parts) {
            if (part.name == "token") {
                token = part.data;
                while (!token.empty() && (token.back() == '\r' || token.back() == '\n')) {
                    token.pop_back();
                }
            } else if (part.name == "channel_id") {
                channel_id = part.data;
                while (!channel_id.empty() && (channel_id.back() == '\r' || channel_id.back() == '\n')) {
                    channel_id.pop_back();
                }
            } else if (part.name == "avatar" && !part.filename.empty()) {
                filename = part.filename;
                file_data = part.data;
            }
        }
    } else {
        try {
            auto data = JsonParser::parse(body);
            token = data["token"];
            channel_id = data["channel_id"];
            std::string base64_data = data["avatar_data"];
            filename = data.find("filename") != data.end() ? data["filename"] : "channel_avatar.jpg";

            if (!base64_data.empty()) {
                size_t comma_pos = base64_data.find(',');
                if (comma_pos != std::string::npos) {
                    base64_data = base64_data.substr(comma_pos + 1);
                }
                file_data = base64Decode(base64_data);
            }
        } catch (...) {
            return JsonParser::createErrorResponse("Invalid request format");
        }
    }

    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }
    if (token.empty()) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    bool is_v2 = resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id);
    if (is_v2) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
    } else {
        auto legacy = db_manager_.getChannelById(channel_id);
        if (legacy.id.empty()) {
            std::string usernameKey = channel_id;
            if (!usernameKey.empty() && usernameKey[0] == '@') {
                usernameKey = usernameKey.substr(1);
            }
            legacy = db_manager_.getChannelByCustomLink(usernameKey);
            if (!legacy.id.empty()) {
                channel_id = legacy.id;
            }
        }
        if (legacy.id.empty()) {
            return JsonParser::createErrorResponse("Channel not found");
        }
        std::string perm_error;
        if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
    }

    if (file_data.empty()) {
        return JsonParser::createErrorResponse("Avatar file is required");
    }

    const long long MAX_AVATAR_SIZE = 10LL * 1024 * 1024;
    if (file_data.length() > MAX_AVATAR_SIZE) {
        return JsonParser::createErrorResponse("Avatar file too large. Maximum size is 10 MB");
    }

    std::string avatars_dir = "/var/www/xipher/uploads/avatars";
    try {
        fs::path dir_path(avatars_dir);
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
        }
    } catch (...) {
        avatars_dir = "/root/xipher/uploads/avatars";
        try {
            fs::path dir_path(avatars_dir);
            if (!fs::exists(dir_path)) {
                fs::create_directories(dir_path);
            }
        } catch (...) {
            return JsonParser::createErrorResponse("Failed to create uploads directory");
        }
    }

    std::string sanitized_filename = filename;
    size_t last_slash = sanitized_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        sanitized_filename = sanitized_filename.substr(last_slash + 1);
    }
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\0'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '/'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\\'), sanitized_filename.end());

    std::string file_ext = ".jpg";
    size_t dot_pos = sanitized_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        file_ext = sanitized_filename.substr(dot_pos);
        std::string ext_lower = file_ext;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        if (ext_lower != ".jpg" && ext_lower != ".jpeg" && ext_lower != ".png" && ext_lower != ".gif" && ext_lower != ".webp") {
            file_ext = ".jpg";
        }
    }

    std::string unique_filename = "channel_" + channel_id + "_" + std::to_string(std::time(nullptr)) + file_ext;
    fs::path base_path(avatars_dir);
    fs::path file_path = base_path / unique_filename;

    std::ofstream file(file_path.string(), std::ios::binary);
    if (!file.is_open()) {
        return JsonParser::createErrorResponse("Failed to save avatar");
    }
    file.write(file_data.c_str(), file_data.length());
    file.close();

    std::string avatar_url = "/avatars/" + unique_filename;
    bool updated = false;
    if (is_v2) {
        updated = db_manager_.updateChatAvatarV2(channel_id, avatar_url);
    } else {
        updated = db_manager_.updateChannelAvatar(channel_id, avatar_url);
    }
    if (!updated) {
        try { if (fs::exists(file_path)) fs::remove(file_path); } catch (...) {}
        return JsonParser::createErrorResponse("Failed to update channel avatar in database");
    }

    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"url\":\"" << JsonParser::escapeJson(avatar_url) << "\"}";
    return oss.str();
}

std::string RequestHandler::handleUploadBotAvatar(const std::string& body, const std::map<std::string, std::string>& headers) {
    // Similar to handleUploadAvatar, but updates bot_builder_bots.bot_avatar_url.
    auto content_type_it = headers.find("Content-Type");
    if (content_type_it == headers.end()) {
        content_type_it = headers.find("content-type");
    }
    std::string content_type = (content_type_it != headers.end()) ? content_type_it->second : "";

    std::string token;
    std::string bot_id;
    std::string file_data;
    std::string filename;

    if (content_type.find("multipart/form-data") != std::string::npos) {
        std::string boundary;
        size_t boundary_pos = content_type.find("boundary=");
        if (boundary_pos != std::string::npos) {
            boundary = content_type.substr(boundary_pos + 9);
            if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') {
                boundary = boundary.substr(1, boundary.length() - 2);
            } else {
                boundary.erase(0, boundary.find_first_not_of(" \t"));
                boundary.erase(boundary.find_last_not_of(" \t") + 1);
            }
        }
        if (boundary.empty()) {
            return JsonParser::createErrorResponse("Boundary not found in Content-Type");
        }

        auto parts = parseMultipartFormData(body, boundary);
        for (const auto& part : parts) {
            if (part.name == "token") {
                token = part.data;
                while (!token.empty() && (token.back() == '\r' || token.back() == '\n')) token.pop_back();
            } else if (part.name == "bot_id") {
                bot_id = part.data;
                while (!bot_id.empty() && (bot_id.back() == '\r' || bot_id.back() == '\n')) bot_id.pop_back();
            } else if (part.name == "avatar" && !part.filename.empty()) {
                filename = part.filename;
                file_data = part.data;
            }
        }
    } else {
        // JSON fallback
        try {
            auto data = JsonParser::parse(body);
            token = data["token"];
            bot_id = data["bot_id"];
            std::string base64_data = data["avatar_data"];
            filename = data.find("filename") != data.end() ? data["filename"] : "bot_avatar.jpg";
            if (!base64_data.empty()) {
                size_t comma_pos = base64_data.find(',');
                if (comma_pos != std::string::npos) base64_data = base64_data.substr(comma_pos + 1);
                file_data = base64Decode(base64_data);
            }
        } catch (...) {
            return JsonParser::createErrorResponse("Invalid request format");
        }
    }

    if (token == kSessionTokenPlaceholder) {
        token.clear();
    }
    if (token.empty()) {
        token = extractSessionTokenFromHeaders(headers);
    }
    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("token and bot_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor = db_manager_.getUserById(user_id);
    if (actor.id.empty()) {
        return JsonParser::createErrorResponse("User not found");
    }
    if (!actor.is_admin && !actor.is_premium) {
        return JsonParser::createErrorResponse("Premium required");
    }

    // Ownership check
    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (bot.user_id != user_id) {
        return JsonParser::createErrorResponse("Permission denied");
    }

    if (file_data.empty()) {
        return JsonParser::createErrorResponse("Avatar file is required");
    }

    const long long MAX_AVATAR_SIZE = 10LL * 1024 * 1024;
    if (file_data.length() > MAX_AVATAR_SIZE) {
        return JsonParser::createErrorResponse("Avatar file too large. Maximum size is 10 MB");
    }

    // Same avatars dir as users
    std::string avatars_dir = "/var/www/xipher/uploads/avatars";
    try {
        fs::path dir_path(avatars_dir);
        if (!fs::exists(dir_path)) fs::create_directories(dir_path);
    } catch (...) {
        avatars_dir = "/root/xipher/uploads/avatars";
        try {
            fs::path dir_path(avatars_dir);
            if (!fs::exists(dir_path)) fs::create_directories(dir_path);
        } catch (...) {
            return JsonParser::createErrorResponse("Failed to create uploads directory");
        }
    }

    std::string sanitized_filename = filename;
    size_t last_slash = sanitized_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) sanitized_filename = sanitized_filename.substr(last_slash + 1);
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\0'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '/'), sanitized_filename.end());
    sanitized_filename.erase(std::remove(sanitized_filename.begin(), sanitized_filename.end(), '\\'), sanitized_filename.end());

    std::string file_ext = ".jpg";
    size_t dot_pos = sanitized_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        file_ext = sanitized_filename.substr(dot_pos);
        std::string ext_lower = file_ext;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        if (ext_lower != ".jpg" && ext_lower != ".jpeg" && ext_lower != ".png" && ext_lower != ".gif" && ext_lower != ".webp") {
            file_ext = ".jpg";
        }
    }

    std::string unique_filename = "bot_" + bot_id + "_" + std::to_string(std::time(nullptr)) + file_ext;
    fs::path base_path(avatars_dir);
    fs::path file_path = base_path / unique_filename;

    std::ofstream file(file_path.string(), std::ios::binary);
    if (!file.is_open()) {
        return JsonParser::createErrorResponse("Failed to save avatar");
    }
    file.write(file_data.c_str(), file_data.length());
    file.close();

    std::string avatar_url = "/avatars/" + unique_filename;
    if (!db_manager_.updateBotBuilderAvatarUrl(bot_id, avatar_url)) {
        try { if (fs::exists(file_path)) fs::remove(file_path); } catch (...) {}
        return JsonParser::createErrorResponse("Failed to update bot avatar in database");
    }

    // Also update bot user avatar so it shows up in chats/groups.
    if (!bot.bot_user_id.empty()) {
        db_manager_.updateUserAvatar(bot.bot_user_id, avatar_url);
    }
    
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"url\":\"" << JsonParser::escapeJson(avatar_url) << "\"}";
    return oss.str();
}

// Call handlers implementation
std::string RequestHandler::handleCallNotification(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    std::string call_type = data.find("call_type") != data.end() ? data["call_type"] : "video";
    
    if (token.empty() || receiver_id.empty()) {
        return JsonParser::createErrorResponse("Token and receiver_id required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Получаем username отправителя
    std::string sender_username = "";
    User sender_user = db_manager_.getUserById(sender_id);
    if (!sender_user.id.empty()) {
        sender_username = sender_user.username;
    }

    // Создаем уникальный ID звонка
    std::string call_id = sender_id + "_" + receiver_id + "_" + std::to_string(std::time(nullptr));

    // Сохраняем информацию о звонке в памяти
    {
        std::lock_guard<std::mutex> lock(calls_mutex_);
        std::map<std::string, std::string> call_info;
        call_info["call_id"] = call_id;
        call_info["caller_id"] = sender_id;
        call_info["caller_username"] = sender_username;
        call_info["receiver_id"] = receiver_id;
        call_info["call_type"] = call_type;
        call_info["created_at"] = std::to_string(std::time(nullptr));
        call_info["status"] = "pending";
        active_calls_[receiver_id] = call_info;
    }
    
    Logger::getInstance().info("Call notification: " + sender_id + " (" + sender_username + ") -> " + receiver_id + " (" + call_type + ")");

    if (receiver_id != sender_id) {
        try {
            auto tokens = db_manager_.getPushTokensForUser(receiver_id);
            if (tokens.empty()) {
                Logger::getInstance().warning("Push skipped: no tokens for call receiver " + receiver_id);
            } else {
                bool fcm_ready = fcm_client_.isReady();
                bool rustore_ready = rustore_client_.isReady();
                auto summary = summarizePushPlatforms(tokens);
                if (summary.has_fcm && !fcm_ready) {
                    Logger::getInstance().warning("Push skipped: FCM not ready for call receiver " + receiver_id);
                }
                if (summary.has_rustore && !rustore_ready) {
                    Logger::getInstance().warning("Push skipped: RuStore not ready for call receiver " + receiver_id);
                }
                std::string caller_name = !sender_username.empty() ? sender_username : "Unknown user";
                std::map<std::string, std::string> payload;
                payload["type"] = "call";
                payload["call_id"] = call_id;
                payload["caller_id"] = sender_id;
                payload["caller_name"] = caller_name;
                payload["call_type"] = call_type;
                payload["title"] = "Incoming call";
                payload["body"] = caller_name;
                sendPushTokensForUser(db_manager_, fcm_client_, rustore_client_, receiver_id, tokens,
                                      "Incoming call", caller_name, payload, "calls", "HIGH",
                                      fcm_ready, rustore_ready);
            }
        } catch (...) {
            Logger::getInstance().warning("Failed to send push call notification");
        }
    }
    
    return JsonParser::createSuccessResponse("Call notification sent");
}

std::string RequestHandler::handleCallResponse(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    std::string response = data["response"]; // "accepted", "rejected", "ended"
    
    if (token.empty() || receiver_id.empty() || response.empty()) {
        return JsonParser::createErrorResponse("Token, receiver_id and response required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Обновляем статус звонка
    std::lock_guard<std::mutex> lock(calls_mutex_);
    
    // Сохраняем ответ для проверки (для исходящих звонков)
    std::string call_key = sender_id + "_" + receiver_id;
    call_responses_[call_key] = response;
    
    if (response == "accepted" || response == "rejected" || response == "ended") {
        // Удаляем звонок из активных после ответа
        auto it = active_calls_.find(receiver_id);
        if (it != active_calls_.end() && it->second["caller_id"] == sender_id) {
            active_calls_.erase(it);
        }
        
        // Удаляем ответ через 30 секунд
        if (response == "ended") {
            call_responses_.erase(call_key);
        }
    }
    
    Logger::getInstance().info("Call response: " + sender_id + " -> " + receiver_id + " (" + response + ")");
    
    return JsonParser::createSuccessResponse("Call response sent");
}

std::string RequestHandler::handleGroupCallNotification(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string call_type = data.find("call_type") != data.end() ? data["call_type"] : "video";
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем, что пользователь является участником группы
    auto member = db_manager_.getGroupMember(group_id, sender_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("User is not a member of this group");
    }
    
    // Получаем список участников группы
    auto members = db_manager_.getGroupMembers(group_id);
    
    // Получаем username отправителя
    std::string sender_username = "";
    User sender_user = db_manager_.getUserById(sender_id);
    if (!sender_user.id.empty()) {
        sender_username = sender_user.username;
    }
    
    // Отправляем уведомление всем участникам группы через WebSocket
    // Это будет обработано в WebSocket обработчике
    // Здесь просто сохраняем информацию о групповом звонке
    
    Logger::getInstance().info("Group call notification: " + sender_id + " (" + sender_username + ") -> group " + group_id + " (" + call_type + ")");
    
    // Возвращаем список участников для фронтенда
    std::ostringstream oss;
    oss << "{\"success\":true,\"members\":[";
    
    bool first = true;
    for (const auto& m : members) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << JsonParser::escapeJson(m.user_id) << "\"";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleCallOffer(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    
    // Decode offer (supports optional base64 encoding)
    auto decodePayload = [&](const std::string& value, const std::string& encoding_key) -> std::string {
        std::string enc = "";
        auto it = data.find(encoding_key);
        if (it != data.end()) {
            enc = it->second;
        }
        if (enc == "b64") {
            return base64Decode(value);
        }
        // Heuristic: if looks like base64 (padding and charset) — try decode
        bool looks_b64 = !value.empty() &&
            (value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") == std::string::npos) &&
            (value.size() % 4 == 0);
        if (looks_b64) {
            std::string decoded = base64Decode(value);
            if (!decoded.empty()) return decoded;
        }
        return value;
    };
    
    std::string offer = decodePayload(data["offer"], "offer_encoding");
    
    if (token.empty() || receiver_id.empty() || offer.empty()) {
        return JsonParser::createErrorResponse("Token, receiver_id and offer required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Сохраняем offer для получателя
    std::lock_guard<std::mutex> lock(calls_mutex_);
    std::string call_key = sender_id + "_" + receiver_id;
    call_offers_[call_key] = offer;
    
    // Обновляем timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    call_signaling_timestamps_[call_key] = timestamp;
    
    Logger::getInstance().info("Call offer saved: " + sender_id + " -> " + receiver_id);
    
    return JsonParser::createSuccessResponse("Offer received");
}

std::string RequestHandler::handleCallAnswer(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    
    auto decodePayload = [&](const std::string& value, const std::string& encoding_key) -> std::string {
        std::string enc = "";
        auto it = data.find(encoding_key);
        if (it != data.end()) {
            enc = it->second;
        }
        if (enc == "b64") {
            return base64Decode(value);
        }
        bool looks_b64 = !value.empty() &&
            (value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") == std::string::npos) &&
            (value.size() % 4 == 0);
        if (looks_b64) {
            std::string decoded = base64Decode(value);
            if (!decoded.empty()) return decoded;
        }
        return value;
    };
    
    std::string answer = decodePayload(data["answer"], "answer_encoding");
    Logger::getInstance().info("HTTP call_answer received: receiver_id=" + receiver_id +
        " token=" + (token.empty() ? "empty" : "present") +
        " answer_len=" + std::to_string(answer.size()));
    
    if (token.empty() || receiver_id.empty() || answer.empty()) {
        return JsonParser::createErrorResponse("Token, receiver_id and answer required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        Logger::getInstance().warning("HTTP call_answer invalid token for receiver_id=" + receiver_id);
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Сохраняем answer для инициатора звонка
    // sender_id здесь - это тот, кто принял звонок и отправляет answer
    // receiver_id - это инициатор звонка, который должен получить answer
    {
        std::lock_guard<std::mutex> lock(calls_mutex_);
        std::string call_key = receiver_id + "_" + sender_id; // Инициатор_Получатель
        call_answers_[call_key] = answer;

        // Обновляем timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()
        ).count();
        call_signaling_timestamps_[call_key] = timestamp;
    }

    Logger::getInstance().info("Call answer saved: " + sender_id + " -> " + receiver_id);
    if (ws_sender_) {
        std::string from_username = "Unknown";
        try {
            auto user = db_manager_.getUserById(sender_id);
            if (!user.id.empty() && !user.username.empty()) {
                from_username = user.username;
            }
        } catch (...) {
            Logger::getInstance().warning("Could not resolve username for call_answer sender: " + sender_id);
        }
        std::string payload = "{\"type\":\"call_answer\","
            "\"receiver_id\":\"" + JsonParser::escapeJson(receiver_id) + "\","
            "\"target_user_id\":\"" + JsonParser::escapeJson(receiver_id) + "\","
            "\"from_user_id\":\"" + JsonParser::escapeJson(sender_id) + "\","
            "\"from_username\":\"" + JsonParser::escapeJson(from_username) + "\","
            "\"answer\":\"" + JsonParser::escapeJson(answer) + "\"}";
        ws_sender_(receiver_id, payload);
        Logger::getInstance().info("Forwarded call_answer via WS (HTTP): " + sender_id + " -> " + receiver_id);
    }
    
    return JsonParser::createSuccessResponse("Answer received");
}

std::string RequestHandler::handleCallIce(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    
    auto decodePayload = [&](const std::string& value, const std::string& encoding_key) -> std::string {
        std::string enc = "";
        auto it = data.find(encoding_key);
        if (it != data.end()) {
            enc = it->second;
        }
        if (enc == "b64") {
            return base64Decode(value);
        }
        bool looks_b64 = !value.empty() &&
            (value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") == std::string::npos) &&
            (value.size() % 4 == 0);
        if (looks_b64) {
            std::string decoded = base64Decode(value);
            if (!decoded.empty()) return decoded;
        }
        return value;
    };
    
    std::string candidate = decodePayload(data["candidate"], "candidate_encoding");
    Logger::getInstance().info("HTTP call_ice received: receiver_id=" + receiver_id +
        " token=" + (token.empty() ? "empty" : "present") +
        " candidate_len=" + std::to_string(candidate.size()));
    
    if (token.empty() || receiver_id.empty() || candidate.empty()) {
        return JsonParser::createErrorResponse("Token, receiver_id and candidate required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        Logger::getInstance().warning("HTTP call_ice invalid token for receiver_id=" + receiver_id);
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Сохраняем ICE кандидат для получателя
    {
        std::lock_guard<std::mutex> lock(calls_mutex_);
        // Определяем правильный ключ (может быть в любом направлении)
        std::string call_key1 = sender_id + "_" + receiver_id;
        std::string call_key2 = receiver_id + "_" + sender_id;

        // Добавляем кандидат в оба возможных направления (на случай если ключ не совпадает)
        call_ice_candidates_[call_key1].push_back(candidate);
        call_ice_candidates_[call_key2].push_back(candidate);

        // Обновляем timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()
        ).count();
        call_signaling_timestamps_[call_key1] = timestamp;
        call_signaling_timestamps_[call_key2] = timestamp;
    }

    Logger::getInstance().info("Call ICE candidate saved: " + sender_id + " -> " + receiver_id);
    if (ws_sender_) {
        std::string from_username = "Unknown";
        try {
            auto user = db_manager_.getUserById(sender_id);
            if (!user.id.empty() && !user.username.empty()) {
                from_username = user.username;
            }
        } catch (...) {
            Logger::getInstance().warning("Could not resolve username for call_ice sender: " + sender_id);
        }
        std::string payload = "{\"type\":\"call_ice_candidate\","
            "\"receiver_id\":\"" + JsonParser::escapeJson(receiver_id) + "\","
            "\"target_user_id\":\"" + JsonParser::escapeJson(receiver_id) + "\","
            "\"from_user_id\":\"" + JsonParser::escapeJson(sender_id) + "\","
            "\"from_username\":\"" + JsonParser::escapeJson(from_username) + "\","
            "\"candidate\":\"" + JsonParser::escapeJson(candidate) + "\"}";
        ws_sender_(receiver_id, payload);
        Logger::getInstance().info("Forwarded call_ice_candidate via WS (HTTP): " + sender_id + " -> " + receiver_id);
    }
    
    return JsonParser::createSuccessResponse("ICE candidate received");
}

std::string RequestHandler::handleGetCallOffer(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    
    if (token.empty() || receiver_id.empty()) {
        return JsonParser::createErrorResponse("Token and receiver_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Получаем offer для текущего пользователя (он должен быть получателем)
    // receiver_id в запросе - это инициатор звонка (caller_id)
    // user_id - это получатель звонка
    std::lock_guard<std::mutex> lock(calls_mutex_);
    std::string call_key = receiver_id + "_" + user_id; // Инициатор_Получатель
    
    auto it = call_offers_.find(call_key);
    if (it != call_offers_.end()) {
        std::map<std::string, std::string> response_data;
        response_data["offer"] = it->second;
        
        // Удаляем offer после получения (одноразовое использование)
        call_offers_.erase(it);
        
        Logger::getInstance().info("Call offer retrieved: " + receiver_id + " -> " + user_id);
        return JsonParser::createSuccessResponse("Offer retrieved", response_data);
    }
    
    // Возможно, ключ в обратном порядке
    std::string call_key_reverse = user_id + "_" + receiver_id;
    auto it_reverse = call_offers_.find(call_key_reverse);
    if (it_reverse != call_offers_.end()) {
        std::map<std::string, std::string> response_data;
        response_data["offer"] = it_reverse->second;
        
        call_offers_.erase(it_reverse);
        
        Logger::getInstance().info("Call offer retrieved (reverse): " + receiver_id + " -> " + user_id);
        return JsonParser::createSuccessResponse("Offer retrieved", response_data);
    }
    
    return JsonParser::createErrorResponse("No offer found");
}

std::string RequestHandler::handleGetCallAnswer(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    
    if (token.empty() || receiver_id.empty()) {
        return JsonParser::createErrorResponse("Token and receiver_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Получаем answer для инициатора звонка
    // user_id - это инициатор звонка
    // receiver_id - это тот, кто принял звонок и отправил answer
    std::lock_guard<std::mutex> lock(calls_mutex_);
    std::string call_key = user_id + "_" + receiver_id; // Инициатор_Получатель
    
    auto it = call_answers_.find(call_key);
    if (it != call_answers_.end()) {
        std::map<std::string, std::string> response_data;
        response_data["answer"] = it->second;
        
        // Удаляем answer после получения (одноразовое использование)
        call_answers_.erase(it);
        
        Logger::getInstance().info("Call answer retrieved: " + user_id + " <- " + receiver_id);
        return JsonParser::createSuccessResponse("Answer retrieved", response_data);
    }
    
    // Возможно, ключ в обратном порядке
    std::string call_key_reverse = receiver_id + "_" + user_id;
    auto it_reverse = call_answers_.find(call_key_reverse);
    if (it_reverse != call_answers_.end()) {
        std::map<std::string, std::string> response_data;
        response_data["answer"] = it_reverse->second;
        
        call_answers_.erase(it_reverse);
        
        Logger::getInstance().info("Call answer retrieved (reverse): " + user_id + " <- " + receiver_id);
        return JsonParser::createSuccessResponse("Answer retrieved", response_data);
    }
    
    return JsonParser::createErrorResponse("No answer found");
}

std::string RequestHandler::handleGetCallIce(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    int64_t last_check = 0;
    
    if (data.find("last_check") != data.end() && !data["last_check"].empty()) {
        try {
            last_check = std::stoll(data["last_check"]);
        } catch (...) {
            last_check = 0;
        }
    }
    
    if (token.empty() || receiver_id.empty()) {
        return JsonParser::createErrorResponse("Token and receiver_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Получаем ICE кандидаты для текущего пользователя
    std::lock_guard<std::mutex> lock(calls_mutex_);
    
    // Проверяем оба возможных направления
    std::string call_key1 = user_id + "_" + receiver_id;
    std::string call_key2 = receiver_id + "_" + user_id;
    
    std::vector<std::string> candidates;
    
    // Получаем кандидаты из первого направления
    auto it1 = call_ice_candidates_.find(call_key1);
    if (it1 != call_ice_candidates_.end()) {
        // Фильтруем кандидаты по timestamp, если указан last_check
        if (last_check > 0) {
            auto timestamp_it = call_signaling_timestamps_.find(call_key1);
            if (timestamp_it != call_signaling_timestamps_.end() && timestamp_it->second > last_check) {
                candidates.insert(candidates.end(), it1->second.begin(), it1->second.end());
            }
        } else {
            candidates.insert(candidates.end(), it1->second.begin(), it1->second.end());
        }
    }
    
    // Получаем кандидаты из второго направления
    auto it2 = call_ice_candidates_.find(call_key2);
    if (it2 != call_ice_candidates_.end()) {
        // Фильтруем кандидаты по timestamp, если указан last_check
        if (last_check > 0) {
            auto timestamp_it = call_signaling_timestamps_.find(call_key2);
            if (timestamp_it != call_signaling_timestamps_.end() && timestamp_it->second > last_check) {
                candidates.insert(candidates.end(), it2->second.begin(), it2->second.end());
            }
        } else {
            candidates.insert(candidates.end(), it2->second.begin(), it2->second.end());
        }
    }
    
    if (!candidates.empty()) {
        // Удаляем дубликаты
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        
        // Конвертируем в JSON массив (экранируем каждый кандидат)
        std::string candidates_json = "[";
        for (size_t i = 0; i < candidates.size(); i++) {
            if (i > 0) candidates_json += ",";
            // Экранируем JSON специальные символы
            std::string escaped = candidates[i];
            std::string escaped_str;
            for (char c : escaped) {
                if (c == '"') escaped_str += "\\\"";
                else if (c == '\\') escaped_str += "\\\\";
                else if (c == '\n') escaped_str += "\\n";
                else if (c == '\r') escaped_str += "\\r";
                else if (c == '\t') escaped_str += "\\t";
                else escaped_str += c;
            }
            candidates_json += "\"" + escaped_str + "\"";
        }
        candidates_json += "]";
        
        std::map<std::string, std::string> response_data;
        response_data["candidates"] = candidates_json;
        
        // Очищаем полученные кандидаты (одноразовое использование)
        if (it1 != call_ice_candidates_.end()) {
            call_ice_candidates_[call_key1].clear();
        }
        if (it2 != call_ice_candidates_.end()) {
            call_ice_candidates_[call_key2].clear();
        }
        
        Logger::getInstance().info("Call ICE candidates retrieved: " + std::to_string(candidates.size()) + " candidates");
        return JsonParser::createSuccessResponse("ICE candidates retrieved", response_data);
    }
    
    return JsonParser::createErrorResponse("No ICE candidates found");
}

// Group handlers implementation
std::string RequestHandler::handleCreateGroup(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string name = data.find("name") != data.end() ? data["name"] : "";
    std::string description = data.find("description") != data.end() ? data["description"] : "";
    
    if (token.empty() || name.empty()) {
        return JsonParser::createErrorResponse("Token and name required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Валидация имени группы
    if (name.length() < 3 || name.length() > 255) {
        return JsonParser::createErrorResponse("Group name must be between 3 and 255 characters");
    }
    
    std::string group_id;
    if (db_manager_.createGroup(name, description, user_id, group_id)) {
        Logger::getInstance().info("Group created: " + group_id + " by user: " + user_id);
        std::map<std::string, std::string> response_data;
        response_data["group_id"] = group_id;
        return JsonParser::createSuccessResponse("Group created successfully", response_data);
    }
    
    Logger::getInstance().error("Failed to create group for user: " + user_id);
    return JsonParser::createErrorResponse("Failed to create group. Check database connection and schema.");
}

std::string RequestHandler::handleGetGroups(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto groups = db_manager_.getUserGroups(user_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"groups\":[";
    
    bool first = true;
    for (const auto& group : groups) {
        if (!first) oss << ",";
        first = false;
        
        // Get user's role in this group
        auto member = db_manager_.getGroupMember(group.id, user_id);
        std::string user_role = member.user_id.empty() ? "none" : member.role;
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(group.id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(group.name) << "\","
            << "\"description\":\"" << JsonParser::escapeJson(group.description) << "\","
            << "\"creator_id\":\"" << JsonParser::escapeJson(group.creator_id) << "\","
            << "\"user_role\":\"" << JsonParser::escapeJson(user_role) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(group.created_at) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetGroupMembers(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем, что пользователь является участником группы
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("User is not a member of this group");
    }
    
    // Получаем список участников группы
    auto members = db_manager_.getGroupMembers(group_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"members\":[";
    
    bool first = true;
    for (const auto& m : members) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << JsonParser::escapeJson(m.id) << "\","
            << "\"user_id\":\"" << JsonParser::escapeJson(m.user_id) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(m.username) << "\","
            << "\"role\":\"" << JsonParser::escapeJson(m.role) << "\","
            << "\"permissions\":" << (m.permissions.empty() ? "{}" : m.permissions) << ","
            << "\"is_muted\":" << (m.is_muted ? "true" : "false") << ","
            << "\"is_banned\":" << (m.is_banned ? "true" : "false") << ","
            << "\"joined_at\":\"" << JsonParser::escapeJson(m.joined_at) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetGroupMessages(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    int limit = 50;
    if (data.find("limit") != data.end()) {
        try {
            limit = std::stoi(data["limit"]);
            if (limit < 1 || limit > 100) limit = 50;
        } catch (...) {
            limit = 50;
        }
    }
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем, что пользователь является участником группы
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("You are not a member of this group");
    }
    
    auto messages = db_manager_.getGroupMessages(group_id, limit);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"messages\":[";
    
    bool first = true;
    for (const auto& msg : messages) {
        if (!first) oss << ",";
        first = false;
        
        std::string time = msg.created_at.length() >= 16 ? msg.created_at.substr(11, 5) : "";
        bool isSent = (msg.sender_id == user_id);
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(msg.id) << "\","
            << "\"group_id\":\"" << JsonParser::escapeJson(msg.group_id) << "\","
            << "\"sender_id\":\"" << JsonParser::escapeJson(msg.sender_id) << "\","
            << "\"sender_username\":\"" << JsonParser::escapeJson(msg.sender_username) << "\","
            << "\"sent\":" << (isSent ? "true" : "false") << ","
            << "\"content\":\"" << JsonParser::escapeJson(msg.content) << "\","
            << "\"message_type\":\"" << JsonParser::escapeJson(msg.message_type) << "\","
            << "\"file_path\":\"" << JsonParser::escapeJson(msg.file_path) << "\","
            << "\"file_name\":\"" << JsonParser::escapeJson(msg.file_name) << "\","
            << "\"file_size\":" << msg.file_size << ","
            << "\"reply_to_message_id\":\"" << JsonParser::escapeJson(msg.reply_to_message_id) << "\","
            << "\"forwarded_from_user_id\":\"" << JsonParser::escapeJson(msg.forwarded_from_user_id) << "\","
            << "\"forwarded_from_username\":\"" << JsonParser::escapeJson(msg.forwarded_from_username) << "\","
            << "\"forwarded_from_message_id\":\"" << JsonParser::escapeJson(msg.forwarded_from_message_id) << "\","
            << "\"is_pinned\":" << (msg.is_pinned ? "true" : "false") << ","
            << "\"time\":\"" << time << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSendGroupMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string content = data.find("content") != data.end() ? data["content"] : "";
    std::string message_type = data.find("message_type") != data.end() ? data["message_type"] : "text";
    std::string file_path = data.find("file_path") != data.end() ? data["file_path"] : "";
    std::string file_name = data.find("file_name") != data.end() ? data["file_name"] : "";
    long long file_size = 0;
    if (data.find("file_size") != data.end()) {
        try {
            file_size = std::stoll(data["file_size"]);
        } catch (...) {
            file_size = 0;
        }
    }
    std::string reply_to_message_id = data.find("reply_to_message_id") != data.end() ? data["reply_to_message_id"] : "";
    std::string forwarded_from_user_id = data.find("forwarded_from_user_id") != data.end() ? data["forwarded_from_user_id"] : "";
    std::string forwarded_from_username = data.find("forwarded_from_username") != data.end() ? data["forwarded_from_username"] : "";
    std::string forwarded_from_message_id = data.find("forwarded_from_message_id") != data.end() ? data["forwarded_from_message_id"] : "";
    
    if (token.empty() || group_id.empty() || content.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and content required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем, что пользователь является участником группы и не забанен
    auto member = db_manager_.getGroupMember(group_id, sender_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("You are not a member of this group");
    }
    
    if (member.is_banned) {
        return JsonParser::createErrorResponse("You are banned from this group");
    }
    
    if (member.is_muted) {
        return JsonParser::createErrorResponse("You are muted in this group");
    }
    
    if (db_manager_.sendGroupMessage(group_id, sender_id, content, message_type, file_path, file_name, file_size,
                                    reply_to_message_id, forwarded_from_user_id, forwarded_from_username, forwarded_from_message_id)) {
        // Lightweight group bots: bots that are members of the group may react (notes, rules, automod, etc.)
        try {
            auto bot_user_ids = db_manager_.getGroupBotUserIds(group_id);
            for (const auto& bot_user_id : bot_user_ids) {
                if (bot_user_id.empty() || bot_user_id == sender_id) continue;
                auto bot = db_manager_.getBotBuilderBotByUserId(bot_user_id);
                if (bot.id.empty()) continue;
                LiteBotRuntime::onGroupMessage(db_manager_, bot, group_id, sender_id, member.role, content);
            }
        } catch (...) {
            // best-effort
        }

        bool fcm_ready = fcm_client_.isReady();
        bool rustore_ready = rustore_client_.isReady();
        if (!fcm_ready && !rustore_ready) {
            Logger::getInstance().warning("Push skipped: no providers ready for group " + group_id);
        } else {
            if (!fcm_ready) {
                Logger::getInstance().warning("Push skipped: FCM not ready for group " + group_id);
            }
            if (!rustore_ready) {
                Logger::getInstance().warning("Push skipped: RuStore not ready for group " + group_id);
            }
            try {
                auto group = db_manager_.getGroupById(group_id);
                std::string group_title = !group.name.empty() ? group.name : "Group";
                User sender_user = db_manager_.getUserById(sender_id);
                std::string sender_name = !sender_user.username.empty() ? sender_user.username : "User";

                std::string body_text = content;
                if (message_type != "text") {
                    if (message_type == "voice") {
                        body_text = "Voice message";
                    } else if (message_type == "file") {
                        body_text = file_name.empty() ? "File" : ("File: " + file_name);
                    } else if (message_type == "image") {
                        body_text = "Photo";
                    } else {
                        body_text = "Message";
                    }
                }
                if (!sender_name.empty()) {
                    body_text = sender_name + ": " + body_text;
                }
                if (body_text.size() > 120) {
                    body_text = body_text.substr(0, 117) + "...";
                }

                std::map<std::string, std::string> payload;
                payload["type"] = "group_message";
                payload["chat_type"] = "group";
                payload["chat_id"] = group_id;
                payload["chat_title"] = group_title;
                payload["sender_id"] = sender_id;
                payload["sender_name"] = sender_name;
                payload["message_type"] = message_type;
                payload["title"] = group_title;
                payload["body"] = body_text;

                std::unordered_set<std::string> bot_users;
                try {
                    for (const auto& bot_user_id : db_manager_.getGroupBotUserIds(group_id)) {
                        if (!bot_user_id.empty()) bot_users.insert(bot_user_id);
                    }
                } catch (...) {
                    // ignore
                }

                auto members = db_manager_.getGroupMembers(group_id);
                for (const auto& m : members) {
                    if (m.user_id.empty() || m.user_id == sender_id || m.is_banned) continue;
                    if (!bot_users.empty() && bot_users.count(m.user_id) > 0) continue;
                    auto tokens = db_manager_.getPushTokensForUser(m.user_id);
                    if (tokens.empty()) {
                        Logger::getInstance().warning("Push skipped: no tokens for group member " + m.user_id);
                        continue;
                    }
                    sendPushTokensForUser(db_manager_, fcm_client_, rustore_client_, m.user_id, tokens,
                                          group_title, body_text, payload, "channel_messages", "HIGH",
                                          fcm_ready, rustore_ready);
                }
            } catch (...) {
                Logger::getInstance().warning("Failed to send group message notification");
            }
        }

        return JsonParser::createSuccessResponse("Message sent");
    }
    
    return JsonParser::createErrorResponse("Failed to send message");
}

std::string RequestHandler::handleCreateGroupInvite(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    int expires_in_seconds = 0;
    if (data.find("expires_in_seconds") != data.end()) {
        try {
            expires_in_seconds = std::stoi(data["expires_in_seconds"]);
        } catch (...) {
            expires_in_seconds = 0;
        }
    }
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем права администратора
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
        return JsonParser::createErrorResponse("Only admins can create invite links");
    }
    
    std::string invite_link = db_manager_.createGroupInviteLink(group_id, user_id, expires_in_seconds);
    if (invite_link.empty()) {
        return JsonParser::createErrorResponse("Failed to create invite link");
    }
    
    std::map<std::string, std::string> response_data;
    response_data["invite_link"] = invite_link;
    return JsonParser::createSuccessResponse("Invite link created", response_data);
}

std::string RequestHandler::handleLeaveGroup(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];

    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("You are not a member of this group");
    }
    if (member.role == "creator") {
        return JsonParser::createErrorResponse("Group creator cannot leave");
    }

    if (db_manager_.removeGroupMember(group_id, user_id)) {
        return JsonParser::createSuccessResponse("Left group");
    }
    return JsonParser::createErrorResponse("Failed to leave group");
}

std::string RequestHandler::handleJoinGroup(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string invite_link = data["invite_link"];
    
    if (token.empty() || invite_link.empty()) {
        return JsonParser::createErrorResponse("Token and invite_link required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    std::string joined_group_id = db_manager_.joinGroupByInviteLink(invite_link, user_id);
    if (!joined_group_id.empty()) {
        // Welcome bots in the group
        try {
            auto u = db_manager_.getUserById(user_id);
            const std::string joined_username = u.username;
            auto bot_user_ids = db_manager_.getGroupBotUserIds(joined_group_id);
            for (const auto& bot_user_id : bot_user_ids) {
                if (bot_user_id.empty()) continue;
                auto bot = db_manager_.getBotBuilderBotByUserId(bot_user_id);
                if (bot.id.empty()) continue;
                LiteBotRuntime::onGroupMemberJoined(db_manager_, bot, joined_group_id, user_id, joined_username);
            }
        } catch (...) {
            // best-effort
        }

        std::map<std::string, std::string> response_data;
        response_data["group_id"] = joined_group_id;
        return JsonParser::createSuccessResponse("Successfully joined group", response_data);
    }
    
    return JsonParser::createErrorResponse("Failed to join group. Invalid or expired invite link");
}

std::string RequestHandler::handleUpdateGroupName(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string new_name = data["new_name"];
    
    if (token.empty() || group_id.empty() || new_name.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and new_name required");
    }
    
    if (new_name.length() < 3 || new_name.length() > 255) {
        return JsonParser::createErrorResponse("Group name must be between 3 and 255 characters");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем права администратора
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
        return JsonParser::createErrorResponse("Only admins can change group name");
    }
    
    if (db_manager_.updateGroupName(group_id, new_name)) {
        return JsonParser::createSuccessResponse("Group name updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update group name");
}

std::string RequestHandler::handleUpdateGroupDescription(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string new_description = data["new_description"];
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем права администратора
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
        return JsonParser::createErrorResponse("Only admins can change group description");
    }
    
    if (db_manager_.updateGroupDescription(group_id, new_description)) {
        return JsonParser::createSuccessResponse("Group description updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update group description");
}

std::string RequestHandler::handlePinMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string group_id = data.find("group_id") != data.end() ? data["group_id"] : "";
    std::string channel_id = data.find("channel_id") != data.end() ? data["channel_id"] : "";

    if (token.empty() || message_id.empty()) {
        return JsonParser::createErrorResponse("Token and message_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // Legacy direct chat (messages table)
    if (group_id.empty() && channel_id.empty()) {
        auto msg = db_manager_.getMessageById(message_id);
        if (msg.id.empty()) {
            return JsonParser::createErrorResponse("Message not found");
        }
        if (msg.sender_id != user_id && msg.receiver_id != user_id) {
            return JsonParser::createErrorResponse("Permission denied");
        }
        std::string peer_id = (msg.sender_id == user_id) ? msg.receiver_id : msg.sender_id;
        if (db_manager_.pinDirectMessage(user_id, peer_id, message_id, user_id)) {
            if (ws_sender_) {
                std::string payload = "{\"type\":\"message_pinned\",\"chat_type\":\"chat\",\"peer_id\":\"" +
                    JsonParser::escapeJson(peer_id) + "\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"pinned_by\":\"" + JsonParser::escapeJson(user_id) + "\"}";
                ws_sender_(user_id, payload);
                if (!peer_id.empty() && peer_id != user_id) {
                    ws_sender_(peer_id, payload);
                }
            }
            return JsonParser::createSuccessResponse("Message pinned");
        }
        return JsonParser::createErrorResponse("Failed to pin message");
    }

    if (!group_id.empty()) {
        auto member = db_manager_.getGroupMember(group_id, user_id);
        if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
            return JsonParser::createErrorResponse("Only admins can pin messages");
        }

        if (db_manager_.pinGroupMessage(group_id, message_id, user_id)) {
            if (ws_sender_) {
                auto members = db_manager_.getGroupMembers(group_id);
                std::string payload = "{\"type\":\"message_pinned\",\"chat_type\":\"group\",\"chat_id\":\"" +
                    JsonParser::escapeJson(group_id) + "\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"pinned_by\":\"" + JsonParser::escapeJson(user_id) + "\"}";
                for (const auto& m : members) {
                    if (!m.user_id.empty()) {
                        ws_sender_(m.user_id, payload);
                    }
                }
            }
            return JsonParser::createSuccessResponse("Message pinned");
        }
        return JsonParser::createErrorResponse("Failed to pin message");
    }

    if (!channel_id.empty()) {
        DatabaseManager::ChannelMember member;
        std::string error_message;
        if (!checkChannelPermission(user_id, channel_id, ChannelPermission::PinMessage, &member, &error_message)) {
            return JsonParser::createErrorResponse(error_message.empty() ? "Permission denied" : error_message);
        }

        Chat chat;
        std::string resolved_channel_id = channel_id;
        bool is_v2 = resolveChannelV2(db_manager_, channel_id, &chat, &resolved_channel_id);
        bool pinned = false;
        if (is_v2) {
            pinned = db_manager_.pinChatMessageV2(resolved_channel_id, message_id, user_id);
        } else {
            pinned = db_manager_.pinChannelMessage(resolved_channel_id, message_id, user_id);
        }
        if (pinned) {
            if (ws_sender_) {
                std::string payload = "{\"type\":\"message_pinned\",\"chat_type\":\"channel\",\"chat_id\":\"" +
                    JsonParser::escapeJson(resolved_channel_id) + "\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"pinned_by\":\"" + JsonParser::escapeJson(user_id) + "\"}";
                if (is_v2) {
                    auto subs = db_manager_.getChannelSubscriberIds(resolved_channel_id);
                    for (const auto& uid : subs) {
                        if (!uid.empty()) ws_sender_(uid, payload);
                    }
                } else {
                    auto members = db_manager_.getChannelMembers(resolved_channel_id);
                for (const auto& m : members) {
                        if (!m.user_id.empty()) ws_sender_(m.user_id, payload);
                    }
                }
            }
            return JsonParser::createSuccessResponse("Message pinned");
        }
        return JsonParser::createErrorResponse("Failed to pin message");
    }

    return JsonParser::createErrorResponse("group_id or channel_id required");
}

std::string RequestHandler::handleUnpinMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string group_id = data.find("group_id") != data.end() ? data["group_id"] : "";
    std::string channel_id = data.find("channel_id") != data.end() ? data["channel_id"] : "";

    if (token.empty() || message_id.empty()) {
        return JsonParser::createErrorResponse("Token and message_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // Legacy direct chat (messages table)
    if (group_id.empty() && channel_id.empty()) {
        auto msg = db_manager_.getMessageById(message_id);
        if (msg.id.empty()) {
            return JsonParser::createErrorResponse("Message not found");
        }
        if (msg.sender_id != user_id && msg.receiver_id != user_id) {
            return JsonParser::createErrorResponse("Permission denied");
        }
        std::string peer_id = (msg.sender_id == user_id) ? msg.receiver_id : msg.sender_id;
        if (db_manager_.unpinDirectMessage(user_id, peer_id)) {
            if (ws_sender_) {
                std::string payload = "{\"type\":\"message_unpinned\",\"chat_type\":\"chat\",\"peer_id\":\"" +
                    JsonParser::escapeJson(peer_id) + "\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"unpinned_by\":\"" + JsonParser::escapeJson(user_id) + "\"}";
                ws_sender_(user_id, payload);
                if (!peer_id.empty() && peer_id != user_id) {
                    ws_sender_(peer_id, payload);
                }
            }
            return JsonParser::createSuccessResponse("Message unpinned");
        }
        return JsonParser::createErrorResponse("Failed to unpin message");
    }

    if (!group_id.empty()) {
        auto member = db_manager_.getGroupMember(group_id, user_id);
        if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
            return JsonParser::createErrorResponse("Only admins can unpin messages");
        }

        if (db_manager_.unpinGroupMessage(group_id, message_id)) {
            if (ws_sender_) {
                auto members = db_manager_.getGroupMembers(group_id);
                std::string payload = "{\"type\":\"message_unpinned\",\"chat_type\":\"group\",\"chat_id\":\"" +
                    JsonParser::escapeJson(group_id) + "\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"unpinned_by\":\"" + JsonParser::escapeJson(user_id) + "\"}";
                for (const auto& m : members) {
                    if (!m.user_id.empty()) {
                        ws_sender_(m.user_id, payload);
                    }
                }
            }
            return JsonParser::createSuccessResponse("Message unpinned");
        }
        return JsonParser::createErrorResponse("Failed to unpin message");
    }

    if (!channel_id.empty()) {
        DatabaseManager::ChannelMember member;
        std::string error_message;
        if (!checkChannelPermission(user_id, channel_id, ChannelPermission::PinMessage, &member, &error_message)) {
            return JsonParser::createErrorResponse(error_message.empty() ? "Permission denied" : error_message);
        }

        Chat chat;
        std::string resolved_channel_id = channel_id;
        bool is_v2 = resolveChannelV2(db_manager_, channel_id, &chat, &resolved_channel_id);
        bool unpinned = false;
        if (is_v2) {
            unpinned = db_manager_.unpinChatMessageV2(resolved_channel_id);
        } else {
            unpinned = db_manager_.unpinChannelMessage(resolved_channel_id, message_id);
        }
        if (unpinned) {
            if (ws_sender_) {
                std::string payload = "{\"type\":\"message_unpinned\",\"chat_type\":\"channel\",\"chat_id\":\"" +
                    JsonParser::escapeJson(resolved_channel_id) + "\",\"message_id\":\"" + JsonParser::escapeJson(message_id) +
                    "\",\"unpinned_by\":\"" + JsonParser::escapeJson(user_id) + "\"}";
                if (is_v2) {
                    auto subs = db_manager_.getChannelSubscriberIds(resolved_channel_id);
                    for (const auto& uid : subs) {
                        if (!uid.empty()) ws_sender_(uid, payload);
                    }
                } else {
                    auto members = db_manager_.getChannelMembers(resolved_channel_id);
                for (const auto& m : members) {
                        if (!m.user_id.empty()) ws_sender_(m.user_id, payload);
                    }
                }
            }
            return JsonParser::createSuccessResponse("Message unpinned");
        }
        return JsonParser::createErrorResponse("Failed to unpin message");
    }

    return JsonParser::createErrorResponse("group_id or channel_id required");
}

std::string RequestHandler::handleBanMember(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string target_user_id = data["target_user_id"];
    bool banned = true;
    if (data.find("banned") != data.end()) {
        banned = (data["banned"] == "true" || data["banned"] == "1");
    }
    std::string until = data.find("until") != data.end() ? data["until"] : "";
    
    if (token.empty() || group_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and target_user_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем права администратора
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
        return JsonParser::createErrorResponse("Only admins can ban members");
    }
    
    // Нельзя забанить создателя
    auto target_member = db_manager_.getGroupMember(group_id, target_user_id);
    if (target_member.role == "creator") {
        return JsonParser::createErrorResponse("Cannot ban group creator");
    }
    
    if (db_manager_.banGroupMember(group_id, target_user_id, banned, until)) {
        return JsonParser::createSuccessResponse(banned ? "Member banned" : "Member unbanned");
    }
    
    return JsonParser::createErrorResponse("Failed to ban/unban member");
}

std::string RequestHandler::handleMuteMember(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string target_user_id = data["target_user_id"];
    bool muted = true;
    if (data.find("muted") != data.end()) {
        muted = (data["muted"] == "true" || data["muted"] == "1");
    }
    
    if (token.empty() || group_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and target_user_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем права администратора
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
        return JsonParser::createErrorResponse("Only admins can mute members");
    }
    
    // Нельзя замутить создателя
    auto target_member = db_manager_.getGroupMember(group_id, target_user_id);
    if (target_member.role == "creator") {
        return JsonParser::createErrorResponse("Cannot mute group creator");
    }
    
    if (db_manager_.muteGroupMember(group_id, target_user_id, muted)) {
        return JsonParser::createSuccessResponse(muted ? "Member muted" : "Member unmuted");
    }
    
    return JsonParser::createErrorResponse("Failed to mute/unmute member");
}

std::string RequestHandler::handleForwardMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string message_id = data["message_id"];
    std::string forward_to_user_id = data.find("forward_to_user_id") != data.end() ? data["forward_to_user_id"] : "";
    std::string forward_to_group_id = data.find("forward_to_group_id") != data.end() ? data["forward_to_group_id"] : "";
    bool forward_as_self = true;
    if (data.find("forward_as_self") != data.end()) {
        forward_as_self = (data["forward_as_self"] == "true" || data["forward_as_self"] == "1");
    }
    
    if (token.empty() || group_id.empty() || message_id.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and message_id required");
    }
    
    if (forward_to_user_id.empty() && forward_to_group_id.empty()) {
        return JsonParser::createErrorResponse("forward_to_user_id or forward_to_group_id required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Получаем исходное сообщение из группы
    auto messages = db_manager_.getGroupMessages(group_id, 1000);
    DatabaseManager::GroupMessage original_message;
    for (const auto& msg : messages) {
        if (msg.id == message_id) {
            original_message = msg;
            break;
        }
    }
    
    if (original_message.id.empty()) {
        return JsonParser::createErrorResponse("Message not found");
    }
    
    // Определяем отправителя пересылки
    std::string forwarded_from_user_id = forward_as_self ? sender_id : original_message.sender_id;
    std::string forwarded_from_username = forward_as_self ? "" : original_message.sender_username;
    
    // Формируем текст пересылки
    std::string forward_prefix = forward_as_self ? 
        ("Переслано от " + original_message.sender_username) : 
        ("Переслано " + original_message.sender_username);
    std::string forward_content = forward_prefix + ": " + original_message.content;
    
    // Отправляем в личный чат
    if (!forward_to_user_id.empty()) {
        if (db_manager_.sendMessage(sender_id, forward_to_user_id, forward_content,
                                    original_message.message_type, original_message.file_path, 
                                    original_message.file_name, original_message.file_size, "",
                                    forwarded_from_user_id, forwarded_from_username, original_message.id)) {
            return JsonParser::createSuccessResponse("Message forwarded");
        }
    }
    
    // Отправляем в другую группу
    if (!forward_to_group_id.empty()) {
        // Проверяем, что пользователь является участником целевой группы
        auto member = db_manager_.getGroupMember(forward_to_group_id, sender_id);
        if (member.id.empty()) {
            return JsonParser::createErrorResponse("You are not a member of the target group");
        }
        
        if (db_manager_.sendGroupMessage(forward_to_group_id, sender_id, forward_content,
                                        original_message.message_type, original_message.file_path,
                                        original_message.file_name, original_message.file_size, "",
                                        forwarded_from_user_id, forwarded_from_username, message_id)) {
            return JsonParser::createSuccessResponse("Message forwarded");
        }
    }
    
    return JsonParser::createErrorResponse("Failed to forward message");
}

// Channel handlers implementation
std::string RequestHandler::handleCreateChannel(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string name = data.find("name") != data.end() ? data["name"] : "";
    std::string description = data.find("description") != data.end() ? data["description"] : "";
    std::string custom_link = data.find("custom_link") != data.end() ? data["custom_link"] : "";
    
    if (token.empty() || name.empty()) {
        return JsonParser::createErrorResponse("Token and name required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    if (name.length() < 3 || name.length() > 255) {
        return JsonParser::createErrorResponse("Channel name must be between 3 and 255 characters");
    }

    User user = db_manager_.getUserById(user_id);
    const int channel_limit = user.is_premium ? 1000 : 250;
    const int public_link_limit = user.is_premium ? 20 : 5;
    const int total_channels = db_manager_.countUserChannelsV2(user_id) + db_manager_.countUserChannelsLegacy(user_id);
    if (total_channels >= channel_limit) {
        return JsonParser::createErrorResponse("Channel limit reached");
    }
    
    // Валидация custom_link
    if (!custom_link.empty()) {
        // Убираем @ если есть
        if (custom_link[0] == '@') {
            custom_link = custom_link.substr(1);
        }
        
        // Проверяем формат (только буквы, цифры, подчеркивания, длина 3-50)
        if (custom_link.length() < 3 || custom_link.length() > 50) {
            return JsonParser::createErrorResponse("Custom link must be between 3 and 50 characters");
        }
        
        for (char c : custom_link) {
            if (!std::isalnum(c) && c != '_') {
                return JsonParser::createErrorResponse("Custom link can only contain letters, numbers and underscores");
            }
        }
        
        // Проверяем уникальность
        if (db_manager_.checkChannelCustomLinkExists(custom_link)) {
            return JsonParser::createErrorResponse("This username is already taken");
        }

        const int current_public_links = db_manager_.countUserPublicLinksV2(user_id)
            + db_manager_.countUserPublicLinksLegacy(user_id);
        if (current_public_links >= public_link_limit) {
            return JsonParser::createErrorResponse("Public link limit reached");
        }
    }
    
    std::string channel_id;
    // Prefer unified chats v2
    if (db_manager_.createChannelV2(name, user_id, channel_id, true, custom_link, description, 0, true)) {
        Logger::getInstance().info("Channel (v2) created: " + channel_id + " by user: " + user_id);
        std::map<std::string, std::string> response_data;
        response_data["channel_id"] = channel_id;
        return JsonParser::createSuccessResponse("Channel created successfully", response_data);
    }
    // Fallback legacy
    if (db_manager_.createChannel(name, description, user_id, channel_id, custom_link)) {
        Logger::getInstance().info("Channel (legacy) created: " + channel_id + " by user: " + user_id);
        std::map<std::string, std::string> response_data;
        response_data["channel_id"] = channel_id;
        return JsonParser::createSuccessResponse("Channel created successfully", response_data);
    }
    
    Logger::getInstance().error("Failed to create channel for user: " + user_id);
    return JsonParser::createErrorResponse("Failed to create channel. Check database connection and schema.");
}

std::string RequestHandler::handleGetChannels(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Merge v2 unified channels + legacy channels to keep UI compatible
    std::vector<DatabaseManager::Channel> channels;
    auto channels_v2 = db_manager_.getUserChannelsV2(user_id);
    auto channels_legacy = db_manager_.getUserChannels(user_id);
    channels.reserve(channels_v2.size() + channels_legacy.size());
    std::unordered_set<std::string> seen;
    for (const auto& c : channels_v2) {
        seen.insert(c.id);
        channels.push_back(c);
    }
    for (const auto& c : channels_legacy) {
        if (seen.find(c.id) != seen.end()) continue;
        seen.insert(c.id);
        channels.push_back(c);
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"channels\":[";
    
    bool first = true;
    for (const auto& channel : channels) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << JsonParser::escapeJson(channel.id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(channel.name) << "\","
            << "\"description\":\"" << JsonParser::escapeJson(channel.description) << "\","
            << "\"custom_link\":\"" << JsonParser::escapeJson(channel.custom_link) << "\","
            << "\"avatar_url\":\"" << JsonParser::escapeJson(channel.avatar_url) << "\","
            << "\"is_private\":" << (channel.is_private ? "true" : "false") << ","
            << "\"is_verified\":" << (channel.is_verified ? "true" : "false") << ","
            << "\"show_author\":" << (channel.show_author ? "true" : "false") << ","
            << "\"created_at\":\"" << JsonParser::escapeJson(channel.created_at) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handlePublicDirectory(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data.count("token") ? data["token"] : "";
    std::string category = data.count("category") ? data["category"] : "all";
    std::string search = data.count("search") ? data["search"] : "";
    int limit = 50;
    
    if (data.count("limit")) {
        try {
            limit = std::stoi(data["limit"]);
            if (limit < 1 || limit > 100) limit = 50;
        } catch (...) {
            limit = 50;
        }
    }
    
    std::string user_id;
    if (!token.empty()) {
        user_id = auth_manager_.getUserIdFromToken(token);
    }
    
    // Get public groups and channels
    std::vector<std::map<std::string, std::string>> items;
    
    // Get public groups
    auto groups = db_manager_.getPublicGroups(category, search, limit);
    for (const auto& g : groups) {
        std::map<std::string, std::string> item;
        item["id"] = g.id;
        item["type"] = "group";
        item["name"] = g.name;
        item["description"] = g.description;
        item["members_count"] = std::to_string(db_manager_.getGroupMembersCount(g.id));
        item["category"] = g.category;
        item["is_member"] = (!user_id.empty() && db_manager_.isGroupMember(g.id, user_id)) ? "true" : "false";
        items.push_back(item);
    }
    
    // Get public channels
    auto channels = db_manager_.getPublicChannels(category, search, limit);
    for (const auto& c : channels) {
        std::map<std::string, std::string> item;
        item["id"] = c.id;
        item["type"] = "channel";
        item["name"] = c.name;
        item["description"] = c.description;
        item["username"] = c.custom_link;
        item["members_count"] = std::to_string(db_manager_.getChannelSubscribersCount(c.id));
        item["category"] = c.category;
        item["avatar_url"] = c.avatar_url;
        item["verified"] = c.is_verified ? "true" : "false";
        item["is_member"] = (!user_id.empty() && db_manager_.isChannelSubscriber(c.id, user_id)) ? "true" : "false";
        items.push_back(item);
    }
    
    // Sort by members count (most popular first)
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        int count_a = 0, count_b = 0;
        try {
            if (a.count("members_count")) count_a = std::stoi(a.at("members_count"));
            if (b.count("members_count")) count_b = std::stoi(b.at("members_count"));
        } catch (...) {}
        return count_a > count_b;
    });
    
    // Limit results
    if ((int)items.size() > limit) {
        items.resize(limit);
    }
    
    // Build response
    std::ostringstream oss;
    oss << "{\"success\":true,\"items\":[";
    
    bool first = true;
    for (const auto& item : items) {
        if (!first) oss << ",";
        first = false;
        oss << "{";
        bool inner_first = true;
        for (const auto& kv : item) {
            if (!inner_first) oss << ",";
            inner_first = false;
            if (kv.first == "verified" || kv.first == "is_member") {
                oss << "\"" << kv.first << "\":" << kv.second;
            } else if (kv.first == "members_count") {
                oss << "\"" << kv.first << "\":" << kv.second;
            } else {
                oss << "\"" << kv.first << "\":\"" << JsonParser::escapeJson(kv.second) << "\"";
            }
        }
        oss << "}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSetGroupPublic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data.count("token") ? data["token"] : "";
    std::string group_id = data.count("group_id") ? data["group_id"] : "";
    bool is_public = data.count("is_public") && (data["is_public"] == "true" || data["is_public"] == "1");
    std::string category = data.count("category") ? data["category"] : "";
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid session");
    }
    
    // Check if user is group admin/creator
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.user_id.empty() || (member.role != "admin" && member.role != "creator")) {
        return JsonParser::createErrorResponse("Only admins can change group visibility");
    }
    
    // Update group public status and category
    if (!db_manager_.setGroupPublic(group_id, is_public, category)) {
        return JsonParser::createErrorResponse("Failed to update group");
    }
    
    return JsonParser::createSuccessResponse("Group visibility updated");
}

std::string RequestHandler::handleSetChannelPublic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data.count("token") ? data["token"] : "";
    std::string channel_id = data.count("channel_id") ? data["channel_id"] : "";
    bool is_public = data.count("is_public") && (data["is_public"] == "true" || data["is_public"] == "1");
    std::string category = data.count("category") ? data["category"] : "";
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid session");
    }
    
    // Check if user is channel admin/creator
    auto channel = db_manager_.getChannelById(channel_id);
    if (channel.id.empty() || channel.creator_id != user_id) {
        // Check admin status
        auto members = db_manager_.getChannelMembers(channel_id);
        bool is_admin = false;
        for (const auto& m : members) {
            if (m.user_id == user_id && (m.role == "admin" || m.role == "creator")) {
                is_admin = true;
                break;
            }
        }
        if (!is_admin) {
            return JsonParser::createErrorResponse("Only admins can change channel visibility");
        }
    }
    
    // Update channel: set is_private to opposite of is_public
    if (!db_manager_.setChannelPublicDirectory(channel_id, is_public, category)) {
        return JsonParser::createErrorResponse("Failed to update channel");
    }
    
    return JsonParser::createSuccessResponse("Channel visibility updated");
}

std::string RequestHandler::handleGetChannelMessages(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    int limit = 50;
    long long offset_local = 0;
    if (data.find("limit") != data.end()) {
        try {
            limit = std::stoi(data["limit"]);
            if (limit < 1 || limit > 100) limit = 50;
        } catch (...) {
            limit = 50;
        }
    }
    if (data.find("offset_id") != data.end()) {
        try {
            offset_local = std::stoll(data["offset_id"]);
            if (offset_local < 0) offset_local = 0;
        } catch (...) {
            offset_local = 0;
        }
    }
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // v2 chats: allow owner/admin/poster (support both UUID and @username)
    Chat chat;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        ChatPermissions perms = db_manager_.getChatPermissions(channel_id, user_id);
        if (!perms.found || perms.status == "kicked" || perms.status == "left") {
            return JsonParser::createErrorResponse("You are not subscribed to this channel or are banned");
        }
        auto messages = db_manager_.getChannelMessagesV2(channel_id, offset_local, limit);
        std::ostringstream oss;
        oss << "{\"success\":true,\"messages\":[";
        bool first = true;
        for (const auto& msg : messages) {
            if (!first) oss << ",";
            first = false;
            std::string time = msg.created_at.length() >= 16 ? msg.created_at.substr(11, 5) : "";
            oss << "{\"id\":\"" << JsonParser::escapeJson(msg.id) << "\","
                << "\"local_id\":" << msg.local_id << ","
                << "\"channel_id\":\"" << JsonParser::escapeJson(msg.channel_id) << "\","
                << "\"sender_id\":\"" << JsonParser::escapeJson(msg.sender_id) << "\","
                << "\"sender_username\":\"" << JsonParser::escapeJson(msg.sender_username) << "\","
                << "\"content\":\"" << JsonParser::escapeJson(msg.content) << "\","
                << "\"message_type\":\"" << JsonParser::escapeJson(msg.message_type) << "\","
                << "\"file_path\":\"" << JsonParser::escapeJson(msg.file_path) << "\","
                << "\"file_name\":\"" << JsonParser::escapeJson(msg.file_name) << "\","
                << "\"file_size\":" << msg.file_size << ","
                << "\"is_pinned\":" << (msg.is_pinned ? "true" : "false") << ","
                << "\"views_count\":" << msg.views_count << ","
                << "\"is_silent\":" << (msg.is_silent ? "true" : "false") << ","
                << "\"time\":\"" << time << "\"}";
        }
        oss << "]}";
        return oss.str();
    }

    // Legacy channel tables
    auto member = db_manager_.getChannelMember(channel_id, user_id);
    if (member.id.empty() || member.is_banned) {
        return JsonParser::createErrorResponse("You are not subscribed to this channel or are banned");
    }
    
    auto messages = db_manager_.getChannelMessages(channel_id, limit);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"messages\":[";
    
    bool first = true;
    for (const auto& msg : messages) {
        if (!first) oss << ",";
        first = false;
        
        std::string time = msg.created_at.length() >= 16 ? msg.created_at.substr(11, 5) : "";
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(msg.id) << "\","
            << "\"channel_id\":\"" << JsonParser::escapeJson(msg.channel_id) << "\","
            << "\"sender_id\":\"" << JsonParser::escapeJson(msg.sender_id) << "\","
            << "\"sender_username\":\"" << JsonParser::escapeJson(msg.sender_username) << "\","
            << "\"content\":\"" << JsonParser::escapeJson(msg.content) << "\","
            << "\"message_type\":\"" << JsonParser::escapeJson(msg.message_type) << "\","
            << "\"file_path\":\"" << JsonParser::escapeJson(msg.file_path) << "\","
            << "\"file_name\":\"" << JsonParser::escapeJson(msg.file_name) << "\","
            << "\"file_size\":" << msg.file_size << ","
            << "\"is_pinned\":" << (msg.is_pinned ? "true" : "false") << ","
            << "\"views_count\":" << msg.views_count << ","
            << "\"time\":\"" << time << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSendChannelMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string content = data.find("content") != data.end() ? data["content"] : "";
    std::string message_type = data.find("message_type") != data.end() ? data["message_type"] : "text";
    std::string file_path = data.find("file_path") != data.end() ? data["file_path"] : "";
    std::string file_name = data.find("file_name") != data.end() ? data["file_name"] : "";
    bool is_silent = (data.find("is_silent") != data.end() && (data["is_silent"] == "true" || data["is_silent"] == "1"));
    long long file_size = 0;
    if (data.find("file_size") != data.end()) {
        try {
            file_size = std::stoll(data["file_size"]);
        } catch (...) {
            file_size = 0;
        }
    }
    
    if (token.empty() || channel_id.empty() || content.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and content required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // Try new unified chats pipeline first (support both UUID and @username)
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        // Permission bitmask
        std::string perm_error;
        if (checkPermission(sender_id, channel_id, kPermCanPostMessages, &perm_error)) {

            // Restrictions
            bool want_media = (message_type != "text");
            bool want_links = (content.find("http://") != std::string::npos || content.find("https://") != std::string::npos);
            if (!checkRestriction(channel_id, sender_id, want_media, want_links, false, &perm_error)) {
                return JsonParser::createErrorResponse(perm_error);
            }

            // Slow mode (1 msg per window by design)
            if (!enforceSlowMode(channel_id, sender_id, chat_v2.slow_mode_sec, 1, &perm_error)) {
                return JsonParser::createErrorResponse(perm_error);
            }

            // Build JSON payload
            std::string content_json = "{\"text\":\"" + JsonParser::escapeJson(content) + "\"}";
            std::string channel_msg_id;
            std::string discussion_msg_id;
            bool ok = db_manager_.sendChannelMessageWithDiscussion(
                channel_id,
                sender_id,
                content_json,
                message_type,
                "",
                is_silent,
                &channel_msg_id,
                &discussion_msg_id
            );
            if (!ok) {
                return JsonParser::createErrorResponse("Failed to send message");
            }

            if (ws_sender_) {
                std::string payload = "{\"type\":\"channel_new_message\",\"chat_id\":\"" +
                    JsonParser::escapeJson(channel_id) + "\",\"message_id\":\"" +
                    JsonParser::escapeJson(channel_msg_id) + "\"}";
                auto subscribers = db_manager_.getChannelSubscriberIds(channel_id);
                for (const auto& uid : subscribers) {
                    ws_sender_(uid, payload);
                }
            }

            if (!is_silent) {
                bool fcm_ready = fcm_client_.isReady();
                bool rustore_ready = rustore_client_.isReady();
                if (!fcm_ready && !rustore_ready) {
                    Logger::getInstance().warning("Push skipped: no providers ready for channel " + channel_id);
                } else {
                    if (!fcm_ready) {
                        Logger::getInstance().warning("Push skipped: FCM not ready for channel " + channel_id);
                    }
                    if (!rustore_ready) {
                        Logger::getInstance().warning("Push skipped: RuStore not ready for channel " + channel_id);
                    }
                    try {
                        std::string channel_title = !chat_v2.title.empty() ? chat_v2.title : "Channel";
                        std::string body_text = content;
                        if (message_type != "text") {
                            if (message_type == "voice") {
                                body_text = "Voice message";
                            } else if (message_type == "file") {
                                body_text = file_name.empty() ? "File" : ("File: " + file_name);
                            } else if (message_type == "image") {
                                body_text = "Photo";
                            } else {
                                body_text = "Message";
                            }
                        }
                        if (body_text.size() > 120) {
                            body_text = body_text.substr(0, 117) + "...";
                        }

                        std::map<std::string, std::string> push_payload;
                        push_payload["type"] = "channel_message";
                        push_payload["chat_type"] = "channel";
                        push_payload["chat_id"] = channel_id;
                        push_payload["chat_title"] = channel_title;
                        push_payload["sender_id"] = sender_id;
                        push_payload["message_id"] = channel_msg_id;
                        push_payload["message_type"] = message_type;
                        push_payload["title"] = channel_title;
                        push_payload["body"] = body_text;

                        auto subscribers = db_manager_.getChannelSubscriberIds(channel_id);
                        for (const auto& uid : subscribers) {
                            if (uid.empty() || uid == sender_id) continue;
                            auto tokens = db_manager_.getPushTokensForUser(uid);
                            if (tokens.empty()) {
                                Logger::getInstance().warning("Push skipped: no tokens for channel subscriber " + uid);
                                continue;
                            }
                            sendPushTokensForUser(db_manager_, fcm_client_, rustore_client_, uid, tokens,
                                                  channel_title, body_text, push_payload,
                                                  "channel_messages", "HIGH", fcm_ready, rustore_ready);
                        }
                    } catch (...) {
                        Logger::getInstance().warning("Failed to send channel message notification");
                    }
                }
            }

            std::map<std::string, std::string> resp;
            resp["message_id"] = channel_msg_id;
            if (!discussion_msg_id.empty()) {
                resp["discussion_message_id"] = discussion_msg_id;
            }
            return JsonParser::createSuccessResponse("Message sent", resp);
        } else {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Insufficient channel permissions" : perm_error);
        }
    }

    // Legacy path fallback
    std::string permission_error;
    if (!checkChannelPermission(sender_id, channel_id, ChannelPermission::PostMessage, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }
    
    if (db_manager_.sendChannelMessage(channel_id, sender_id, content, message_type, file_path, file_name, file_size)) {
        if (!is_silent) {
            bool fcm_ready = fcm_client_.isReady();
            bool rustore_ready = rustore_client_.isReady();
            if (!fcm_ready && !rustore_ready) {
                Logger::getInstance().warning("Push skipped: no providers ready for channel " + channel_id);
            } else {
                if (!fcm_ready) {
                    Logger::getInstance().warning("Push skipped: FCM not ready for channel " + channel_id);
                }
                if (!rustore_ready) {
                    Logger::getInstance().warning("Push skipped: RuStore not ready for channel " + channel_id);
                }
                try {
                    auto channel = db_manager_.getChannelById(channel_id);
                    std::string channel_title = !channel.name.empty() ? channel.name : "Channel";
                    std::string body_text = content;
                    if (message_type != "text") {
                        if (message_type == "voice") {
                            body_text = "Voice message";
                        } else if (message_type == "file") {
                            body_text = file_name.empty() ? "File" : ("File: " + file_name);
                        } else if (message_type == "image") {
                            body_text = "Photo";
                        } else {
                            body_text = "Message";
                        }
                    }
                    if (body_text.size() > 120) {
                        body_text = body_text.substr(0, 117) + "...";
                    }

                    std::map<std::string, std::string> push_payload;
                    push_payload["type"] = "channel_message";
                    push_payload["chat_type"] = "channel";
                    push_payload["chat_id"] = channel_id;
                    push_payload["chat_title"] = channel_title;
                    push_payload["sender_id"] = sender_id;
                    push_payload["message_type"] = message_type;
                    push_payload["title"] = channel_title;
                    push_payload["body"] = body_text;

                    auto members = db_manager_.getChannelMembers(channel_id);
                    for (const auto& m : members) {
                        if (m.user_id.empty() || m.user_id == sender_id || m.is_banned) continue;
                        auto tokens = db_manager_.getPushTokensForUser(m.user_id);
                        if (tokens.empty()) {
                            Logger::getInstance().warning("Push skipped: no tokens for channel member " + m.user_id);
                            continue;
                        }
                        sendPushTokensForUser(db_manager_, fcm_client_, rustore_client_, m.user_id, tokens,
                                              channel_title, body_text, push_payload,
                                              "channel_messages", "HIGH", fcm_ready, rustore_ready);
                    }
                } catch (...) {
                    Logger::getInstance().warning("Failed to send legacy channel message notification");
                }
            }
        }
        return JsonParser::createSuccessResponse("Message sent");
    }
    
    return JsonParser::createErrorResponse("Failed to send message");
}

std::string RequestHandler::handleReadChannel(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    long long max_read_local = 0;
    if (data.find("max_read_local") != data.end()) {
        try {
            max_read_local = std::stoll(data["max_read_local"]);
            if (max_read_local < 0) max_read_local = 0;
        } catch (...) {
            max_read_local = 0;
        }
    }

    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // Check membership (v2; support both UUID and @username)
    Chat chat;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        ChatPermissions perms = db_manager_.getChatPermissions(channel_id, user_id);
        if (!perms.found || perms.status == "kicked" || perms.status == "left") {
            return JsonParser::createErrorResponse("You are not subscribed to this channel or are banned");
        }
    } else {
        auto member = db_manager_.getChannelMember(channel_id, user_id);
        if (member.id.empty() || member.is_banned) {
            return JsonParser::createErrorResponse("You are not subscribed to this channel or are banned");
        }
    }

    if (!db_manager_.upsertChannelReadState(channel_id, user_id, max_read_local)) {
        return JsonParser::createErrorResponse("Failed to update read state");
    }

    long long unread = db_manager_.getChannelUnreadCount(channel_id, user_id);
    std::map<std::string, std::string> resp;
    resp["unread"] = std::to_string(unread);
    return JsonParser::createSuccessResponse("Read state updated", resp);
}

std::string RequestHandler::handleSubscribeChannel(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channels: join as member for public channels (support UUID and @username)
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        ChatPermissions perms = db_manager_.getChatPermissions(channel_id, user_id);
        if (perms.found) {
            if (perms.status == "kicked") {
                return JsonParser::createErrorResponse("You are banned in this channel");
            }
            if (perms.status != "left") {
                return JsonParser::createSuccessResponse("Already subscribed");
            }
        }
        if (!chat_v2.is_public) {
            if (db_manager_.createChatJoinRequest(channel_id, user_id)) {
                return JsonParser::createSuccessResponse("Join request sent");
            }
            return JsonParser::createErrorResponse("Failed to send join request");
        }
        if (db_manager_.upsertChatMemberV2(channel_id, user_id)) {
            return JsonParser::createSuccessResponse("Subscribed to channel");
        }
        return JsonParser::createErrorResponse("Failed to subscribe to channel");
    }
    
    // Для приватных каналов нужно создать заявку
    auto channel = db_manager_.getChannelById(channel_id);
    if (channel.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        channel = db_manager_.getChannelByCustomLink(usernameKey);
        if (!channel.id.empty()) {
            channel_id = channel.id;
        }
    }
    auto existing_member = db_manager_.getChannelMember(channel_id, user_id);
    if (!existing_member.id.empty()) {
        if (existing_member.is_banned) {
            return JsonParser::createErrorResponse("You are banned in this channel");
        }
        return JsonParser::createSuccessResponse("Already subscribed");
    }
    if (channel.is_private) {
        if (db_manager_.createChannelJoinRequest(channel_id, user_id)) {
            return JsonParser::createSuccessResponse("Join request sent");
        }
        return JsonParser::createErrorResponse("Failed to send join request");
    }
    
    // Для публичных каналов сразу добавляем
    if (db_manager_.addChannelMember(channel_id, user_id, "subscriber")) {
        return JsonParser::createSuccessResponse("Subscribed to channel");
    }
    
    return JsonParser::createErrorResponse("Failed to subscribe to channel");
}

std::string RequestHandler::handleCreateChannelInvite(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    int expire_seconds = 0;
    int usage_limit = 0;
    if (data.find("expire_seconds") != data.end()) {
        try { expire_seconds = std::stoi(data["expire_seconds"]); } catch (...) { expire_seconds = 0; }
    }
    if (data.find("usage_limit") != data.end()) {
        try { usage_limit = std::stoi(data["usage_limit"]); } catch (...) { usage_limit = 0; }
    }
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    // Normalize v2 channel_id (support UUID and @username)
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    bool is_v2 = resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id);
    if (is_v2) {
        channel_id = resolved_channel_id;
    }
    // permission to invite
    if (is_v2) {
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanInvite, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
    } else {
        auto legacy = db_manager_.getChannelById(channel_id);
        if (legacy.id.empty()) {
            std::string usernameKey = channel_id;
            if (!usernameKey.empty() && usernameKey[0] == '@') {
                usernameKey = usernameKey.substr(1);
            }
            legacy = db_manager_.getChannelByCustomLink(usernameKey);
            if (!legacy.id.empty()) {
                channel_id = legacy.id;
            }
        }
        std::string perm_error;
        if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageRequests, nullptr, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
    }
    std::string invite_token;
    if (!db_manager_.createChannelInviteLink(channel_id, user_id, expire_seconds, usage_limit, invite_token)) {
        return JsonParser::createErrorResponse("Failed to create invite link");
    }
    std::map<std::string, std::string> resp;
    resp["token"] = invite_token;
    return JsonParser::createSuccessResponse("Invite created", resp);
}

std::string RequestHandler::handleRevokeChannelInvite(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string invite_token = data["invite_token"];
    if (token.empty() || invite_token.empty()) {
        return JsonParser::createErrorResponse("Token and invite_token required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    std::string channel_id = db_manager_.getChannelIdByInviteToken(invite_token);
    if (channel_id.empty()) {
        return JsonParser::createErrorResponse("Invite not found");
    }
    Chat chat_v2 = db_manager_.getChatV2(channel_id);
    if (!chat_v2.id.empty() && chat_v2.type == "channel") {
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanInvite, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
    } else {
        std::string perm_error;
        if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageRequests, nullptr, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
    }
    if (!db_manager_.revokeChannelInviteLink(invite_token)) {
        return JsonParser::createErrorResponse("Failed to revoke invite");
    }
    return JsonParser::createSuccessResponse("Invite revoked");
}

std::string RequestHandler::handleJoinChannelByInvite(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string invite_token = data["invite_token"];
    std::string token = data["token"];
    if (invite_token.empty() || token.empty()) {
        return JsonParser::createErrorResponse("invite_token and token required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    std::string channel_id;
    bool is_v2_invite = false;
    if (!db_manager_.joinChannelByInviteToken(invite_token, user_id, channel_id, false, &is_v2_invite)) {
        return JsonParser::createErrorResponse("Invalid or expired invite");
    }
    bool is_private = false;
    bool is_v2 = false;
    Chat chat;
    if (is_v2_invite) {
        chat = db_manager_.getChatV2(channel_id);
        if (!chat.id.empty() && chat.type == "channel") {
            is_v2 = true;
            is_private = !chat.is_public;
        }
    }
    if (!is_v2) {
        auto legacy = db_manager_.getChannelById(channel_id);
        if (!legacy.id.empty()) {
            is_private = legacy.is_private;
        }
    }

    if (is_private) {
        bool requested = false;
        if (is_v2) {
            requested = db_manager_.createChatJoinRequest(channel_id, user_id);
        } else {
            requested = db_manager_.createChannelJoinRequest(channel_id, user_id);
        }
        if (!requested) {
            return JsonParser::createErrorResponse("Failed to send join request");
        }
        std::map<std::string, std::string> resp;
        resp["channel_id"] = channel_id;
        resp["requested"] = "true";
        return JsonParser::createSuccessResponse("Join request sent", resp);
    }

    bool joined = false;
    if (is_v2) {
        ChatPermissions perms = db_manager_.getChatPermissions(channel_id, user_id);
        if (perms.found) {
            if (perms.status == "kicked") {
                return JsonParser::createErrorResponse("You are banned in this channel");
            }
            if (perms.status != "left") {
                joined = true;
            }
        }
        if (!joined) {
            joined = db_manager_.upsertChatMemberV2(channel_id, user_id);
        }
    } else {
        auto member = db_manager_.getChannelMember(channel_id, user_id);
        if (!member.id.empty()) {
            if (member.is_banned) {
                return JsonParser::createErrorResponse("You are banned in this channel");
            }
            joined = true;
        } else {
            joined = db_manager_.addChannelMember(channel_id, user_id, "subscriber");
        }
    }
    if (!joined) {
        return JsonParser::createErrorResponse("Failed to join channel");
    }
    std::map<std::string, std::string> resp;
    resp["channel_id"] = channel_id;
    return JsonParser::createSuccessResponse("Joined channel", resp);
}

std::string RequestHandler::handleAddMessageReaction(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string reaction = data["reaction"];
    
    if (token.empty() || message_id.empty() || reaction.empty()) {
        return JsonParser::createErrorResponse("Token, message_id and reaction required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // v2 message ids are BIGINT; legacy channel_messages ids are UUID.
    if (isAllDigits(message_id)) {
        std::string chat_id;
        std::string sender_id;
        if (!db_manager_.getChatMessageMetaV2(message_id, chat_id, sender_id)) {
            return JsonParser::createErrorResponse("Message not found");
        }
        auto allowed = db_manager_.getChatAllowedReactions(chat_id);
        if (!allowed.empty() && std::find(allowed.begin(), allowed.end(), reaction) == allowed.end()) {
            return JsonParser::createErrorResponse("Reaction not allowed");
        }
        if (db_manager_.addMessageReactionV2(message_id, user_id, reaction)) {
            return JsonParser::createSuccessResponse("Reaction added");
        }
        return JsonParser::createErrorResponse("Failed to add reaction");
    }

    if (db_manager_.addMessageReaction(message_id, user_id, reaction)) {
        return JsonParser::createSuccessResponse("Reaction added");
    }
    return JsonParser::createErrorResponse("Failed to add reaction");
}

std::string RequestHandler::handleRemoveMessageReaction(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string reaction = data["reaction"];
    
    if (token.empty() || message_id.empty() || reaction.empty()) {
        return JsonParser::createErrorResponse("Token, message_id and reaction required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // v2 message ids are BIGINT; legacy channel_messages ids are UUID.
    if (isAllDigits(message_id)) {
    if (db_manager_.removeMessageReactionV2(message_id, user_id, reaction)) {
        return JsonParser::createSuccessResponse("Reaction removed");
    }
        return JsonParser::createErrorResponse("Failed to remove reaction");
    }

    if (db_manager_.removeMessageReaction(message_id, user_id, reaction)) {
        return JsonParser::createSuccessResponse("Reaction removed");
    }
    return JsonParser::createErrorResponse("Failed to remove reaction");
}

std::string RequestHandler::handleGetMessageReactions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string message_id = data["message_id"];
    
    if (message_id.empty()) {
        return JsonParser::createErrorResponse("message_id required");
    }
    
    std::vector<ReactionCount> reactions;
    if (isAllDigits(message_id)) {
        reactions = db_manager_.getMessageReactionCountsV2(message_id);
    } else {
        auto legacy = db_manager_.getMessageReactions(message_id);
        for (const auto& lr : legacy) {
            ReactionCount rc;
            rc.reaction = lr.reaction;
            rc.count = lr.count;
            reactions.push_back(rc);
        }
    }
    std::ostringstream oss;
    oss << "{\"success\":true,\"reactions\":[";
    
    bool first = true;
    for (const auto& reaction : reactions) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"reaction\":\"" << JsonParser::escapeJson(reaction.reaction) << "\","
            << "\"count\":" << reaction.count << "}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleCreatePoll(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string message_type = data.find("message_type") != data.end() ? data["message_type"] : "chat";
    std::string question = data["question"];
    bool is_anonymous = data.find("is_anonymous") != data.end() ? (data["is_anonymous"] == "true" || data["is_anonymous"] == "1") : true;
    bool allows_multiple = data.find("allows_multiple") != data.end() ? (data["allows_multiple"] == "true" || data["allows_multiple"] == "1") : false;
    std::string closes_at = data.find("closes_at") != data.end() ? data["closes_at"] : "";
    std::vector<std::string> options;
    int idx = 0;
    while (true) {
        std::string key = "option" + std::to_string(idx);
        if (data.find(key) == data.end()) break;
        options.push_back(data[key]);
        idx++;
    }
    if (token.empty() || message_id.empty() || question.empty() || options.empty()) {
        return JsonParser::createErrorResponse("token, message_id, question, options required");
    }
    if (message_type != "chat" && message_type != "channel" && message_type != "group") {
        return JsonParser::createErrorResponse("message_type must be 'chat', 'channel', or 'group'");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Authorization checks based on message type
    if (message_type == "channel") {
        // For channels, we need channel_id - get it from message metadata
        std::string channel_id;
        std::string sender_id;
        if (!db_manager_.getChannelMessageMetaLegacy(message_id, channel_id, sender_id)) {
            return JsonParser::createErrorResponse("Message not found or not a channel message");
        }
        if (sender_id != user_id) {
            // Check if user has permission to post messages in channel
            std::string permission_error;
            if (!checkChannelPermission(user_id, channel_id, ChannelPermission::PostMessage, nullptr, &permission_error)) {
                return JsonParser::createErrorResponse(permission_error);
            }
        }
    } else if (message_type == "group") {
        // For groups, we need group_id - get it from message metadata
        std::string group_id;
        std::string sender_id;
        if (!db_manager_.getGroupMessageMeta(message_id, group_id, sender_id)) {
            return JsonParser::createErrorResponse("Message not found or not a group message");
        }
        if (sender_id != user_id) {
            // Check if user is a member of the group
            auto member = db_manager_.getGroupMember(group_id, user_id);
            if (member.id.empty() || member.is_banned) {
                return JsonParser::createErrorResponse("You are not a member of this group or are banned");
            }
        }
    }
    // For chat messages, no additional checks needed (already handled by message ownership)
    
    if (!db_manager_.createPollV2(message_id, message_type, question, is_anonymous, allows_multiple, options, closes_at)) {
        return JsonParser::createErrorResponse("Failed to create poll");
    }
    return JsonParser::createSuccessResponse("Poll created");
}

std::string RequestHandler::handleVotePoll(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string poll_id = data["poll_id"];
    std::string option_id = data["option_id"];
    if (token.empty() || poll_id.empty() || option_id.empty()) {
        return JsonParser::createErrorResponse("token, poll_id, option_id required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    if (!db_manager_.votePollV2(poll_id, option_id, user_id)) {
        return JsonParser::createErrorResponse("Failed to vote");
    }
    return JsonParser::createSuccessResponse("Vote recorded");
}

std::string RequestHandler::handleGetPoll(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string message_id = data["message_id"];
    std::string message_type = data.find("message_type") != data.end() ? data["message_type"] : "chat";
    
    if (token.empty() || message_id.empty()) {
        return JsonParser::createErrorResponse("token and message_id required");
    }
    if (message_type != "chat" && message_type != "channel" && message_type != "group") {
        return JsonParser::createErrorResponse("message_type must be 'chat', 'channel', or 'group'");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check permissions based on message type
    if (message_type == "channel") {
        std::string channel_id;
        std::string sender_id;
        if (!db_manager_.getChannelMessageMetaLegacy(message_id, channel_id, sender_id)) {
            return JsonParser::createErrorResponse("Message not found or not a channel message");
        }
        std::string permission_error;
        if (!checkChannelPermission(user_id, channel_id, ChannelPermission::PostMessage, nullptr, &permission_error)) {
            return JsonParser::createErrorResponse(permission_error);
        }
    } else if (message_type == "group") {
        std::string group_id;
        std::string sender_id;
        if (!db_manager_.getGroupMessageMeta(message_id, group_id, sender_id)) {
            return JsonParser::createErrorResponse("Message not found or not a group message");
        }
        auto member = db_manager_.getGroupMember(group_id, user_id);
        if (member.id.empty() || member.is_banned) {
            return JsonParser::createErrorResponse("You are not a member of this group or are banned");
        }
    }
    
    auto poll = db_manager_.getPollByMessage(message_type, message_id);
    if (poll.id.empty()) {
        return JsonParser::createErrorResponse("Poll not found");
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"poll\":{"
        << "\"id\":\"" << JsonParser::escapeJson(poll.id) << "\","
        << "\"message_id\":\"" << JsonParser::escapeJson(poll.message_id) << "\","
        << "\"message_type\":\"" << JsonParser::escapeJson(poll.message_type) << "\","
        << "\"question\":\"" << JsonParser::escapeJson(poll.question) << "\","
        << "\"is_anonymous\":" << (poll.is_anonymous ? "true" : "false") << ","
        << "\"allows_multiple\":" << (poll.allows_multiple ? "true" : "false") << ","
        << "\"closes_at\":\"" << JsonParser::escapeJson(poll.closes_at) << "\","
        << "\"options\":[";
    
    bool first = true;
    for (const auto& opt : poll.options) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << JsonParser::escapeJson(opt.id) << "\","
            << "\"option_text\":\"" << JsonParser::escapeJson(opt.option_text) << "\","
            << "\"vote_count\":" << opt.vote_count << "}";
    }
    
    oss << "]}}";
    return oss.str();
}

std::string RequestHandler::handleAddMessageView(const std::string& body) {
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
    
    if (db_manager_.addMessageView(message_id, user_id)) {
        int views_count = db_manager_.getMessageViewsCount(message_id);
        std::map<std::string, std::string> response_data;
        response_data["views_count"] = std::to_string(views_count);
        return JsonParser::createSuccessResponse("View added", response_data);
    }
    
    return JsonParser::createErrorResponse("Failed to add view");
}

std::string RequestHandler::handleSetChannelCustomLink(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string custom_link = data.find("custom_link") != data.end() ? data["custom_link"] : "";
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channel username update (support UUID and @username)
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }

        // If custom_link is empty -> clear username
        if (custom_link.empty()) {
            if (db_manager_.updateChatUsernameV2(channel_id, "")) {
                return JsonParser::createSuccessResponse("Custom link removed");
            }
            return JsonParser::createErrorResponse("Failed to remove custom link");
        }

        if (!custom_link.empty() && custom_link[0] == '@') {
            custom_link = custom_link.substr(1);
        }

        // chats.username is VARCHAR(32) in v2 schema; validate accordingly
        if (custom_link.length() < 3 || custom_link.length() > 32) {
            return JsonParser::createErrorResponse("Custom link must be between 3 and 32 characters");
        }
        for (char c : custom_link) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                return JsonParser::createErrorResponse("Custom link can only contain letters, numbers and underscores");
            }
        }

        const bool had_link = !chat_v2.username.empty();
        if (!had_link) {
            User user = db_manager_.getUserById(user_id);
            const int public_link_limit = user.is_premium ? 20 : 5;
            const int current_links = db_manager_.countUserPublicLinksV2(user_id)
                + db_manager_.countUserPublicLinksLegacy(user_id);
            if (current_links >= public_link_limit) {
                return JsonParser::createErrorResponse("Public link limit reached");
            }
        }

        Chat existing = db_manager_.getChatByUsernameV2(custom_link);
        if (!existing.id.empty() && existing.id != channel_id) {
            return JsonParser::createErrorResponse("This username is already taken");
        }

        if (db_manager_.updateChatUsernameV2(channel_id, custom_link)) {
            return JsonParser::createSuccessResponse("Custom link set");
        }
        return JsonParser::createErrorResponse("Failed to set custom link");
    }

    // Legacy channel tables: resolve by custom_link to real id (because channel_id may be '@username')
    auto legacy = db_manager_.getChannelById(channel_id);
    if (legacy.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        legacy = db_manager_.getChannelByCustomLink(usernameKey);
        if (!legacy.id.empty()) {
            channel_id = legacy.id;
        }
    }
    
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }
    
    // Если custom_link пустой, удаляем его (устанавливаем NULL)
    if (custom_link.empty()) {
        if (db_manager_.setChannelCustomLink(channel_id, "")) {
            return JsonParser::createSuccessResponse("Custom link removed");
        }
        return JsonParser::createErrorResponse("Failed to remove custom link");
    }
    
    // Убираем @ если есть
    if (!custom_link.empty() && custom_link[0] == '@') {
        custom_link = custom_link.substr(1);
    }
    
    // Валидация custom_link
    if (custom_link.length() < 3 || custom_link.length() > 50) {
        return JsonParser::createErrorResponse("Custom link must be between 3 and 50 characters");
    }
    
    // Проверяем формат (только буквы, цифры, подчеркивания)
    for (char c : custom_link) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return JsonParser::createErrorResponse("Custom link can only contain letters, numbers and underscores");
        }
    }
    
    // Проверяем уникальность (если не пустой)
    if (db_manager_.checkChannelCustomLinkExists(custom_link)) {
        // Проверяем, не принадлежит ли этот custom_link текущему каналу
        auto existingChannel = db_manager_.getChannelByCustomLink(custom_link);
        if (existingChannel.id != channel_id) {
            return JsonParser::createErrorResponse("This username is already taken");
        }
    }

    const bool had_link = !legacy.custom_link.empty();
    if (!had_link) {
        User user = db_manager_.getUserById(user_id);
        const int public_link_limit = user.is_premium ? 20 : 5;
        const int current_links = db_manager_.countUserPublicLinksV2(user_id)
            + db_manager_.countUserPublicLinksLegacy(user_id);
        if (current_links >= public_link_limit) {
            return JsonParser::createErrorResponse("Public link limit reached");
        }
    }
    
    if (db_manager_.setChannelCustomLink(channel_id, custom_link)) {
        return JsonParser::createSuccessResponse("Custom link set");
    }
    
    return JsonParser::createErrorResponse("Failed to set custom link");
}

std::string RequestHandler::handleSetChannelPrivacy(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    bool is_private = true;
    if (data.find("is_private") != data.end()) {
        is_private = (data["is_private"] == "true" || data["is_private"] == "1");
    }
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channel privacy update: is_private <-> !is_public (support UUID and @username)
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        bool is_public = !is_private;
        if (db_manager_.updateChatIsPublicV2(channel_id, is_public)) {
            return JsonParser::createSuccessResponse("Channel privacy updated");
        }
        return JsonParser::createErrorResponse("Failed to update channel privacy");
    }

    // Legacy path (resolve by custom_link if needed)
    auto legacy = db_manager_.getChannelById(channel_id);
    if (legacy.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        legacy = db_manager_.getChannelByCustomLink(usernameKey);
        if (!legacy.id.empty()) {
            channel_id = legacy.id;
        }
    }
    
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }
    
    if (db_manager_.setChannelPrivacy(channel_id, is_private)) {
        return JsonParser::createSuccessResponse("Channel privacy updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update channel privacy");
}

std::string RequestHandler::handleSetChannelShowAuthor(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    bool show_author = true;
    if (data.find("show_author") != data.end()) {
        show_author = (data["show_author"] == "true" || data["show_author"] == "1");
    }
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channels: chat.sign_messages=true means "hide author"; legacy show_author is the inverse
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        bool sign_messages = !show_author;
        if (db_manager_.updateChatSignMessagesV2(channel_id, sign_messages)) {
            return JsonParser::createSuccessResponse("Show author setting updated");
        }
        return JsonParser::createErrorResponse("Failed to update show author setting");
    }

    // Legacy path (resolve by custom_link if needed)
    auto legacy = db_manager_.getChannelById(channel_id);
    if (legacy.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        legacy = db_manager_.getChannelByCustomLink(usernameKey);
        if (!legacy.id.empty()) {
            channel_id = legacy.id;
        }
    }
    
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }
    
    if (db_manager_.setChannelShowAuthor(channel_id, show_author)) {
        return JsonParser::createSuccessResponse("Show author setting updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update show author setting");
}

std::string RequestHandler::handleUpdateChannelName(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string new_name = data["new_name"];
    
    if (token.empty() || channel_id.empty() || new_name.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and new_name required");
    }
    
    if (new_name.length() < 3 || new_name.length() > 255) {
        return JsonParser::createErrorResponse("Channel name must be between 3 and 255 characters");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // v2 path (support UUID and @username)
    Chat chat;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        if (db_manager_.updateChatTitleV2(channel_id, new_name)) {
            return JsonParser::createSuccessResponse("Channel name updated");
        }
        // fallthrough to legacy if update failed
    }

    // legacy path
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }
    
    if (db_manager_.updateChannelName(channel_id, new_name)) {
        return JsonParser::createSuccessResponse("Channel name updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update channel name");
}

std::string RequestHandler::handleUpdateChannelDescription(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string new_description = data.find("new_description") != data.end() ? data["new_description"] : "";
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // v2 path (support UUID and @username)
    Chat chat;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        if (db_manager_.updateChatAboutV2(channel_id, new_description)) {
            return JsonParser::createSuccessResponse("Channel description updated");
        }
        // fallthrough to legacy if update failed
    }

    // legacy path
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }
    
    if (db_manager_.updateChannelDescription(channel_id, new_description)) {
        return JsonParser::createSuccessResponse("Channel description updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update channel description");
}

std::string RequestHandler::handleAddAllowedReaction(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string reaction = data["reaction"];
    
    if (token.empty() || channel_id.empty() || reaction.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and reaction required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    bool is_v2 = resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id);
    if (is_v2) {
        channel_id = resolved_channel_id;
    }
    
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageReactions, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }

    if (is_v2) {
        if (db_manager_.addChatAllowedReaction(channel_id, reaction)) {
            return JsonParser::createSuccessResponse("Allowed reaction added");
        }
        return JsonParser::createErrorResponse("Failed to add allowed reaction");
    }

    if (db_manager_.addAllowedReaction(channel_id, reaction)) {
        return JsonParser::createSuccessResponse("Allowed reaction added");
    }
    
    return JsonParser::createErrorResponse("Failed to add allowed reaction");
}

std::string RequestHandler::handleRemoveAllowedReaction(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string reaction = data["reaction"];
    
    if (token.empty() || channel_id.empty() || reaction.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and reaction required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    bool is_v2 = resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id);
    if (is_v2) {
        channel_id = resolved_channel_id;
    }
    
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageReactions, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }

    if (is_v2) {
        if (db_manager_.removeChatAllowedReaction(channel_id, reaction)) {
            return JsonParser::createSuccessResponse("Allowed reaction removed");
        }
        return JsonParser::createErrorResponse("Failed to remove allowed reaction");
    }

    if (db_manager_.removeAllowedReaction(channel_id, reaction)) {
        return JsonParser::createSuccessResponse("Allowed reaction removed");
    }
    
    return JsonParser::createErrorResponse("Failed to remove allowed reaction");
}

std::string RequestHandler::handleGetChannelAllowedReactions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];

    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channels: allowed reactions list is not enforced yet; return empty list (means "all allowed")
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        ChatPermissions perms = db_manager_.getChatPermissions(channel_id, user_id);
        if (!perms.found || perms.status == "kicked" || perms.status == "left") {
            return JsonParser::createErrorResponse("You are not a member of this channel or are banned");
        }
        auto reactions = db_manager_.getChatAllowedReactions(channel_id);
        std::ostringstream oss;
        oss << "{\"success\":true,\"reactions\":[";
        for (size_t i = 0; i < reactions.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << JsonParser::escapeJson(reactions[i]) << "\"";
        }
        oss << "]}";
        return oss.str();
    }

    // Require at least channel membership (banned members denied)
    DatabaseManager::ChannelMember member = db_manager_.getChannelMember(channel_id, user_id);
    if (member.id.empty() || member.is_banned) {
        return JsonParser::createErrorResponse("You are not a member of this channel or are banned");
    }

    auto reactions = db_manager_.getAllowedReactions(channel_id);
    std::ostringstream oss;
    oss << "{\"success\":true,\"reactions\":[";
    for (size_t i = 0; i < reactions.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << JsonParser::escapeJson(reactions[i]) << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetChannelJoinRequests(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channels: join requests are not implemented yet; return empty list for admins/owners.
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanInvite, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        auto requests = db_manager_.getChatJoinRequests(channel_id);
        std::ostringstream oss;
        oss << "{\"success\":true,\"requests\":[";
        bool first = true;
        for (const auto& req : requests) {
            if (!first) oss << ",";
            first = false;
            oss << "{\"id\":\"" << JsonParser::escapeJson(req.id) << "\","
                << "\"user_id\":\"" << JsonParser::escapeJson(req.user_id) << "\","
                << "\"username\":\"" << JsonParser::escapeJson(req.username) << "\","
                << "\"joined_at\":\"" << JsonParser::escapeJson(req.joined_at) << "\"}";
        }
        oss << "]}";
        return oss.str();
    }
    
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageRequests, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }
    
    auto requests = db_manager_.getChannelJoinRequests(channel_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"requests\":[";
    
    bool first = true;
    for (const auto& req : requests) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << JsonParser::escapeJson(req.id) << "\","
            << "\"user_id\":\"" << JsonParser::escapeJson(req.user_id) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(req.username) << "\","
            << "\"joined_at\":\"" << JsonParser::escapeJson(req.joined_at) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleAcceptChannelJoinRequest(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string target_user_id = data["target_user_id"];
    
    if (token.empty() || channel_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and target_user_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanInvite, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        if (db_manager_.acceptChatJoinRequest(channel_id, target_user_id)) {
            return JsonParser::createSuccessResponse("Join request accepted");
        }
        return JsonParser::createErrorResponse("Failed to accept join request");
    }

    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageRequests, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }

    if (db_manager_.acceptChannelJoinRequest(channel_id, target_user_id)) {
        return JsonParser::createSuccessResponse("Join request accepted");
    }

    return JsonParser::createErrorResponse("Failed to accept join request");
}

std::string RequestHandler::handleRejectChannelJoinRequest(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string target_user_id = data["target_user_id"];
    
    if (token.empty() || channel_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and target_user_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanInvite, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        if (db_manager_.rejectChatJoinRequest(channel_id, target_user_id)) {
            return JsonParser::createSuccessResponse("Join request rejected");
        }
        return JsonParser::createErrorResponse("Failed to reject join request");
    }

    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageRequests, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }

    if (db_manager_.rejectChannelJoinRequest(channel_id, target_user_id)) {
        return JsonParser::createSuccessResponse("Join request rejected");
    }

    return JsonParser::createErrorResponse("Failed to reject join request");
}

std::string RequestHandler::handleSetChannelAdminPermissions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string target_user_id = data["target_user_id"];
    bool revoke = false;
    if (data.find("revoke") != data.end()) {
        revoke = (data["revoke"] == "true" || data["revoke"] == "1");
    }
    uint64_t perms = 0;
    if (data.find("perms") != data.end()) {
        try {
            perms = std::stoull(data["perms"]);
        } catch (...) {
            perms = 0;
        }
    }

    if (token.empty() || channel_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and target_user_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanPromote, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        auto target = db_manager_.getChatParticipantV2(channel_id, target_user_id);
        if (target.chat_id.empty()) {
            return JsonParser::createErrorResponse("Target user is not a channel member");
        }
        if (target.status == "owner") {
            return JsonParser::createErrorResponse("Cannot change channel creator");
        }
        if (revoke || perms == 0) {
            if (db_manager_.demoteChatAdminV2(channel_id, target_user_id)) {
                return JsonParser::createSuccessResponse("Admin rights removed");
            }
            return JsonParser::createErrorResponse("Failed to update admin rights");
        }
        std::string title = data.find("title") != data.end() ? data["title"] : ("admin-" + target_user_id);
        if (db_manager_.promoteAdmin(channel_id, target_user_id, perms, title)) {
            return JsonParser::createSuccessResponse("Admin permissions updated");
        }
        return JsonParser::createErrorResponse("Failed to update admin permissions");
    }

    // Legacy channels
    auto legacy = db_manager_.getChannelById(channel_id);
    if (legacy.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        legacy = db_manager_.getChannelByCustomLink(usernameKey);
        if (!legacy.id.empty()) {
            channel_id = legacy.id;
        }
    }
    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageAdmins, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error.empty() ? "Permission denied" : permission_error);
    }
    auto target_member = db_manager_.getChannelMember(channel_id, target_user_id);
    if (target_member.id.empty()) {
        return JsonParser::createErrorResponse("Target user is not a channel member");
    }
    if (target_member.role == "creator") {
        return JsonParser::createErrorResponse("Cannot change channel creator");
    }
    const std::string next_role = (revoke || perms == 0) ? "subscriber" : "admin";
    if (db_manager_.updateChannelMemberRole(channel_id, target_user_id, next_role)) {
        return JsonParser::createSuccessResponse("Admin permissions updated");
    }
    return JsonParser::createErrorResponse("Failed to update admin permissions");
}

std::string RequestHandler::handleBanChannelMember(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string target_user_id = data["target_user_id"];
    bool banned = true;
    if (data.find("banned") != data.end()) {
        banned = (data["banned"] == "true" || data["banned"] == "1");
    }

    if (token.empty() || channel_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, channel_id and target_user_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanRestrict, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        auto target_participant = db_manager_.getChatParticipantV2(channel_id, target_user_id);
        if (target_participant.chat_id.empty()) {
            return JsonParser::createErrorResponse("Target user is not a channel member");
        }
        if (target_participant.status == "owner") {
            return JsonParser::createErrorResponse("Cannot ban channel creator");
        }
        const std::string new_status = banned ? "kicked" : "member";
        if (db_manager_.updateChatParticipantStatusV2(channel_id, target_user_id, new_status)) {
            return JsonParser::createSuccessResponse(banned ? "Member banned" : "Member unbanned");
        }
        return JsonParser::createErrorResponse("Failed to update ban status");
    }

    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::BanUser, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }

    auto target_member = db_manager_.getChannelMember(channel_id, target_user_id);
    if (target_member.id.empty()) {
        return JsonParser::createErrorResponse("Target user is not a channel member");
    }
    if (target_member.role == "creator") {
        return JsonParser::createErrorResponse("Cannot ban channel creator");
    }

    if (db_manager_.banChannelMember(channel_id, target_user_id, banned)) {
        return JsonParser::createSuccessResponse(banned ? "Member banned" : "Member unbanned");
    }

    return JsonParser::createErrorResponse("Failed to update ban status");
}

std::string RequestHandler::handleGetChannelMembers(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];

    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channels (support UUID and @username)
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "403 Forbidden" : perm_error);
        }
        auto members = db_manager_.getChannelMembersV2(channel_id);
        std::ostringstream oss;
        oss << "{\"success\":true,\"members\":[";
        bool first = true;
        for (const auto& m : members) {
            if (!first) oss << ",";
            first = false;
            oss << "{"
                << "\"user_id\":\"" << JsonParser::escapeJson(m.user_id) << "\","
                << "\"username\":\"" << JsonParser::escapeJson(m.username) << "\","
                << "\"role\":\"" << JsonParser::escapeJson(m.role) << "\","
                << "\"is_banned\":" << (m.is_banned ? "true" : "false") << ","
                << "\"joined_at\":\"" << JsonParser::escapeJson(m.joined_at) << "\","
                << "\"admin_perms\":" << m.admin_perms << ","
                << "\"admin_title\":\"" << JsonParser::escapeJson(m.admin_title) << "\""
                << "}";
        }
        oss << "]}";
        return oss.str();
    }

    // Legacy channels: resolve by custom_link if needed
    auto legacy = db_manager_.getChannelById(channel_id);
    if (legacy.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        legacy = db_manager_.getChannelByCustomLink(usernameKey);
        if (!legacy.id.empty()) {
            channel_id = legacy.id;
        }
    }

    std::string permission_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &permission_error)) {
        return JsonParser::createErrorResponse(permission_error);
    }

    auto members = db_manager_.getChannelMembers(channel_id);
    std::ostringstream oss;
    oss << "{\"success\":true,\"members\":[";
    bool first = true;
    for (const auto& m : members) {
        if (!first) oss << ",";
        first = false;
        oss << "{"
            << "\"user_id\":\"" << JsonParser::escapeJson(m.user_id) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(m.username) << "\","
            << "\"role\":\"" << JsonParser::escapeJson(m.role) << "\","
            << "\"is_banned\":" << (m.is_banned ? "true" : "false") << ","
            << "\"joined_at\":\"" << JsonParser::escapeJson(m.joined_at) << "\""
            << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSearchChannel(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string custom_link = data.find("custom_link") != data.end() ? data["custom_link"] : "";
    std::string username = data.find("username") != data.end() ? data["username"] : "";
    
    // Поддерживаем оба варианта: custom_link и username
    if (custom_link.empty() && !username.empty()) {
        custom_link = username;
    }
    
    if (custom_link.empty()) {
        return JsonParser::createErrorResponse("custom_link or username required");
    }
    
    // Убираем @ если есть
    if (custom_link[0] == '@') {
        custom_link = custom_link.substr(1);
    }
    
    // Валидация
    if (custom_link.length() < 3 || custom_link.length() > 50) {
        return JsonParser::createErrorResponse("Invalid custom_link format");
    }
    
    // Prefer v2 unified channels if present
    Chat chat = db_manager_.getChatByUsernameV2(custom_link);
    if (!chat.id.empty() && chat.type == "channel") {
        std::ostringstream oss;
        oss << "{\"success\":true,\"channel\":{"
            << "\"id\":\"" << JsonParser::escapeJson(chat.id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(chat.title) << "\","
            << "\"description\":\"" << JsonParser::escapeJson(chat.about) << "\","
            << "\"custom_link\":\"" << JsonParser::escapeJson(chat.username) << "\","
            << "\"avatar_url\":\"" << JsonParser::escapeJson(chat.avatar_url) << "\","
            << "\"is_private\":" << (chat.is_public ? "false" : "true") << ","
            << "\"show_author\":" << (chat.sign_messages ? "false" : "true") << ","
            << "\"created_at\":\"" << JsonParser::escapeJson(chat.created_at) << "\""
            << "}}";
        return oss.str();
    }

    // Legacy fallback
    auto channel = db_manager_.getChannelByCustomLink(custom_link);
    if (channel.id.empty()) {
        return JsonParser::createErrorResponse("Channel not found");
    }
    
    // Формируем ответ
    std::ostringstream oss;
    oss << "{\"success\":true,\"channel\":{"
        << "\"id\":\"" << JsonParser::escapeJson(channel.id) << "\","
        << "\"name\":\"" << JsonParser::escapeJson(channel.name) << "\","
        << "\"description\":\"" << JsonParser::escapeJson(channel.description) << "\","
        << "\"custom_link\":\"" << JsonParser::escapeJson(channel.custom_link) << "\","
        << "\"avatar_url\":\"" << JsonParser::escapeJson(channel.avatar_url) << "\","
        << "\"is_private\":" << (channel.is_private ? "true" : "false") << ","
        << "\"show_author\":" << (channel.show_author ? "true" : "false") << ","
        << "\"created_at\":\"" << JsonParser::escapeJson(channel.created_at) << "\""
        << "}}";
    
    return oss.str();
}

std::string RequestHandler::handleChannelUrl(const std::string& path) {
    // Извлекаем username из пути /@username
    std::string username = path.substr(2); // Убираем "/@"
    
    if (username.empty()) {
        // Редирект на главную страницу чата
        return "HTTP/1.1 302 Found\r\nLocation: /chat\r\n\r\n";
    }
    
    // Функция для получения первого UTF-8 символа (поддержка кириллицы и эмодзи)
    auto getFirstUtf8Char = [](const std::string& str) -> std::string {
        if (str.empty()) return "X";
        unsigned char first = static_cast<unsigned char>(str[0]);
        size_t charLen = 1;
        if ((first & 0x80) == 0) charLen = 1;           // ASCII
        else if ((first & 0xE0) == 0xC0) charLen = 2;   // 2-byte UTF-8 (кириллица)
        else if ((first & 0xF0) == 0xE0) charLen = 3;   // 3-byte UTF-8
        else if ((first & 0xF8) == 0xF0) charLen = 4;   // 4-byte UTF-8 (эмодзи)
        if (charLen > str.size()) charLen = str.size();
        return str.substr(0, charLen);
    };
    
    // Функция для преобразования UTF-8 символа в uppercase (для кириллицы)
    auto toUpperUtf8 = [](const std::string& ch) -> std::string {
        if (ch.size() == 1) {
            return std::string(1, static_cast<char>(toupper(static_cast<unsigned char>(ch[0]))));
        }
        if (ch.size() == 2) {
            unsigned char b0 = static_cast<unsigned char>(ch[0]);
            unsigned char b1 = static_cast<unsigned char>(ch[1]);
            if (b0 == 0xD0 && b1 >= 0xB0 && b1 <= 0xBF) {
                return std::string{static_cast<char>(0xD0), static_cast<char>(b1 - 0x20)};
            }
            if (b0 == 0xD1 && b1 >= 0x80 && b1 <= 0x8F) {
                return std::string{static_cast<char>(0xD0), static_cast<char>(b1 + 0x20)};
            }
            if (b0 == 0xD1 && b1 == 0x91) {
                return std::string{static_cast<char>(0xD0), static_cast<char>(0x81)};
            }
        }
        return ch;
    };
    
    // === Стили в стиле Telegram с Xipher брендингом ===
    auto buildStyles = []() -> std::string {
        std::ostringstream css;
        css << "*{box-sizing:border-box;margin:0;padding:0;}";
        css << "html,body{height:100%;width:100%;}";
        css << "body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
            << "background:#1a1a1a;color:#fff;min-height:100vh;}";
        // Header
        css << ".header{position:fixed;top:0;left:0;right:0;height:56px;background:rgba(33,33,33,0.98);"
            << "backdrop-filter:blur(10px);display:flex;align-items:center;justify-content:space-between;"
            << "padding:0 20px;z-index:1000;border-bottom:1px solid rgba(255,255,255,0.1);}";
        css << ".header-logo{display:flex;align-items:center;gap:10px;font-size:20px;font-weight:700;"
            << "color:#fff;text-decoration:none;}";
        css << ".header-logo svg{width:28px;height:28px;}";
        css << ".header-logo span{background:linear-gradient(135deg,#6366f1,#8b5cf6);"
            << "-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;}";
        css << ".btn-download{background:linear-gradient(135deg,#6366f1,#8b5cf6);color:#fff;padding:10px 20px;"
            << "border-radius:20px;text-decoration:none;font-weight:600;font-size:14px;transition:all 0.2s;"
            << "box-shadow:0 4px 15px rgba(99,102,241,0.4);}";
        css << ".btn-download:hover{transform:scale(1.05);box-shadow:0 6px 20px rgba(99,102,241,0.5);}";
        // Main content - простая центровка без flexbox на body
        css << ".main{min-height:100vh;display:flex;align-items:center;justify-content:center;"
            << "padding:80px 20px 40px;position:relative;}";
        // Card
        css << ".card{background:rgba(33,33,33,0.95);backdrop-filter:blur(20px);border-radius:16px;"
            << "padding:32px 24px;max-width:380px;width:100%;text-align:center;"
            << "box-shadow:0 20px 60px rgba(0,0,0,0.5),0 0 0 1px rgba(255,255,255,0.05);}";
        // Avatar
        css << ".avatar{width:120px;height:120px;border-radius:50%;margin:0 auto 16px;"
            << "display:flex;align-items:center;justify-content:center;font-size:48px;font-weight:700;"
            << "color:#fff;position:relative;overflow:hidden;background:linear-gradient(135deg,#6366f1,#8b5cf6);"
            << "box-shadow:0 8px 30px rgba(99,102,241,0.4);}";
        css << ".avatar.bot{background:linear-gradient(135deg,#10b981,#059669);"
            << "box-shadow:0 8px 30px rgba(16,185,129,0.4);}";
        css << ".avatar.channel{background:linear-gradient(135deg,#ef4444,#dc2626);"
            << "box-shadow:0 8px 30px rgba(239,68,68,0.4);}";
        css << ".avatar img{width:100%;height:100%;object-fit:cover;border-radius:50%;}";
        // Title
        css << ".title{font-size:22px;font-weight:700;color:#fff;margin-bottom:4px;}";
        css << ".username{font-size:15px;color:#8b8b8b;margin-bottom:8px;}";
        css << ".subscribers{font-size:14px;color:#6b6b6b;margin-bottom:16px;}";
        // Description
        css << ".description{font-size:15px;color:#aaa;line-height:1.6;margin-bottom:20px;max-height:100px;overflow:hidden;}";
        css << ".description a{color:#6366f1;text-decoration:none;}";
        css << ".description a:hover{text-decoration:underline;}";
        // Buttons
        css << ".buttons{display:flex;flex-direction:column;gap:10px;}";
        css << ".btn{display:block;padding:14px 24px;border-radius:24px;font-weight:600;font-size:15px;"
            << "text-decoration:none;text-align:center;transition:all 0.2s;cursor:pointer;border:none;"
            << "text-transform:uppercase;letter-spacing:0.5px;}";
        css << ".btn-primary{background:linear-gradient(135deg,#6366f1,#8b5cf6);color:#fff;"
            << "box-shadow:0 4px 20px rgba(99,102,241,0.4);}";
        css << ".btn-primary:hover{transform:translateY(-2px);box-shadow:0 6px 25px rgba(99,102,241,0.5);}";
        css << ".btn-outline{background:transparent;color:#6366f1;border:2px solid #6366f1;}";
        css << ".btn-outline:hover{background:rgba(99,102,241,0.1);}";
        css << ".btn-bot{background:linear-gradient(135deg,#10b981,#059669);color:#fff;"
            << "box-shadow:0 4px 20px rgba(16,185,129,0.4);}";
        css << ".btn-bot:hover{transform:translateY(-2px);box-shadow:0 6px 25px rgba(16,185,129,0.5);}";
        // Verified badge
        css << ".verified{display:inline-flex;align-items:center;gap:6px;}";
        css << ".verified svg{width:18px;height:18px;fill:#6366f1;}";
        return css.str();
    };
    
    auto buildHtmlHead = [](const std::string& title, const std::string& description, 
                           const std::string& url, const std::string& image,
                           const std::string& styles) -> std::string {
        std::ostringstream head;
        head << "<!DOCTYPE html><html lang=\"ru\"><head><meta charset=\"UTF-8\">";
        head << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no\">";
        head << "<title>" << title << " — Xipher</title>";
        head << "<link rel=\"icon\" type=\"image/x-icon\" href=\"/xipher.ico\">";
        head << "<link rel=\"shortcut icon\" href=\"/xipher.ico\">";
        head << "<link rel=\"apple-touch-icon\" href=\"/xipher.ico\">";
        head << "<meta name=\"description\" content=\"" << description << "\">";
        head << "<meta property=\"og:type\" content=\"website\">";
        head << "<meta property=\"og:title\" content=\"" << title << "\">";
        head << "<meta property=\"og:description\" content=\"" << description << "\">";
        head << "<meta property=\"og:url\" content=\"" << url << "\">";
        head << "<meta property=\"og:image\" content=\"" << image << "\">";
        head << "<meta name=\"theme-color\" content=\"#1a1a1a\">";
        head << "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">";
        head << "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>";
        head << "<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap\" rel=\"stylesheet\">";
        head << "<style>" << styles << "</style></head>";
        return head.str();
    };
    
    auto buildHeader = []() -> std::string {
        std::ostringstream h;
        h << "<header class=\"header\">";
        h << "<a href=\"/\" class=\"header-logo\">";
        h << "<svg viewBox=\"0 0 32 32\" fill=\"none\"><circle cx=\"16\" cy=\"16\" r=\"14\" fill=\"url(#g)\"/>"
          << "<defs><linearGradient id=\"g\" x1=\"0\" y1=\"0\" x2=\"32\" y2=\"32\">"
          << "<stop offset=\"0%\" stop-color=\"#6366f1\"/><stop offset=\"100%\" stop-color=\"#8b5cf6\"/>"
          << "</linearGradient></defs>"
          << "<text x=\"16\" y=\"21\" text-anchor=\"middle\" fill=\"#fff\" font-size=\"14\" font-weight=\"700\">X</text></svg>";
        h << "<span>Xipher</span></a>";
        h << "<a href=\"https://www.rustore.ru/catalog/app/com.xipher.messenger\" class=\"btn-download\">СКАЧАТЬ</a>";
        h << "</header>";
        return h.str();
    };
    
    // Prefer v2 unified channels by username, fallback to legacy channels table
    DatabaseManager::Channel channel;
    Chat chat = db_manager_.getChatByUsernameV2(username);
    if (!chat.id.empty() && chat.type == "channel") {
        channel.id = chat.id;
        channel.name = chat.title;
        channel.description = chat.about;
        channel.custom_link = chat.username;
        channel.is_private = !chat.is_public;
        channel.show_author = !chat.sign_messages;
        channel.created_at = chat.created_at;
    } else {
        channel = db_manager_.getChannelByCustomLink(username);
    }
    
    // Если канал найден - показываем страницу канала
    if (!channel.id.empty()) {
        const std::string safeName = htmlEscape(channel.name.empty() ? channel.custom_link : channel.name);
        const std::string rawDescription = channel.description.empty() ? "Добро пожаловать в наш канал!" : channel.description;
        std::string safeDescription = htmlEscape(rawDescription);
        std::string descriptionHtml = safeDescription;
        for (size_t pos = 0; (pos = descriptionHtml.find('\n', pos)) != std::string::npos; pos += 4) {
            descriptionHtml.replace(pos, 1, "<br>");
        }
        std::string descriptionMeta = safeDescription;
        for (size_t pos = 0; (pos = descriptionMeta.find('\n', pos)) != std::string::npos; ) {
            descriptionMeta.replace(pos, 1, " ");
        }
        
        std::string avatarSource = channel.name.empty() ? channel.custom_link : channel.name;
        std::string avatarLetter = avatarSource.empty() ? "X" : toUpperUtf8(getFirstUtf8Char(avatarSource));
        avatarLetter = htmlEscape(avatarLetter);
        
        // Формируем содержимое аватара канала (картинка или буква)
        std::string channelAvatarUrl = channel.avatar_url;
        if (channelAvatarUrl.empty() && !chat.avatar_url.empty()) {
            channelAvatarUrl = chat.avatar_url;
        }
        
        std::string avatarContent;
        if (!channelAvatarUrl.empty()) {
            std::string fullAvatarUrl = channelAvatarUrl;
            if (channelAvatarUrl[0] == '/') {
                fullAvatarUrl = "https://xipher.pro" + channelAvatarUrl;
            }
            avatarContent = "<img src=\"" + htmlEscape(fullAvatarUrl) + "\" alt=\"" + safeName + "\" onerror=\"this.style.display='none';this.parentNode.innerHTML='" + avatarLetter + "';\">";
        } else {
            avatarContent = avatarLetter;
        }
        
        // Получаем количество подписчиков
        int subscriberCount = db_manager_.getChannelSubscribersCount(channel.id);
        std::string subscriberText;
        if (subscriberCount >= 1000000) {
            subscriberText = std::to_string(subscriberCount / 1000000) + "." + std::to_string((subscriberCount % 1000000) / 100000) + " млн подписчиков";
        } else if (subscriberCount >= 1000) {
            subscriberText = std::to_string(subscriberCount / 1000) + " " + std::to_string((subscriberCount % 1000) / 100) + " подписчиков";
        } else {
            subscriberText = std::to_string(subscriberCount) + " подписчиков";
        }
        
        const std::string shareUrl = "https://xipher.pro" + path;
        const std::string openUrl = "/chat?channel=" + channel.id;
        
        std::ostringstream html;
        html << buildHtmlHead(safeName, descriptionMeta, shareUrl, 
                              channelAvatarUrl.empty() ? "https://xipher.pro/images/channel-share.png" :
                              (channelAvatarUrl[0] == '/' ? "https://xipher.pro" + channelAvatarUrl : channelAvatarUrl),
                              buildStyles());
        html << "<body>";
        html << buildHeader();
        html << "<main class=\"main\"><div class=\"card\">";
        html << "<div class=\"avatar channel\">" << avatarContent << "</div>";
        html << "<div class=\"title\">" << safeName << "</div>";
        html << "<div class=\"username\">@" << htmlEscape(username) << "</div>";
        html << "<div class=\"subscribers\">" << subscriberText << "</div>";
        html << "<div class=\"description\">" << descriptionHtml << "</div>";
        html << "<div class=\"buttons\">";
        html << "<a class=\"btn btn-primary\" href=\"" << openUrl << "\">ОТКРЫТЬ КАНАЛ</a>";
        html << "<a class=\"btn btn-outline\" href=\"/\">ОТКРЫТЬ В ВЕБ</a>";
        html << "</div></div></main></body></html>";
        
        const std::string htmlStr = html.str();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n";
        resp << "Content-Type: text/html; charset=utf-8\r\n";
        resp << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
        resp << "Content-Length: " << htmlStr.size() << "\r\n\r\n";
        resp << htmlStr;
        return resp.str();
    }
    
    // Проверяем, это пользователь?
    User user = db_manager_.getUserByUsername(username);
    if (!user.id.empty()) {
        UserProfile profile;
        db_manager_.getUserProfile(user.id, profile);
        
        // Получаем аватар пользователя
        std::string userAvatarUrl;
        std::string tempUsername;
        db_manager_.getUserPublic(user.id, tempUsername, userAvatarUrl);
        
        std::string displayName = profile.first_name.empty() ? user.username : profile.first_name;
        if (!profile.last_name.empty()) displayName += " " + profile.last_name;
        const std::string safeName = htmlEscape(displayName);
        
        std::string bioText = profile.bio.empty() ? "Пользователь Xipher" : profile.bio;
        std::string safeBio = htmlEscape(bioText);
        std::string bioHtml = safeBio;
        for (size_t pos = 0; (pos = bioHtml.find('\n', pos)) != std::string::npos; pos += 4) {
            bioHtml.replace(pos, 1, "<br>");
        }
        std::string bioMeta = safeBio;
        for (size_t pos = 0; (pos = bioMeta.find('\n', pos)) != std::string::npos; ) {
            bioMeta.replace(pos, 1, " ");
        }
        
        std::string avatarSource = displayName.empty() ? username : displayName;
        std::string avatarLetter = avatarSource.empty() ? "X" : toUpperUtf8(getFirstUtf8Char(avatarSource));
        avatarLetter = htmlEscape(avatarLetter);
        
        const std::string shareUrl = "https://xipher.pro" + path;
        const std::string openUrl = "/chat?user=" + user.id;
        
        std::string avatarClass = "avatar";
        std::string btnClass = "btn btn-primary";
        std::string btnText = "НАПИСАТЬ";
        if (user.is_bot) {
            avatarClass = "avatar bot";
            btnClass = "btn btn-bot";
            btnText = "ЗАПУСТИТЬ БОТА";
        }
        
        // Формируем содержимое аватара (картинка или буква)
        std::string avatarContent;
        if (!userAvatarUrl.empty()) {
            std::string fullAvatarUrl = userAvatarUrl;
            if (userAvatarUrl[0] == '/') {
                fullAvatarUrl = "https://xipher.pro" + userAvatarUrl;
            }
            avatarContent = "<img src=\"" + htmlEscape(fullAvatarUrl) + "\" alt=\"" + safeName + "\" onerror=\"this.style.display='none';this.parentNode.innerHTML='" + avatarLetter + "';\">";
        } else {
            avatarContent = avatarLetter;
        }
        
        std::ostringstream html;
        html << buildHtmlHead(safeName, bioMeta, shareUrl, 
                              userAvatarUrl.empty() ? "https://xipher.pro/images/user-share.png" : 
                              (userAvatarUrl[0] == '/' ? "https://xipher.pro" + userAvatarUrl : userAvatarUrl),
                              buildStyles());
        html << "<body>";
        html << buildHeader();
        html << "<main class=\"main\"><div class=\"card\">";
        html << "<div class=\"" << avatarClass << "\">" << avatarContent << "</div>";
        html << "<div class=\"title\">" << safeName << "</div>";
        html << "<div class=\"username\">@" << htmlEscape(username) << "</div>";
        html << "<div class=\"description\">" << bioHtml << "</div>";
        html << "<div class=\"buttons\">";
        html << "<a class=\"" << btnClass << "\" href=\"" << openUrl << "\">" << btnText << "</a>";
        html << "<a class=\"btn btn-outline\" href=\"/\">ОТКРЫТЬ В ВЕБ</a>";
        html << "</div></div></main></body></html>";
        
        const std::string htmlStr = html.str();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n";
        resp << "Content-Type: text/html; charset=utf-8\r\n";
        resp << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
        resp << "Content-Length: " << htmlStr.size() << "\r\n\r\n";
        resp << htmlStr;
        return resp.str();
    }
    
    // Ничего не найдено - страница 404
    std::ostringstream notFoundHtml;
    notFoundHtml << buildHtmlHead("Не найдено", "Пользователь или канал не найден", 
                                   "https://xipher.pro" + path, "https://xipher.pro/images/404.png", buildStyles());
    notFoundHtml << "<body>";
    notFoundHtml << buildHeader();
    notFoundHtml << "<main class=\"main\"><div class=\"card\">";
    notFoundHtml << "<div class=\"avatar\" style=\"background:linear-gradient(135deg,#64748b,#475569);\">?</div>";
    notFoundHtml << "<div class=\"title\">@" << htmlEscape(username) << "</div>";
    notFoundHtml << "<div class=\"username\">не найден</div>";
    notFoundHtml << "<div class=\"description\">Возможно, ссылка устарела или пользователь сменил имя.<br>Проверьте правильность написания.</div>";
    notFoundHtml << "<div class=\"buttons\">";
    notFoundHtml << "<a class=\"btn btn-primary\" href=\"/\">НА ГЛАВНУЮ</a>";
    notFoundHtml << "<a class=\"btn btn-outline\" href=\"/register\">РЕГИСТРАЦИЯ</a>";
    notFoundHtml << "</div></div></main></body></html>";
    
    const std::string notFoundStr = notFoundHtml.str();
    std::ostringstream resp;
    resp << "HTTP/1.1 404 Not Found\r\n";
    resp << "Content-Type: text/html; charset=utf-8\r\n";
    resp << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
    resp << "Content-Length: " << notFoundStr.size() << "\r\n\r\n";
    resp << notFoundStr;
    return resp.str();
}

std::string RequestHandler::handleKickMember(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string target_user_id = data["target_user_id"];
    
    if (token.empty() || group_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and target_user_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем права администратора
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || (member.role != "admin" && member.role != "creator")) {
        return JsonParser::createErrorResponse("Only admins can kick members");
    }
    
    // Нельзя кикнуть создателя
    auto target_member = db_manager_.getGroupMember(group_id, target_user_id);
    if (target_member.role == "creator") {
        return JsonParser::createErrorResponse("Cannot kick group creator");
    }
    
    if (db_manager_.removeGroupMember(group_id, target_user_id)) {
        return JsonParser::createSuccessResponse("Member kicked from group");
    }
    
    return JsonParser::createErrorResponse("Failed to kick member");
}

std::string RequestHandler::handleAddFriendToGroup(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string friend_id = data["friend_id"];
    
    if (token.empty() || group_id.empty() || friend_id.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and friend_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем, что пользователь является участником группы
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("You are not a member of this group");
    }
    
    // Проверяем, что friend_id действительно друг
    if (!db_manager_.areFriends(user_id, friend_id)) {
        return JsonParser::createErrorResponse("User is not your friend");
    }
    
    if (db_manager_.addGroupMember(group_id, friend_id, "member")) {
        return JsonParser::createSuccessResponse("Friend added to group");
    }
    
    return JsonParser::createErrorResponse("Failed to add friend to group");
}

std::string RequestHandler::handleSetAdminPermissions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string target_user_id = data["target_user_id"];
    std::string role = data.find("role") != data.end() ? data["role"] : "admin";
    std::string permissions_json = data.find("permissions") != data.end() ? data["permissions"] : "{}";
    
    if (token.empty() || group_id.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and target_user_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Только создатель может назначать администраторов
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || member.role != "creator") {
        return JsonParser::createErrorResponse("Only group creator can set admin permissions");
    }
    
    // Нельзя изменить роль создателя
    if (target_user_id == user_id) {
        return JsonParser::createErrorResponse("Cannot change creator role");
    }
    
    // Обновляем роль
    if (!db_manager_.updateGroupMemberRole(group_id, target_user_id, role)) {
        return JsonParser::createErrorResponse("Failed to update role");
    }
    
    // Если это админ, сохраняем его права
    if (role == "admin" && !permissions_json.empty() && permissions_json != "{}") {
        db_manager_.updateAdminPermissions(group_id, target_user_id, permissions_json);
    }
    
    return JsonParser::createSuccessResponse("Admin permissions set");
}

std::string RequestHandler::handleDeleteGroup(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Только создатель может удалить группу
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || member.role != "creator") {
        return JsonParser::createErrorResponse("Only group creator can delete the group");
    }
    
    if (db_manager_.deleteGroup(group_id)) {
        return JsonParser::createSuccessResponse("Group deleted");
    }
    
    return JsonParser::createErrorResponse("Failed to delete group");
}

std::string RequestHandler::handleSearchGroupMessages(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string query = data["query"];
    
    if (token.empty() || group_id.empty() || query.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and query required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Проверяем, что пользователь является участником группы
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("You are not a member of this group");
    }
    
    auto messages = db_manager_.searchGroupMessages(group_id, query, 50);
    
    std::string result = "{\"success\":true,\"messages\":[";
    bool first = true;
    for (const auto& msg : messages) {
        if (!first) result += ",";
        first = false;
        result += "{\"id\":\"" + msg.id + "\",";
        result += "\"content\":\"" + JsonParser::escapeJson(msg.content) + "\",";
        result += "\"sender_id\":\"" + msg.sender_id + "\",";
        result += "\"sender_name\":\"" + JsonParser::escapeJson(msg.sender_username) + "\",";
        result += "\"created_at\":\"" + msg.created_at + "\"}";
    }
    result += "]}";
    
    return result;
}

std::string RequestHandler::handleUpdateGroupPermissions(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data["group_id"];
    std::string permission = data["permission"];
    bool enabled = (data["enabled"] == "true" || data["enabled"] == "1");
    
    if (token.empty() || group_id.empty() || permission.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and permission required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Только создатель/админ может менять разрешения
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.id.empty() || (member.role != "creator" && member.role != "admin")) {
        return JsonParser::createErrorResponse("Only admins can update group permissions");
    }
    
    if (db_manager_.updateGroupPermission(group_id, permission, enabled)) {
        return JsonParser::createSuccessResponse("Permission updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update permission");
}

std::string RequestHandler::handleGetChannelInfo(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Try v2 chat first
    Chat chat = db_manager_.getChatV2(channel_id);
    if (chat.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        chat = db_manager_.getChatByUsernameV2(usernameKey);
        if (!chat.id.empty()) {
            channel_id = chat.id;
        }
    }
    if (!chat.id.empty() && chat.type == "channel") {
        ChatPermissions perms = db_manager_.getChatPermissions(channel_id, user_id);
        // Fallback for creator even if no participant row
        if (chat.created_by == user_id) {
            perms.found = true;
            perms.status = "owner";
            perms.perms = kAllAdminPerms;
        }
        bool is_member = perms.found && perms.status != "kicked" && perms.status != "left";
        bool is_admin = (perms.status == "owner") || (perms.perms != 0);
        int subscribers_count = db_manager_.countChannelSubscribersV2(channel_id);
        int total_members = db_manager_.countChannelMembersV2(channel_id);
        std::ostringstream oss;
        oss << "{\"success\":true,\"channel\":{"
            << "\"id\":\"" << JsonParser::escapeJson(chat.id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(chat.title) << "\","
            << "\"description\":\"" << JsonParser::escapeJson(chat.about) << "\","
            << "\"custom_link\":\"" << JsonParser::escapeJson(chat.username) << "\","
            << "\"avatar_url\":\"" << JsonParser::escapeJson(chat.avatar_url) << "\","
            << "\"is_private\":" << (chat.is_public ? "false" : "true") << ","
            << "\"is_verified\":" << (chat.is_verified ? "true" : "false") << ","
            << "\"show_author\":" << (chat.sign_messages ? "false" : "true") << ","
            << "\"created_at\":\"" << JsonParser::escapeJson(chat.created_at) << "\""
            << "},\"user_role\":\"" << (perms.found ? JsonParser::escapeJson(perms.status) : "none") << "\","
            << "\"permissions\":" << perms.perms << ","
            << "\"is_subscribed\":" << (is_member ? "true" : "false") << ","
            << "\"subscribers_count\":" << subscribers_count << ","
            << "\"total_members\":" << total_members << ","
            << "\"is_admin\":" << (is_admin ? "true" : "false") << "}";
        return oss.str();
    }

    // Legacy channel tables (with fallback by username/custom_link)
    auto channel = db_manager_.getChannelById(channel_id);
    if (channel.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        channel = db_manager_.getChannelByCustomLink(usernameKey);
        if (channel.id.empty()) {
            return JsonParser::createErrorResponse("Channel not found");
        }
        // replace channel_id to the real id for membership checks
        channel_id = channel.id;
    }
    
    auto member = db_manager_.getChannelMember(channel_id, user_id);
    // Fallback: if creator but no membership row, treat as creator
    if (member.id.empty() && channel.creator_id == user_id) {
        member.id = "creator-implicit";
        member.channel_id = channel_id;
        member.user_id = user_id;
        member.role = "creator";
        member.is_banned = false;
    }
    int subscribers_count = db_manager_.countChannelSubscribers(channel_id);
    int total_members = db_manager_.countChannelMembers(channel_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"channel\":{"
        << "\"id\":\"" << JsonParser::escapeJson(channel.id) << "\","
        << "\"name\":\"" << JsonParser::escapeJson(channel.name) << "\","
        << "\"description\":\"" << JsonParser::escapeJson(channel.description) << "\","
        << "\"custom_link\":\"" << JsonParser::escapeJson(channel.custom_link) << "\","
        << "\"avatar_url\":\"" << JsonParser::escapeJson(channel.avatar_url) << "\","
        << "\"is_private\":" << (channel.is_private ? "true" : "false") << ","
        << "\"is_verified\":" << (channel.is_verified ? "true" : "false") << ","
        << "\"show_author\":" << (channel.show_author ? "true" : "false") << ","
        << "\"created_at\":\"" << JsonParser::escapeJson(channel.created_at) << "\""
        << "},\"user_role\":\"" << (member.id.empty() ? "none" : JsonParser::escapeJson(member.role)) << "\","
        << "\"permissions\":0,"
        << "\"is_subscribed\":" << (!member.id.empty() && !member.is_banned ? "true" : "false") << ","
        << "\"subscribers_count\":" << subscribers_count << ","
        << "\"total_members\":" << total_members << ","
        << "\"is_admin\":" << ((!member.id.empty() && (member.role == "admin" || member.role == "creator")) ? "true" : "false") << "}";
    
    return oss.str();
}

std::string RequestHandler::handleUnsubscribeChannel(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    
    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    // v2 unified channels (support UUID and @username)
    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        ChatPermissions perms = db_manager_.getChatPermissions(channel_id, user_id);
        if (!perms.found || perms.status == "kicked" || perms.status == "left") {
            return JsonParser::createErrorResponse("Not subscribed to channel");
        }
        // Admins/owners cannot leave
        if (perms.status == "owner" || perms.status == "admin") {
            return JsonParser::createErrorResponse("Admins cannot unsubscribe from channel");
        }
        if (db_manager_.leaveChatV2(channel_id, user_id)) {
            return JsonParser::createSuccessResponse("Unsubscribed from channel");
        }
        return JsonParser::createErrorResponse("Failed to unsubscribe from channel");
    }

    // Legacy channels (resolve by custom_link if needed)
    auto channel = db_manager_.getChannelById(channel_id);
    if (channel.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        channel = db_manager_.getChannelByCustomLink(usernameKey);
        if (!channel.id.empty()) {
            channel_id = channel.id;
        }
    }
    
    auto member = db_manager_.getChannelMember(channel_id, user_id);
    if (member.id.empty()) {
        return JsonParser::createErrorResponse("Not subscribed to channel");
    }
    
    // Нельзя отписаться если ты создатель или админ
    if (member.role == "creator" || member.role == "admin") {
        return JsonParser::createErrorResponse("Admins cannot unsubscribe from channel");
    }
    
    if (db_manager_.removeChannelMember(channel_id, user_id)) {
        return JsonParser::createSuccessResponse("Unsubscribed from channel");
    }
    
    return JsonParser::createErrorResponse("Failed to unsubscribe from channel");
}

std::string RequestHandler::handleDeleteChannel(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];

    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        if (chat_v2.created_by != user_id) {
            return JsonParser::createErrorResponse("Only channel owner can delete the channel");
        }
        if (db_manager_.deleteChatV2(channel_id)) {
            return JsonParser::createSuccessResponse("Channel deleted");
        }
        return JsonParser::createErrorResponse("Failed to delete channel");
    }

    auto legacy = db_manager_.getChannelById(channel_id);
    if (legacy.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        legacy = db_manager_.getChannelByCustomLink(usernameKey);
        if (!legacy.id.empty()) {
            channel_id = legacy.id;
        }
    }
    if (legacy.id.empty()) {
        return JsonParser::createErrorResponse("Channel not found");
    }
    if (legacy.creator_id != user_id) {
        return JsonParser::createErrorResponse("Only channel owner can delete the channel");
    }
    if (db_manager_.deleteChannelLegacy(channel_id)) {
        return JsonParser::createSuccessResponse("Channel deleted");
    }
    return JsonParser::createErrorResponse("Failed to delete channel");
}

std::string RequestHandler::handleLinkChannelDiscussion(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_id = data["channel_id"];
    std::string discussion_id = data.find("discussion_id") != data.end() ? data["discussion_id"] : "";
    bool create_new = false;
    if (data.find("create_new") != data.end()) {
        create_new = (data["create_new"] == "true" || data["create_new"] == "1");
    } else if (data.find("createNew") != data.end()) {
        create_new = (data["createNew"] == "true" || data["createNew"] == "1");
    }

    if (token.empty() || channel_id.empty()) {
        return JsonParser::createErrorResponse("Token and channel_id required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    Chat chat_v2;
    std::string resolved_channel_id = channel_id;
    if (resolveChannelV2(db_manager_, channel_id, &chat_v2, &resolved_channel_id)) {
        channel_id = resolved_channel_id;
        std::string perm_error;
        if (!checkPermission(user_id, channel_id, kPermCanChangeInfo, &perm_error)) {
            return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
        }
        if (discussion_id.empty()) {
            return JsonParser::createErrorResponse("discussion_id required for v2 channels");
        }
        if (db_manager_.linkChat(channel_id, discussion_id)) {
            std::map<std::string, std::string> resp;
            resp["discussion_id"] = discussion_id;
            return JsonParser::createSuccessResponse("Discussion linked", resp);
        }
        return JsonParser::createErrorResponse("Failed to link discussion chat");
    }

    auto legacy = db_manager_.getChannelById(channel_id);
    if (legacy.id.empty()) {
        std::string usernameKey = channel_id;
        if (!usernameKey.empty() && usernameKey[0] == '@') {
            usernameKey = usernameKey.substr(1);
        }
        legacy = db_manager_.getChannelByCustomLink(usernameKey);
        if (!legacy.id.empty()) {
            channel_id = legacy.id;
        }
    }
    if (legacy.id.empty()) {
        return JsonParser::createErrorResponse("Channel not found");
    }
    std::string perm_error;
    if (!checkChannelPermission(user_id, channel_id, ChannelPermission::ManageSettings, nullptr, &perm_error)) {
        return JsonParser::createErrorResponse(perm_error.empty() ? "Permission denied" : perm_error);
    }

    if (create_new || discussion_id.empty()) {
        std::string group_id;
        if (!db_manager_.createChannelChat(channel_id, group_id)) {
            return JsonParser::createErrorResponse("Failed to create discussion group");
        }
        std::map<std::string, std::string> resp;
        resp["discussion_id"] = group_id;
        return JsonParser::createSuccessResponse("Discussion linked", resp);
    }

    auto group = db_manager_.getGroupById(discussion_id);
    if (group.id.empty()) {
        return JsonParser::createErrorResponse("Discussion group not found");
    }
    if (db_manager_.linkChannelChat(channel_id, discussion_id)) {
        std::map<std::string, std::string> resp;
        resp["discussion_id"] = discussion_id;
        return JsonParser::createSuccessResponse("Discussion linked", resp);
    }
    return JsonParser::createErrorResponse("Failed to link discussion group");
}

// Проверка входящих звонков
std::string RequestHandler::handleCheckIncomingCalls(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    std::lock_guard<std::mutex> lock(calls_mutex_);
    
    // Проверяем, есть ли входящие звонки для этого пользователя
    auto it = active_calls_.find(user_id);
    if (it != active_calls_.end() && it->second["status"] == "pending") {
        std::map<std::string, std::string> call_info = it->second;
        
        std::ostringstream oss;
        oss << "{\"success\":true,\"call\":{"
            << "\"id\":\"" << JsonParser::escapeJson(call_info["call_id"]) << "\","
            << "\"caller_id\":\"" << JsonParser::escapeJson(call_info["caller_id"]) << "\","
            << "\"caller_username\":\"" << JsonParser::escapeJson(call_info["caller_username"]) << "\","
            << "\"call_type\":\"" << JsonParser::escapeJson(call_info["call_type"]) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(call_info["created_at"]) << "\""
            << "}}";
        
        // Помечаем звонок как обработанный (но не удаляем, чтобы можно было показать еще раз)
        // Удалим только после ответа
        return oss.str();
    }
    
    return JsonParser::createSuccessResponse("{\"success\":true,\"call\":null}");
}

// Завершение звонка
std::string RequestHandler::handleCallEnd(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    
    if (token.empty() || receiver_id.empty()) {
        return JsonParser::createErrorResponse("Token and receiver_id required");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    std::lock_guard<std::mutex> lock(calls_mutex_);
    
    // Удаляем звонок из активных
    auto it = active_calls_.find(receiver_id);
    if (it != active_calls_.end() && (it->second["caller_id"] == sender_id || receiver_id == sender_id)) {
        active_calls_.erase(it);
    }
    
    Logger::getInstance().info("Call ended: " + sender_id + " <-> " + receiver_id);
    
    return JsonParser::createSuccessResponse("Call ended");
}

// Проверка ответа на звонок (для исходящих звонков)
std::string RequestHandler::handleCheckCallResponse(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    
    if (token.empty() || receiver_id.empty()) {
        return JsonParser::createErrorResponse("Token and receiver_id required");
    }
    
    std::string caller_id = auth_manager_.getUserIdFromToken(token);
    if (caller_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    std::lock_guard<std::mutex> lock(calls_mutex_);
    
    // Проверяем, есть ли ответ на звонок
    std::string call_key = caller_id + "_" + receiver_id;
    auto it = call_responses_.find(call_key);
    if (it != call_responses_.end()) {
        std::string response = it->second;
        
        // Удаляем ответ после получения (кроме "ended", он удаляется отдельно)
        if (response != "ended") {
            call_responses_.erase(it);
        }
        
        std::ostringstream oss;
        oss << "{\"success\":true,\"response\":\"" << JsonParser::escapeJson(response) << "\"}";
        return oss.str();
    }
    
    return JsonParser::createSuccessResponse("{\"success\":true,\"response\":null}");
}

// ========== Verification Requests ==========

std::string RequestHandler::handleRequestVerification(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string channel_username = data["channel_username"];
    std::string reason = data.count("reason") ? data["reason"] : "";
    
    if (token.empty() || channel_username.empty()) {
        return JsonParser::createErrorResponse("Token and channel_username required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Remove @ if present
    if (!channel_username.empty() && channel_username[0] == '@') {
        channel_username = channel_username.substr(1);
    }
    
    // Find channel by username (try v2 first, then legacy)
    Chat chat = db_manager_.getChatByUsernameV2(channel_username);
    std::string channel_id, channel_name, creator_id;
    int subscribers_count = 0;
    bool is_verified = false;
    
    if (!chat.id.empty() && chat.type == "channel") {
        channel_id = chat.id;
        channel_name = chat.title;
        creator_id = chat.created_by;
        is_verified = chat.is_verified;
        subscribers_count = db_manager_.countChannelSubscribersV2(channel_id);
    } else {
        auto channel = db_manager_.getChannelByCustomLink(channel_username);
        if (channel.id.empty()) {
            return JsonParser::createErrorResponse("Канал не найден");
        }
        channel_id = channel.id;
        channel_name = channel.name;
        creator_id = channel.creator_id;
        is_verified = channel.is_verified;
        subscribers_count = db_manager_.countChannelSubscribers(channel_id);
    }
    
    // Check if user is the owner
    if (creator_id != user_id) {
        return JsonParser::createErrorResponse("Только владелец канала может подать заявку");
    }
    
    // Check if already verified
    if (is_verified) {
        return JsonParser::createErrorResponse("Канал уже верифицирован");
    }
    
    // Check for existing pending request
    std::string check_sql = "SELECT id FROM verification_requests WHERE channel_id = $1::uuid AND status = 'pending'";
    const char* check_params[1] = {channel_id.c_str()};
    PGresult* check_res = PQexecParams(db_manager_.getDb()->getConnection(), check_sql.c_str(), 1, nullptr, check_params, nullptr, nullptr, 0);
    if (check_res && PQntuples(check_res) > 0) {
        PQclear(check_res);
        return JsonParser::createErrorResponse("Заявка на верификацию уже подана");
    }
    if (check_res) PQclear(check_res);
    
    // Get owner username
    auto owner = db_manager_.getUserById(user_id);
    std::string owner_username = owner.username;
    
    // Insert verification request
    std::string subs_str = std::to_string(subscribers_count);
    std::string insert_sql = "INSERT INTO verification_requests (channel_id, channel_username, channel_name, owner_id, owner_username, subscribers_count, reason) "
                             "VALUES ($1::uuid, $2, $3, $4::uuid, $5, $6, $7)";
    const char* params[7] = {channel_id.c_str(), channel_username.c_str(), channel_name.c_str(), 
                             user_id.c_str(), owner_username.c_str(), subs_str.c_str(), reason.c_str()};
    PGresult* res = PQexecParams(db_manager_.getDb()->getConnection(), insert_sql.c_str(), 7, nullptr, params, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return JsonParser::createErrorResponse("Ошибка при создании заявки");
    }
    PQclear(res);
    
    return JsonParser::createSuccessResponse("Заявка на верификацию отправлена");
}

std::string RequestHandler::handleGetVerificationRequests(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string status_filter = data.count("status") ? data["status"] : "pending";
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check if user is admin
    auto user = db_manager_.getUserById(user_id);
    if (!user.is_admin) {
        return JsonParser::createErrorResponse("Доступ запрещён");
    }
    
    std::string sql = "SELECT id, channel_id, channel_username, channel_name, owner_id, owner_username, "
                      "subscribers_count, reason, status, admin_comment, created_at, reviewed_at "
                      "FROM verification_requests WHERE status = $1 ORDER BY created_at DESC LIMIT 100";
    const char* params[1] = {status_filter.c_str()};
    PGresult* res = PQexecParams(db_manager_.getDb()->getConnection(), sql.c_str(), 1, nullptr, params, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) PQclear(res);
        return JsonParser::createErrorResponse("Database error");
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"requests\":[";
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        if (i > 0) oss << ",";
        oss << "{\"id\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 0)) << "\","
            << "\"channel_id\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 1)) << "\","
            << "\"channel_username\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 2)) << "\","
            << "\"channel_name\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 3)) << "\","
            << "\"owner_id\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 4)) << "\","
            << "\"owner_username\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 5)) << "\","
            << "\"subscribers_count\":" << atoi(PQgetvalue(res, i, 6)) << ","
            << "\"reason\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 7)) << "\","
            << "\"status\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 8)) << "\","
            << "\"admin_comment\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 9)) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 10)) << "\","
            << "\"reviewed_at\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 11)) << "\"}";
    }
    
    oss << "]}";
    PQclear(res);
    return oss.str();
}

std::string RequestHandler::handleReviewVerification(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string request_id = data["request_id"];
    std::string action = data["action"]; // approve or reject
    std::string comment = data.count("comment") ? data["comment"] : "";
    
    if (token.empty() || request_id.empty() || action.empty()) {
        return JsonParser::createErrorResponse("Token, request_id and action required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check if user is admin
    auto user = db_manager_.getUserById(user_id);
    if (!user.is_admin) {
        return JsonParser::createErrorResponse("Доступ запрещён");
    }
    
    // Get request details
    std::string get_sql = "SELECT channel_id, status FROM verification_requests WHERE id = $1::uuid";
    const char* get_params[1] = {request_id.c_str()};
    PGresult* get_res = PQexecParams(db_manager_.getDb()->getConnection(), get_sql.c_str(), 1, nullptr, get_params, nullptr, nullptr, 0);
    
    if (!get_res || PQntuples(get_res) == 0) {
        if (get_res) PQclear(get_res);
        return JsonParser::createErrorResponse("Заявка не найдена");
    }
    
    std::string channel_id = PQgetvalue(get_res, 0, 0);
    std::string current_status = PQgetvalue(get_res, 0, 1);
    PQclear(get_res);
    
    if (current_status != "pending") {
        return JsonParser::createErrorResponse("Заявка уже рассмотрена");
    }
    
    std::string new_status = (action == "approve") ? "approved" : "rejected";
    
    // Update request status
    std::string update_sql = "UPDATE verification_requests SET status = $1, admin_comment = $2, reviewed_by = $3::uuid, reviewed_at = NOW() WHERE id = $4::uuid";
    const char* update_params[4] = {new_status.c_str(), comment.c_str(), user_id.c_str(), request_id.c_str()};
    PGresult* update_res = PQexecParams(db_manager_.getDb()->getConnection(), update_sql.c_str(), 4, nullptr, update_params, nullptr, nullptr, 0);
    
    if (!update_res || PQresultStatus(update_res) != PGRES_COMMAND_OK) {
        if (update_res) PQclear(update_res);
        return JsonParser::createErrorResponse("Ошибка при обновлении заявки");
    }
    PQclear(update_res);
    
    // If approved, set channel as verified
    if (action == "approve") {
        // Try v2 first
        std::string verify_sql = "UPDATE chats SET is_verified = true WHERE id = $1::uuid";
        const char* verify_params[1] = {channel_id.c_str()};
        PGresult* verify_res = PQexecParams(db_manager_.getDb()->getConnection(), verify_sql.c_str(), 1, nullptr, verify_params, nullptr, nullptr, 0);
        if (verify_res) PQclear(verify_res);
        
        // Also try legacy
        std::string verify_legacy_sql = "UPDATE channels SET is_verified = true WHERE id = $1::uuid";
        verify_res = PQexecParams(db_manager_.getDb()->getConnection(), verify_legacy_sql.c_str(), 1, nullptr, verify_params, nullptr, nullptr, 0);
        if (verify_res) PQclear(verify_res);
    }
    
    std::string msg = (action == "approve") ? "Заявка одобрена, канал верифицирован" : "Заявка отклонена";
    return JsonParser::createSuccessResponse(msg);
}

std::string RequestHandler::handleGetMyVerificationRequests(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    std::string sql = "SELECT id, channel_id, channel_username, channel_name, subscribers_count, reason, status, admin_comment, created_at, reviewed_at "
                      "FROM verification_requests WHERE owner_id = $1::uuid ORDER BY created_at DESC LIMIT 50";
    const char* params[1] = {user_id.c_str()};
    PGresult* res = PQexecParams(db_manager_.getDb()->getConnection(), sql.c_str(), 1, nullptr, params, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) PQclear(res);
        return JsonParser::createErrorResponse("Database error");
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"requests\":[";
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        if (i > 0) oss << ",";
        oss << "{\"id\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 0)) << "\","
            << "\"channel_id\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 1)) << "\","
            << "\"channel_username\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 2)) << "\","
            << "\"channel_name\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 3)) << "\","
            << "\"subscribers_count\":" << atoi(PQgetvalue(res, i, 4)) << ","
            << "\"reason\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 5)) << "\","
            << "\"status\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 6)) << "\","
            << "\"admin_comment\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 7)) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 8)) << "\","
            << "\"reviewed_at\":\"" << JsonParser::escapeJson(PQgetvalue(res, i, 9)) << "\"}";
    }
    
    oss << "]}";
    PQclear(res);
    return oss.str();
}

// =====================================================
// Group Topics (Forum mode) handlers implementation
// =====================================================

std::string RequestHandler::handleSetGroupForumMode(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data.count("group_id") ? data["group_id"] : "";
    std::string enabled_str = data.count("enabled") ? data["enabled"] : "true";
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check if user is admin/creator
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can change forum mode");
    }
    
    bool enabled = (enabled_str == "true" || enabled_str == "1");
    
    if (db_manager_.setGroupForumMode(group_id, enabled)) {
        Logger::getInstance().info("Group " + group_id + " forum mode set to " + enabled_str);
        std::map<std::string, std::string> response_data;
        response_data["forum_mode"] = enabled ? "true" : "false";
        return JsonParser::createSuccessResponse("Forum mode updated", response_data);
    }
    
    return JsonParser::createErrorResponse("Failed to update forum mode");
}

std::string RequestHandler::handleCreateGroupTopic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data.count("group_id") ? data["group_id"] : "";
    std::string name = data.count("name") ? data["name"] : "";
    std::string icon_emoji = data.count("icon_emoji") ? data["icon_emoji"] : "💬";
    std::string icon_color = data.count("icon_color") ? data["icon_color"] : "#9333ea";
    
    if (token.empty() || group_id.empty() || name.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and name required");
    }
    
    if (name.length() < 1 || name.length() > 128) {
        return JsonParser::createErrorResponse("Topic name must be between 1 and 128 characters");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check if group is in forum mode
    if (!db_manager_.isGroupForumMode(group_id)) {
        return JsonParser::createErrorResponse("Group is not in forum mode");
    }
    
    // Check if user is member
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.user_id.empty()) {
        return JsonParser::createErrorResponse("Not a group member");
    }
    
    std::string topic_id;
    if (db_manager_.createGroupTopic(group_id, name, icon_emoji, icon_color, user_id, false, topic_id)) {
        Logger::getInstance().info("Topic created: " + topic_id + " in group " + group_id);
        
        std::ostringstream oss;
        oss << "{\"success\":true,\"topic\":{";
        oss << "\"id\":\"" << JsonParser::escapeJson(topic_id) << "\",";
        oss << "\"group_id\":\"" << JsonParser::escapeJson(group_id) << "\",";
        oss << "\"name\":\"" << JsonParser::escapeJson(name) << "\",";
        oss << "\"icon_emoji\":\"" << JsonParser::escapeJson(icon_emoji) << "\",";
        oss << "\"icon_color\":\"" << JsonParser::escapeJson(icon_color) << "\",";
        oss << "\"is_general\":false,";
        oss << "\"is_closed\":false,";
        oss << "\"message_count\":0}}";
        return oss.str();
    }
    
    return JsonParser::createErrorResponse("Failed to create topic");
}

std::string RequestHandler::handleUpdateGroupTopic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    std::string name = data.count("name") ? data["name"] : "";
    std::string icon_emoji = data.count("icon_emoji") ? data["icon_emoji"] : "";
    std::string icon_color = data.count("icon_color") ? data["icon_color"] : "";
    std::string is_closed_str = data.count("is_closed") ? data["is_closed"] : "";
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    // Check if user is admin
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can edit topics");
    }
    
    // Use existing values if not provided
    if (name.empty()) name = topic.name;
    if (icon_emoji.empty()) icon_emoji = topic.icon_emoji;
    if (icon_color.empty()) icon_color = topic.icon_color;
    bool is_closed = is_closed_str.empty() ? topic.is_closed : (is_closed_str == "true" || is_closed_str == "1");
    
    if (db_manager_.updateGroupTopic(topic_id, name, icon_emoji, icon_color, is_closed)) {
        return JsonParser::createSuccessResponse("Topic updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update topic");
}

std::string RequestHandler::handleDeleteGroupTopic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    if (topic.is_general) {
        return JsonParser::createErrorResponse("Cannot delete General topic");
    }
    
    // Check if user is admin
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can delete topics");
    }
    
    if (db_manager_.deleteGroupTopic(topic_id)) {
        return JsonParser::createSuccessResponse("Topic deleted");
    }
    
    return JsonParser::createErrorResponse("Failed to delete topic");
}

std::string RequestHandler::handleGetGroupTopics(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data.count("group_id") ? data["group_id"] : "";
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check if user is member
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.user_id.empty()) {
        return JsonParser::createErrorResponse("Not a group member");
    }
    
    bool is_forum = db_manager_.isGroupForumMode(group_id);
    auto topics = db_manager_.getGroupTopics(group_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"forum_mode\":" << (is_forum ? "true" : "false") << ",\"topics\":[";
    
    bool first = true;
    for (const auto& topic : topics) {
        if (!first) oss << ",";
        first = false;
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(topic.id) << "\","
            << "\"group_id\":\"" << JsonParser::escapeJson(topic.group_id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(topic.name) << "\","
            << "\"icon_emoji\":\"" << JsonParser::escapeJson(topic.icon_emoji) << "\","
            << "\"icon_color\":\"" << JsonParser::escapeJson(topic.icon_color) << "\","
            << "\"is_general\":" << (topic.is_general ? "true" : "false") << ","
            << "\"is_closed\":" << (topic.is_closed ? "true" : "false") << ","
            << "\"is_hidden\":" << (topic.is_hidden ? "true" : "false") << ","
            << "\"pinned_order\":" << topic.pinned_order << ","
            << "\"message_count\":" << topic.message_count << ","
            << "\"unread_count\":" << topic.unread_count << ","
            << "\"last_message_at\":\"" << JsonParser::escapeJson(topic.last_message_at) << "\","
            << "\"last_message_content\":\"" << JsonParser::escapeJson(topic.last_message_content) << "\","
            << "\"last_message_sender\":\"" << JsonParser::escapeJson(topic.last_message_sender) << "\","
            << "\"last_message_sender_id\":\"" << JsonParser::escapeJson(topic.last_message_sender_id) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(topic.created_at) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handlePinGroupTopic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    int order = data.count("order") ? std::stoi(data["order"]) : 1;
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can pin topics");
    }
    
    if (db_manager_.pinGroupTopic(topic_id, order)) {
        return JsonParser::createSuccessResponse("Topic pinned");
    }
    
    return JsonParser::createErrorResponse("Failed to pin topic");
}

std::string RequestHandler::handleUnpinGroupTopic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can unpin topics");
    }
    
    if (db_manager_.unpinGroupTopic(topic_id)) {
        return JsonParser::createSuccessResponse("Topic unpinned");
    }
    
    return JsonParser::createErrorResponse("Failed to unpin topic");
}

std::string RequestHandler::handleGetTopicMessages(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    int limit = data.count("limit") ? std::stoi(data["limit"]) : 50;
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    // Check if user is member
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.user_id.empty()) {
        return JsonParser::createErrorResponse("Not a group member");
    }
    
    auto messages = db_manager_.getTopicMessages(topic_id, limit);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"topic\":{";
    oss << "\"id\":\"" << JsonParser::escapeJson(topic.id) << "\",";
    oss << "\"name\":\"" << JsonParser::escapeJson(topic.name) << "\",";
    oss << "\"icon_emoji\":\"" << JsonParser::escapeJson(topic.icon_emoji) << "\",";
    oss << "\"is_closed\":" << (topic.is_closed ? "true" : "false");
    oss << "},\"messages\":[";
    
    bool first = true;
    for (const auto& msg : messages) {
        if (!first) oss << ",";
        first = false;
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(msg.id) << "\","
            << "\"sender_id\":\"" << JsonParser::escapeJson(msg.sender_id) << "\","
            << "\"sender_username\":\"" << JsonParser::escapeJson(msg.sender_username) << "\","
            << "\"content\":\"" << JsonParser::escapeJson(msg.content) << "\","
            << "\"message_type\":\"" << JsonParser::escapeJson(msg.message_type) << "\","
            << "\"file_path\":\"" << JsonParser::escapeJson(msg.file_path) << "\","
            << "\"file_name\":\"" << JsonParser::escapeJson(msg.file_name) << "\","
            << "\"file_size\":" << msg.file_size << ","
            << "\"reply_to_message_id\":\"" << JsonParser::escapeJson(msg.reply_to_message_id) << "\","
            << "\"is_pinned\":" << (msg.is_pinned ? "true" : "false") << ","
            << "\"created_at\":\"" << JsonParser::escapeJson(msg.created_at) << "\"}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSendTopicMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    std::string content = data.count("content") ? data["content"] : "";
    std::string message_type = data.count("message_type") ? data["message_type"] : "text";
    std::string file_path = data.count("file_path") ? data["file_path"] : "";
    std::string file_name = data.count("file_name") ? data["file_name"] : "";
    long long file_size = data.count("file_size") ? std::stoll(data["file_size"]) : 0;
    std::string reply_to = data.count("reply_to_message_id") ? data["reply_to_message_id"] : "";
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    if (content.empty() && file_path.empty()) {
        return JsonParser::createErrorResponse("Content or file required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    if (topic.is_closed) {
        // Check if user is admin
        auto member = db_manager_.getGroupMember(topic.group_id, user_id);
        if (member.role != "creator" && member.role != "admin") {
            return JsonParser::createErrorResponse("Topic is closed");
        }
    }
    
    // Check if user is member
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.user_id.empty()) {
        return JsonParser::createErrorResponse("Not a group member");
    }
    
    if (member.is_muted) {
        return JsonParser::createErrorResponse("You are muted in this group");
    }
    
    if (db_manager_.sendGroupMessageToTopic(topic.group_id, topic_id, user_id, content, message_type, file_path, file_name, file_size, reply_to)) {
        Logger::getInstance().info("Message sent to topic " + topic_id + " by user " + user_id);
        
        // Notify via WebSocket if available
        if (ws_sender_) {
            std::ostringstream ws_msg;
            ws_msg << "{\"type\":\"topic_message\",\"topic_id\":\"" << JsonParser::escapeJson(topic_id) << "\","
                   << "\"group_id\":\"" << JsonParser::escapeJson(topic.group_id) << "\","
                   << "\"sender_id\":\"" << JsonParser::escapeJson(user_id) << "\"}";
            
            // Get group members and notify them
            auto members = db_manager_.getGroupMembers(topic.group_id);
            for (const auto& m : members) {
                if (m.user_id != user_id) {
                    ws_sender_(m.user_id, ws_msg.str());
                }
            }
        }
        
        return JsonParser::createSuccessResponse("Message sent");
    }
    
    return JsonParser::createErrorResponse("Failed to send message");
}

std::string RequestHandler::handleHideGeneralTopic(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    bool is_hidden = data.count("is_hidden") ? (data["is_hidden"] == "true") : true;
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    if (!topic.is_general) {
        return JsonParser::createErrorResponse("Only General topic can be hidden");
    }
    
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can hide General topic");
    }
    
    if (db_manager_.setTopicHidden(topic_id, is_hidden)) {
        return JsonParser::createSuccessResponse(is_hidden ? "General topic hidden" : "General topic shown");
    }
    
    return JsonParser::createErrorResponse("Failed to update topic visibility");
}

std::string RequestHandler::handleToggleTopicClosed(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    bool is_closed = data.count("is_closed") ? (data["is_closed"] == "true") : true;
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can close/reopen topics");
    }
    
    if (db_manager_.setTopicClosed(topic_id, is_closed)) {
        return JsonParser::createSuccessResponse(is_closed ? "Topic closed" : "Topic reopened");
    }
    
    return JsonParser::createErrorResponse("Failed to update topic status");
}

std::string RequestHandler::handleReorderPinnedTopics(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data.count("group_id") ? data["group_id"] : "";
    std::string topic_ids_str = data.count("topic_ids") ? data["topic_ids"] : "";
    
    if (token.empty() || group_id.empty() || topic_ids_str.empty()) {
        return JsonParser::createErrorResponse("Token, group_id and topic_ids required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.role != "creator" && member.role != "admin") {
        return JsonParser::createErrorResponse("Only admins can reorder pinned topics");
    }
    
    // Parse topic IDs (comma separated)
    std::vector<std::string> topic_ids;
    std::istringstream iss(topic_ids_str);
    std::string id;
    while (std::getline(iss, id, ',')) {
        if (!id.empty()) {
            topic_ids.push_back(id);
        }
    }
    
    if (db_manager_.reorderPinnedTopics(group_id, topic_ids)) {
        return JsonParser::createSuccessResponse("Topics reordered");
    }
    
    return JsonParser::createErrorResponse("Failed to reorder topics");
}

std::string RequestHandler::handleSearchTopics(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string group_id = data.count("group_id") ? data["group_id"] : "";
    std::string query = data.count("query") ? data["query"] : "";
    
    if (token.empty() || group_id.empty()) {
        return JsonParser::createErrorResponse("Token and group_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto member = db_manager_.getGroupMember(group_id, user_id);
    if (member.user_id.empty()) {
        return JsonParser::createErrorResponse("Not a group member");
    }
    
    auto topics = db_manager_.searchTopics(group_id, query);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"topics\":[";
    
    bool first = true;
    for (const auto& topic : topics) {
        if (!first) oss << ",";
        first = false;
        
        oss << "{\"id\":\"" << JsonParser::escapeJson(topic.id) << "\","
            << "\"name\":\"" << JsonParser::escapeJson(topic.name) << "\","
            << "\"icon_emoji\":\"" << JsonParser::escapeJson(topic.icon_emoji) << "\","
            << "\"icon_color\":\"" << JsonParser::escapeJson(topic.icon_color) << "\","
            << "\"is_general\":" << (topic.is_general ? "true" : "false") << ","
            << "\"is_closed\":" << (topic.is_closed ? "true" : "false") << ","
            << "\"message_count\":" << topic.message_count << "}";
    }
    
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleSetTopicNotifications(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string topic_id = data.count("topic_id") ? data["topic_id"] : "";
    std::string mode = data.count("mode") ? data["mode"] : "all";
    
    if (token.empty() || topic_id.empty()) {
        return JsonParser::createErrorResponse("Token and topic_id required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto topic = db_manager_.getGroupTopicById(topic_id);
    if (topic.id.empty()) {
        return JsonParser::createErrorResponse("Topic not found");
    }
    
    auto member = db_manager_.getGroupMember(topic.group_id, user_id);
    if (member.user_id.empty()) {
        return JsonParser::createErrorResponse("Not a group member");
    }
    
    if (db_manager_.setTopicNotifications(topic_id, user_id, mode)) {
        return JsonParser::createSuccessResponse("Notification settings updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update notifications");
}

} // namespace xipher
