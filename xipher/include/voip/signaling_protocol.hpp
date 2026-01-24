#ifndef SIGNALING_PROTOCOL_HPP
#define SIGNALING_PROTOCOL_HPP

#include <string>
#include <map>

namespace xipher {

/**
 * VoIP Signaling Protocol Definitions
 * 
 * JSON-based protocol for WebRTC signaling (Offer/Answer/ICE)
 */

namespace SignalingProtocol {

    // Message Types
    constexpr const char* CALL_INIT = "call_init";
    constexpr const char* CALL_OFFER = "call_offer";
    constexpr const char* CALL_ANSWER = "call_answer";
    constexpr const char* CALL_ICE_CANDIDATE = "call_ice_candidate";
    constexpr const char* CALL_END = "call_end";
    constexpr const char* CALL_ERROR = "call_error";
    
    // Error Codes
    namespace ErrorCode {
        constexpr int SUCCESS = 0;
        constexpr int FEATURE_LOCKED = 1001;      // User doesn't have VoIP access
        constexpr int INVALID_TOKEN = 1002;       // Authentication failed
        constexpr int USER_NOT_FOUND = 1003;      // Target user not found
        constexpr int USER_OFFLINE = 1004;        // Target user is offline
        constexpr int INVALID_MESSAGE = 1005;     // Malformed message
        constexpr int CALL_IN_PROGRESS = 1006;    // Another call is active
        constexpr int PERMISSION_DENIED = 1007;   // General permission error
    }
    
    /**
     * Create a call_init message
     */
    std::string createCallInit(const std::string& caller_id, 
                               const std::string& callee_id,
                               const std::string& call_type = "audio");
    
    /**
     * Create a call_offer message
     */
    std::string createCallOffer(const std::string& caller_id,
                                const std::string& callee_id,
                                const std::string& offer_sdp);
    
    /**
     * Create a call_answer message
     */
    std::string createCallAnswer(const std::string& caller_id,
                                  const std::string& callee_id,
                                  const std::string& answer_sdp);
    
    /**
     * Create a call_ice_candidate message
     */
    std::string createCallIceCandidate(const std::string& caller_id,
                                       const std::string& callee_id,
                                       const std::string& candidate,
                                       int sdp_mline_index,
                                       const std::string& sdp_mid);
    
    /**
     * Create a call_end message
     */
    std::string createCallEnd(const std::string& caller_id,
                              const std::string& callee_id,
                              const std::string& reason = "user_ended");
    
    /**
     * Create an error message
     */
    std::string createError(int error_code, 
                           const std::string& error_message,
                           const std::string& context = "");
    
    /**
     * Parse a signaling message
     */
    std::map<std::string, std::string> parseMessage(const std::string& json);
    
    /**
     * Validate message structure
     */
    bool validateMessage(const std::map<std::string, std::string>& message, 
                        const std::string& expected_type);

} // namespace SignalingProtocol

} // namespace xipher

#endif // SIGNALING_PROTOCOL_HPP
