#include "../include/utils/json_parser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace xipher {

namespace {

bool hexToInt(char c, uint32_t& value) {
    if (c >= '0' && c <= '9') {
        value = static_cast<uint32_t>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        value = static_cast<uint32_t>(10 + (c - 'a'));
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        value = static_cast<uint32_t>(10 + (c - 'A'));
        return true;
    }
    return false;
}

bool parseHex4(const std::string& input, size_t start, uint32_t& codepoint) {
    if (start + 4 > input.size()) {
        return false;
    }
    codepoint = 0;
    for (size_t i = 0; i < 4; i++) {
        uint32_t nibble = 0;
        if (!hexToInt(input[start + i], nibble)) {
            return false;
        }
        codepoint = (codepoint << 4) | nibble;
    }
    return true;
}

void appendUtf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        out += static_cast<char>(0xF0 | (codepoint >> 18));
        out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out += '?';
    }
}

void appendEscapedChar(const std::string& input, size_t& pos, std::string& out) {
    if (pos + 1 >= input.size()) {
        out += '\\';
        pos++;
        return;
    }

    const char esc = input[pos + 1];
    switch (esc) {
        case '"': out += '"'; pos += 2; return;
        case '\\': out += '\\'; pos += 2; return;
        case 'n': out += '\n'; pos += 2; return;
        case 'r': out += '\r'; pos += 2; return;
        case 't': out += '\t'; pos += 2; return;
        case 'b': out += '\b'; pos += 2; return;
        case 'f': out += '\f'; pos += 2; return;
        case 'u': {
            uint32_t codepoint = 0;
            if (!parseHex4(input, pos + 2, codepoint)) {
                out += 'u';
                pos += 2;
                return;
            }
            pos += 6;
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                if (pos + 1 < input.size() && input[pos] == '\\' && input[pos + 1] == 'u') {
                    uint32_t low = 0;
                    if (parseHex4(input, pos + 2, low) && low >= 0xDC00 && low <= 0xDFFF) {
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                        pos += 6;
                    }
                }
            }
            appendUtf8(out, codepoint);
            return;
        }
        default:
            out += esc;
            pos += 2;
            return;
    }
}

} // namespace

std::map<std::string, std::string> JsonParser::parse(const std::string& json) {
    std::map<std::string, std::string> result;
    
    if (json.empty()) {
        return result;
    }
    
    // Remove whitespace
    std::string cleaned;
    bool in_string = false;
    bool escape_next = false;
    
    for (size_t i = 0; i < json.length(); i++) {
        char c = json[i];
        
        if (escape_next) {
            cleaned += c;
            escape_next = false;
            continue;
        }
        
        if (c == '\\') {
            escape_next = true;
            cleaned += c;
            continue;
        }
        
        if (c == '"') {
            in_string = !in_string;
            cleaned += c;
            continue;
        }
        
        if (in_string) {
            cleaned += c;
        } else if (!std::isspace(c)) {
            cleaned += c;
        }
    }
    
    // Parse key-value pairs
    size_t pos = 0;
    while (pos < cleaned.length()) {
        // Find opening brace or start of key
        while (pos < cleaned.length() && cleaned[pos] != '"' && cleaned[pos] != '{') {
            pos++;
        }
        
        if (pos >= cleaned.length()) break;
        
        if (cleaned[pos] == '{') {
            pos++;
            continue;
        }
        
        if (cleaned[pos] != '"') break;
        pos++; // Skip opening quote
        
        // Extract key
        std::string key;
        while (pos < cleaned.length() && cleaned[pos] != '"') {
            if (cleaned[pos] == '\\') {
                appendEscapedChar(cleaned, pos, key);
                continue;
            }
            key += cleaned[pos];
            pos++;
        }
        
        if (pos >= cleaned.length()) break;
        pos++; // Skip closing quote
        
        // Find colon
        while (pos < cleaned.length() && cleaned[pos] != ':') {
            pos++;
        }
        if (pos >= cleaned.length()) break;
        pos++; // Skip colon
        
        // Extract value
        std::string value;
        
        // Skip whitespace before value
        while (pos < cleaned.length() && std::isspace(cleaned[pos])) {
            pos++;
        }
        
        if (pos >= cleaned.length()) break;
        
        if (cleaned[pos] == '"') {
            // String value
            pos++; // Skip opening quote
            while (pos < cleaned.length() && cleaned[pos] != '"') {
                if (cleaned[pos] == '\\') {
                    appendEscapedChar(cleaned, pos, value);
                    continue;
                }
                value += cleaned[pos];
                pos++;
            }
            if (pos < cleaned.length()) pos++; // Skip closing quote
        } else {
            // Number, boolean, or null
            while (pos < cleaned.length() && 
                   cleaned[pos] != ',' && 
                   cleaned[pos] != '}' && 
                   cleaned[pos] != ']' &&
                   !std::isspace(cleaned[pos])) {
                value += cleaned[pos];
                pos++;
            }
        }
        
        result[key] = value;
        
        // Skip to next pair or end
        while (pos < cleaned.length() && cleaned[pos] != ',' && cleaned[pos] != '}') {
            pos++;
        }
        if (pos < cleaned.length() && cleaned[pos] == ',') {
            pos++;
        }
    }
    
    return result;
}

