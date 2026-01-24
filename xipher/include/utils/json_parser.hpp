#ifndef JSON_PARSER_HPP
#define JSON_PARSER_HPP

#include <string>
#include <map>
#include <vector>

namespace xipher {

class JsonParser {
public:
    static std::map<std::string, std::string> parse(const std::string& json);
    static std::vector<std::string> parseStringArray(const std::string& json);
    static std::string stringify(const std::map<std::string, std::string>& data);
    static std::string createResponse(bool success, const std::string& message, const std::map<std::string, std::string>& data = {});
    static std::string createErrorResponse(const std::string& message);
    static std::string createSuccessResponse(const std::string& message, const std::map<std::string, std::string>& data = {});
    static std::string escapeJson(const std::string& str);
    static std::string unescapeJson(const std::string& str);
    
private:
};

} // namespace xipher

#endif // JSON_PARSER_HPP

