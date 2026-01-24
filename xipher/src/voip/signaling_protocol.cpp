#include "../../include/voip/signaling_protocol.hpp"
#include "../../include/utils/json_parser.hpp"
#include <sstream>
#include <ctime>

namespace xipher {
namespace SignalingProtocol {

std::string createCallInit(const std::string& caller_id, 
                          const std::string& callee_id,
                          const std::string& call_type) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" << CALL_INIT << "\","
        << "\"caller_id\":\"" << JsonParser::escapeJson(caller_id) << "\","
        << "\"callee_id\":\"" << JsonParser::escapeJson(callee_id) << "\","
        << "\"call_type\":\"" << JsonParser::escapeJson(call_type) << "\","
        << "\"timestamp\":" << std::time(nullptr)
        << "}";
    return oss.str();
}

std::string createCallOffer(const std::string& caller_id,
                           const std::string& callee_id,
                           const std::string& offer_sdp) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" << CALL_OFFER << "\","
        << "\"caller_id\":\"" << JsonParser::escapeJson(caller_id) << "\","
        << "\"callee_id\":\"" << JsonParser::escapeJson(callee_id) << "\","
        << "\"offer\":\"" << JsonParser::escapeJson(offer_sdp) << "\","
        << "\"timestamp\":" << std::time(nullptr)
        << "}";
    return oss.str();
}

std::string createCallAnswer(const std::string& caller_id,
                            const std::string& callee_id,
                            const std::string& answer_sdp) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" << CALL_ANSWER << "\","
        << "\"caller_id\":\"" << JsonParser::escapeJson(caller_id) << "\","
        << "\"callee_id\":\"" << JsonParser::escapeJson(callee_id) << "\","
        << "\"answer\":\"" << JsonParser::escapeJson(answer_sdp) << "\","
        << "\"timestamp\":" << std::time(nullptr)
        << "}";
    return oss.str();
}

std::string createCallIceCandidate(const std::string& caller_id,
                                  const std::string& callee_id,
                                  const std::string& candidate,
                                  int sdp_mline_index,
                                  const std::string& sdp_mid) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" << CALL_ICE_CANDIDATE << "\","
        << "\"caller_id\":\"" << JsonParser::escapeJson(caller_id) << "\","
        << "\"callee_id\":\"" << JsonParser::escapeJson(callee_id) << "\","
        << "\"candidate\":\"" << JsonParser::escapeJson(candidate) << "\","
        << "\"sdp_mline_index\":" << sdp_mline_index << ","
        << "\"sdp_mid\":\"" << JsonParser::escapeJson(sdp_mid) << "\","
        << "\"timestamp\":" << std::time(nullptr)
        << "}";
    return oss.str();
}

std::string createCallEnd(const std::string& caller_id,
                         const std::string& callee_id,
                         const std::string& reason) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" << CALL_END << "\","
        << "\"caller_id\":\"" << JsonParser::escapeJson(caller_id) << "\","
        << "\"callee_id\":\"" << JsonParser::escapeJson(callee_id) << "\","
        << "\"reason\":\"" << JsonParser::escapeJson(reason) << "\","
        << "\"timestamp\":" << std::time(nullptr)
        << "}";
    return oss.str();
}

std::string createError(int error_code, 
                       const std::string& error_message,
                       const std::string& context) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" << CALL_ERROR << "\","
        << "\"error_code\":" << error_code << ","
        << "\"error_message\":\"" << JsonParser::escapeJson(error_message) << "\"";
    
    if (!context.empty()) {
        oss << ",\"context\":\"" << JsonParser::escapeJson(context) << "\"";
    }
    
    oss << ",\"timestamp\":" << std::time(nullptr) << "}";
    return oss.str();
}

std::map<std::string, std::string> parseMessage(const std::string& json) {
    return JsonParser::parse(json);
}

bool validateMessage(const std::map<std::string, std::string>& message, 
                    const std::string& expected_type) {
    if (message.find("type") == message.end()) {
        return false;
    }
    
    if (message.at("type") != expected_type) {
        return false;
    }
    
    // Type-specific validation
    if (expected_type == CALL_INIT || expected_type == CALL_OFFER || 
        expected_type == CALL_ANSWER || expected_type == CALL_END) {
        return message.find("caller_id") != message.end() &&
               message.find("callee_id") != message.end();
    }
    
    if (expected_type == CALL_OFFER) {
        return message.find("offer") != message.end();
    }
    
    if (expected_type == CALL_ANSWER) {
        return message.find("answer") != message.end();
    }
    
    if (expected_type == CALL_ICE_CANDIDATE) {
        return message.find("candidate") != message.end();
    }
    
    return true;
}

} // namespace SignalingProtocol
} // namespace xipher
