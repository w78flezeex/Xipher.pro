#ifndef FCM_CLIENT_HPP
#define FCM_CLIENT_HPP

#include <map>
#include <mutex>
#include <string>

namespace xipher {

class FcmClient {
public:
    explicit FcmClient(const std::string& service_account_path);

    bool isReady() const;

    bool sendMessage(const std::string& device_token,
                     const std::string& title,
                     const std::string& body,
                     const std::map<std::string, std::string>& data,
                     const std::string& channel_id,
                     const std::string& priority,
                     std::string* out_error_code = nullptr);

private:
    bool loadServiceAccount(const std::string& path);
    std::string buildJwt();
    std::string getAccessToken();

    std::string project_id_;
    std::string client_email_;
    std::string private_key_;
    std::string token_uri_;

    std::string access_token_;
    int64_t access_token_expiry_{0};
    mutable std::mutex token_mutex_;
    bool ready_{false};
};

} // namespace xipher

#endif
