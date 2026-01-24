#include "../../include/notifications/rustore_client.hpp"
#include "../../include/utils/json_parser.hpp"
#include "../../include/utils/logger.hpp"

#include <curl/curl.h>

#include <sstream>

namespace xipher {

namespace {

size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

std::string trimTrailingSlash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string extractRuStoreErrorStatus(const std::string& body) {
    const std::string status_key = "\"status\":\"";
    size_t pos = body.find(status_key);
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

RuStoreClient::RuStoreClient(const std::string& project_id,
                             const std::string& service_token,
                             const std::string& base_url)
    : project_id_(project_id),
      service_token_(service_token),
      base_url_(trimTrailingSlash(base_url.empty() ? "https://vkpns.rustore.ru" : base_url)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    ready_ = !project_id_.empty() && !service_token_.empty();
    if (!ready_) {
        Logger::getInstance().warning("RuStore push disabled: missing project id or service token");
    }
}

bool RuStoreClient::isReady() const {
    return ready_;
}

bool RuStoreClient::sendMessage(const std::string& device_token,
                                const std::string& title,
                                const std::string& body,
                                const std::map<std::string, std::string>& data,
                                const std::string& channel_id,
                                std::string* out_error_code) {
    Logger::getInstance().info("RuStore sendMessage called, token_len=" + std::to_string(device_token.length()) + ", title=" + title);
    if (!ready_ || device_token.empty()) {
        Logger::getInstance().warning("RuStore sendMessage skipped: ready=" + std::string(ready_ ? "true" : "false") + ", token_empty=" + std::string(device_token.empty() ? "true" : "false"));
        return false;
    }
    if (out_error_code) {
        out_error_code->clear();
    }

    std::ostringstream payload;
    payload << "{\"message\":{"
            << "\"token\":\"" << JsonParser::escapeJson(device_token) << "\"";

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

    if (!title.empty() || !body.empty()) {
        payload << ",\"notification\":{";
        bool first = true;
        if (!title.empty()) {
            payload << "\"title\":\"" << JsonParser::escapeJson(title) << "\"";
            first = false;
        }
        if (!body.empty()) {
            if (!first) payload << ",";
            payload << "\"body\":\"" << JsonParser::escapeJson(body) << "\"";
        }
        payload << "}";
    }

    if (!channel_id.empty()) {
        payload << ",\"android\":{"
                << "\"notification\":{"
                << "\"channel_id\":\"" << JsonParser::escapeJson(channel_id) << "\""
                << "}}";
    }

    payload << "}}";

    std::string url = base_url_ + "/v1/projects/" + project_id_ + "/messages:send";

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + service_token_).c_str());

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
        std::string error_code = extractRuStoreErrorStatus(response);
        if (out_error_code) {
            *out_error_code = error_code;
        }
        std::string suffix;
        if (!error_code.empty()) {
            suffix = " (" + error_code + ")";
        }
        Logger::getInstance().warning("RuStore send failed: HTTP " + std::to_string(code) + suffix);
        return false;
    }
    return true;
}

} // namespace xipher
