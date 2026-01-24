#ifndef RUSTORE_CLIENT_HPP
#define RUSTORE_CLIENT_HPP

#include <map>
#include <string>

namespace xipher {

class RuStoreClient {
public:
    RuStoreClient(const std::string& project_id,
                  const std::string& service_token,
                  const std::string& base_url = "");

    bool isReady() const;

    bool sendMessage(const std::string& device_token,
                     const std::string& title,
                     const std::string& body,
                     const std::map<std::string, std::string>& data,
                     const std::string& channel_id,
                     std::string* out_error_code = nullptr);

private:
    std::string project_id_;
    std::string service_token_;
    std::string base_url_;
    bool ready_{false};
};

} // namespace xipher

#endif
