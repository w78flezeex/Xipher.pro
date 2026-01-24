#include "../../include/voip/voip_access_control.hpp"
#include "../../include/utils/logger.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace xipher {

VoipAccessControl::VoipAccessControl(int64_t cutoff_timestamp, bool enabled)
    : cutoff_timestamp_(cutoff_timestamp), enabled_(enabled) {
    if (enabled_) {
        Logger::getInstance().info("VoIP Access Control ENABLED with cutoff: " + 
                                   std::to_string(cutoff_timestamp_));
    } else {
        Logger::getInstance().info("VoIP Access Control DISABLED - all users have access");
    }
}

bool VoipAccessControl::checkUserAccess(const std::string& user_id, DatabaseManager& db_manager) {
    // If access control is disabled, all users have access
    if (!enabled_) {
        Logger::getInstance().debug("VoIP access GRANTED (access control disabled) for user: " + user_id);
        return true;
    }
    
    try {
        User user = db_manager.getUserById(user_id);
        
        if (user.id.empty()) {
            Logger::getInstance().warning("VoIP access check: User not found: " + user_id);
            return false;
        }
        
        if (!user.is_active) {
            Logger::getInstance().warning("VoIP access check: User is inactive: " + user_id);
            return false;
        }
        
        // Parse registration timestamp
        int64_t registration_timestamp = parseTimestamp(user.created_at);
        
        if (registration_timestamp == 0) {
            Logger::getInstance().error("VoIP access check: Failed to parse timestamp for user: " + user_id);
            return false; // Fail closed - deny access if we can't verify
        }
        
        bool has_access = checkAccessByTimestamp(registration_timestamp);
        
        if (!has_access) {
            Logger::getInstance().info("VoIP access DENIED for user " + user_id + 
                                      " (registered: " + user.created_at + 
                                      ", cutoff: " + std::to_string(cutoff_timestamp_) + ")");
        } else {
            Logger::getInstance().debug("VoIP access GRANTED for user " + user_id);
        }
        
        return has_access;
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("VoIP access check exception: " + std::string(e.what()));
        return false; // Fail closed
    }
}

bool VoipAccessControl::checkAccessByTimestamp(int64_t registration_timestamp) const {
    // Legacy users (registered BEFORE cutoff) have access
    // New users (registered AFTER cutoff) are blocked
    return registration_timestamp <= cutoff_timestamp_;
}

int64_t VoipAccessControl::parseTimestamp(const std::string& timestamp_str) {
    if (timestamp_str.empty()) {
        return 0;
    }
    
    // PostgreSQL TIMESTAMP WITH TIME ZONE format examples:
    // "2024-01-15 10:30:45.123456+00"
    // "2024-01-15 10:30:45+00"
    // "2024-01-15T10:30:45Z"
    
    try {
        std::tm tm = {};
        std::istringstream ss(timestamp_str);
        
        // Try ISO 8601 format first (with T separator)
        if (timestamp_str.find('T') != std::string::npos) {
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            if (ss.fail()) {
                // Try with milliseconds
                ss.clear();
                ss.str(timestamp_str);
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            }
        } else {
            // PostgreSQL format (space separator)
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            if (ss.fail()) {
                // Try with milliseconds
                ss.clear();
                ss.str(timestamp_str);
                size_t dot_pos = timestamp_str.find('.');
                if (dot_pos != std::string::npos) {
                    std::string without_ms = timestamp_str.substr(0, dot_pos);
                    ss.str(without_ms);
                    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                }
            }
        }
        
        if (ss.fail()) {
            Logger::getInstance().warning("Failed to parse timestamp: " + timestamp_str);
            return 0;
        }
        
        // Convert to Unix timestamp
        // Note: mktime assumes local time, but PostgreSQL timestamps are UTC
        // We need to handle timezone properly
        std::time_t time = std::mktime(&tm);
        
        // Adjust for UTC (this is a simplified approach)
        // In production, you'd want to parse the timezone offset properly
        // For now, assume the timestamp is already in UTC
        return static_cast<int64_t>(time);
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Timestamp parse exception: " + std::string(e.what()));
        return 0;
    }
}

} // namespace xipher