std::vector<std::string> JsonParser::parseStringArray(const std::string& json) {
    std::vector<std::string> result;
    if (json.empty()) return result;
    
    size_t pos = json.find('[');
    if (pos == std::string::npos) return result;
    pos++;
    
    while (pos < json.length()) {
        // Skip whitespace
        while (pos < json.length() && std::isspace(json[pos])) pos++;
        if (pos >= json.length() || json[pos] == ']') break;
        
        if (json[pos] == '"') {
            pos++; // Skip opening quote
            std::string value;
            while (pos < json.length() && json[pos] != '"') {
                if (json[pos] == '\\' && pos + 1 < json.length()) {
                    pos++;
                    switch (json[pos]) {
                        case '"': value += '"'; break;
                        case '\\': value += '\\'; break;
                        case 'n': value += '\n'; break;
                        case 'r': value += '\r'; break;
                        case 't': value += '\t'; break;
                        default: value += json[pos]; break;
                    }
                    pos++;
                } else {
                    value += json[pos];
                    pos++;
                }
            }
            if (pos < json.length()) pos++; // Skip closing quote
            result.push_back(value);
        }
        
        // Find next comma or end
        while (pos < json.length() && json[pos] != ',' && json[pos] != ']') pos++;
        if (pos < json.length() && json[pos] == ',') pos++;
    }
    
    return result;
}

std::string JsonParser::stringify(const std::map<std::string, std::string>& data) {
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& pair : data) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << escapeJson(pair.first) << "\":\"" << escapeJson(pair.second) << "\"";
    }
    
    oss << "}";
    return oss.str();
}

std::string JsonParser::createResponse(bool success, const std::string& message, const std::map<std::string, std::string>& data) {
    std::ostringstream oss;
    oss << "{\"success\":" << (success ? "true" : "false") 
        << ",\"message\":\"" << escapeJson(message) << "\"";
    
    if (!data.empty()) {
        oss << ",\"data\":{";
        bool first = true;
        for (const auto& pair : data) {
            if (!first) oss << ",";
            first = false;
            oss << "\"" << escapeJson(pair.first) << "\":\"" << escapeJson(pair.second) << "\"";
        }
        oss << "}";
    }
    
    oss << "}";
    return oss.str();
}

std::string JsonParser::createErrorResponse(const std::string& message) {
    return createResponse(false, message);
}

std::string JsonParser::createSuccessResponse(const std::string& message, const std::map<std::string, std::string>& data) {
    return createResponse(true, message, data);
}

std::string JsonParser::escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string JsonParser::unescapeJson(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length();) {
        if (str[i] == '\\') {
            appendEscapedChar(str, i, result);
            continue;
        }
        result += str[i];
        i++;
    }
    return result;
}

} // namespace xipher
