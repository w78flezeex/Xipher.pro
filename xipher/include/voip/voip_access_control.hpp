#ifndef VOIP_ACCESS_CONTROL_HPP
#define VOIP_ACCESS_CONTROL_HPP

#include <string>
#include <chrono>
#include "../database/db_manager.hpp"

namespace xipher {

/**
 * VoIP Access Control Gatekeeper
 * 
 * Implements the "Loyalty Beta" feature lock:
 * - Legacy users (registered BEFORE cutoff_date) have full VoIP access
 * - New users (registered AFTER cutoff_date) are blocked from VoIP
 * 
 * Can be disabled by setting enabled=false to allow all users
 */
class VoipAccessControl {
public:
    /**
     * Initialize the access control with a cutoff timestamp
     * @param cutoff_timestamp Unix timestamp (seconds since epoch) - users registered after this are blocked
     * @param enabled If false, all users have access (check is bypassed)
     */
    explicit VoipAccessControl(int64_t cutoff_timestamp, bool enabled = true);
    
    /**
     * Check if a user has access to VoIP features
     * @param user_id User UUID to check
     * @param db_manager Database manager to query user registration date
     * @return true if user has access, false if blocked
     */
    bool checkUserAccess(const std::string& user_id, DatabaseManager& db_manager);
    
    /**
     * Check if a user has access based on their registration timestamp
     * @param registration_timestamp Unix timestamp of user registration
     * @return true if user has access, false if blocked
     */
    bool checkAccessByTimestamp(int64_t registration_timestamp) const;
    
    /**
     * Get the cutoff timestamp
     */
    int64_t getCutoffTimestamp() const { return cutoff_timestamp_; }
    
    /**
     * Set a new cutoff timestamp (for dynamic updates)
     */
    void setCutoffTimestamp(int64_t timestamp) { cutoff_timestamp_ = timestamp; }
    
    /**
     * Enable or disable access control
     * If disabled, all users have access (check is bypassed)
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    /**
     * Check if access control is enabled
     */
    bool isEnabled() const { return enabled_; }
    
    /**
     * Parse ISO 8601 timestamp string to Unix timestamp
     * Handles PostgreSQL TIMESTAMP WITH TIME ZONE format
     */
    static int64_t parseTimestamp(const std::string& timestamp_str);

private:
    int64_t cutoff_timestamp_; // Unix timestamp in seconds
    bool enabled_; // If false, all users have access
};

} // namespace xipher

#endif // VOIP_ACCESS_CONTROL_HPP
