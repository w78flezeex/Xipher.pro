#include "../../include/notifications/fcm_client.hpp"
#include "../../include/utils/json_parser.hpp"
#include "../../include/utils/logger.hpp"

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <chrono>
#include <fstream>
#include <sstream>

namespace xipher {

namespace {

size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

std::string base64UrlEncode(const std::string& input) {
    if (input.empty()) return "";
    std::string out;
    out.resize(4 * ((input.size() + 2) / 3));
    int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]),
                                  reinterpret_cast<const unsigned char*>(input.data()),
                                  static_cast<int>(input.size()));
    out.resize(written);
    for (char& c : out) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!out.empty() && out.back() == '=') {
        out.pop_back();
    }
    return out;
}

std::string extractFcmErrorCode(const std::string& body) {
    const std::string error_code_key = "\"errorCode\":\"";
    size_t pos = body.find(error_code_key);
    if (pos != std::string::npos) {
        pos += error_code_key.size();
        size_t end = body.find('"', pos);
        if (end != std::string::npos) {
            return body.substr(pos, end - pos);
        }
    }
    const std::string status_key = "\"status\":\"";
    pos = body.find(status_key);
    if (pos != std::string::npos) {
        pos += status_key.size();
        size_t end = body.find('"', pos);
        if (end != std::string::npos) {
            return body.substr(pos, end - pos);
        }
    }
    return "";
}

} // namespace

FcmClient::FcmClient(const std::string& service_account_path) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    ready_ = loadServiceAccount(service_account_path);
    if (!ready_) {
        Logger::getInstance().warning("FCM disabled: service account not loaded");
    }
}

bool FcmClient::isReady() const {
    return ready_;
}

bool FcmClient::loadServiceAccount(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::getInstance().warning("FCM service account file not found: " + path);
        return false;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    auto data = JsonParser::parse(buf.str());

    project_id_ = data.count("project_id") ? data["project_id"] : "";
    client_email_ = data.count("client_email") ? data["client_email"] : "";
    private_key_ = data.count("private_key") ? data["private_key"] : "";
    token_uri_ = data.count("token_uri") ? data["token_uri"] : "https://oauth2.googleapis.com/token";

    if (project_id_.empty() || client_email_.empty() || private_key_.empty()) {
        Logger::getInstance().warning("FCM service account missing required fields");
        return false;
    }
    return true;
}

std::string FcmClient::buildJwt() {
    const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    const int64_t exp = now + 3600;

    std::ostringstream header;
    header << "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";

    std::ostringstream claims;
    claims << "{\"iss\":\"" << JsonParser::escapeJson(client_email_) << "\","
           << "\"scope\":\"https://www.googleapis.com/auth/firebase.messaging\","
           << "\"aud\":\"" << JsonParser::escapeJson(token_uri_) << "\","
           << "\"iat\":" << now << ","
           << "\"exp\":" << exp << "}";

    std::string header_b64 = base64UrlEncode(header.str());
    std::string claims_b64 = base64UrlEncode(claims.str());
    std::string signing_input = header_b64 + "." + claims_b64;

    BIO* bio = BIO_new_mem_buf(private_key_.data(), -1);
    if (!bio) return "";
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return "";
    }

    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }

    if (EVP_DigestSignUpdate(ctx, signing_input.data(), signing_input.size()) <= 0) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(ctx, nullptr, &sig_len) <= 0) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }

    std::string sig(sig_len, 0);
    if (EVP_DigestSignFinal(ctx, reinterpret_cast<unsigned char*>(&sig[0]), &sig_len) <= 0) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }
    sig.resize(sig_len);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    std::string sig_b64 = base64UrlEncode(sig);
    return signing_input + "." + sig_b64;
}

std::string FcmClient::getAccessToken() {
    std::lock_guard<std::mutex> lock(token_mutex_);
    const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    if (!access_token_.empty() && now < access_token_expiry_) {
        return access_token_;
    }

    std::string jwt = buildJwt();
    if (jwt.empty()) {
        Logger::getInstance().warning("FCM JWT creation failed");
        return "";
    }

    std::string body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + jwt;

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_URL, token_uri_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || code < 200 || code >= 300) {
        Logger::getInstance().warning("FCM token request failed: HTTP " + std::to_string(code));
        return "";
    }

    auto parsed = JsonParser::parse(response);
    access_token_ = parsed.count("access_token") ? parsed["access_token"] : "";
    std::string expires_in = parsed.count("expires_in") ? parsed["expires_in"] : "0";
    long ttl = 0;
    try {
        ttl = std::stol(expires_in);
    } catch (...) {
        ttl = 0;
    }
    access_token_expiry_ = now + (ttl > 60 ? ttl - 30 : ttl);
    return access_token_;
}

bool FcmClient::sendMessage(const std::string& device_token,
                            const std::string& title,
                            const std::string& body,
                            const std::map<std::string, std::string>& data,
                            const std::string& channel_id,
                            const std::string& priority,
                            std::string* out_error_code) {
    if (!ready_ || device_token.empty()) return false;
    if (out_error_code) {
        out_error_code->clear();
    }

    std::string access_token = getAccessToken();
    if (access_token.empty()) return false;

    std::ostringstream payload;
    payload << "{\"message\":{"
            << "\"token\":\"" << JsonParser::escapeJson(device_token) << "\"";

    if (!title.empty() || !body.empty()) {
        payload << ",\"notification\":{"
                << "\"title\":\"" << JsonParser::escapeJson(title) << "\","
                << "\"body\":\"" << JsonParser::escapeJson(body) << "\"}";
    }

    if (!data.empty()) {
        payload << ",\"data\":{";
        bool first = true;
        for (const auto& kv : data) {
            if (!first) payload << ",";
            first = false;
            payload << "\"" << JsonParser::escapeJson(kv.first) << "\":"
                    << "\"" << JsonParser::escapeJson(kv.second) << "\"";
        }
        payload << "}";
    }

    payload << ",\"android\":{"
            << "\"priority\":\"" << (priority.empty() ? "HIGH" : JsonParser::escapeJson(priority)) << "\""
            << ",\"notification\":{"
            << "\"channel_id\":\"" << JsonParser::escapeJson(channel_id) << "\""
            << ",\"sound\":\"default\""
            << ",\"priority\":\"PRIORITY_HIGH\""
            << "}}";

    payload << "}}";

    std::string url = "https://fcm.googleapis.com/v1/projects/" + project_id_ + "/messages:send";

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    const std::string payload_str = payload.str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload_str.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || code < 200 || code >= 300) {
        std::string error_code = extractFcmErrorCode(response);
        if (out_error_code) {
            *out_error_code = error_code;
        }
        std::string suffix;
        if (!error_code.empty()) {
            suffix = " (" + error_code + ")";
        }
        Logger::getInstance().warning("FCM send failed: HTTP " + std::to_string(code) + suffix);
        return false;
    }
    return true;
}

} // namespace xipher
