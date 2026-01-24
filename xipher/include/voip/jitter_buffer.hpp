#ifndef JITTER_BUFFER_HPP
#define JITTER_BUFFER_HPP

#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <memory>

namespace xipher {
namespace voip {

/**
 * Adaptive Jitter Buffer for VoIP
 * 
 * Handles network jitter by buffering incoming audio packets
 * and delivering them at a steady rate to the playback system.
 * 
 * Features:
 * - Adaptive buffer sizing based on network conditions
 * - Packet reordering
 * - Late packet detection and handling
 * - Statistics tracking
 */
class JitterBuffer {
public:
    struct Packet {
        std::vector<uint8_t> data;
        uint32_t sequence_number;
        uint64_t timestamp_us;  // Microseconds since epoch
        uint32_t frame_size_samples;
        bool is_valid;
    };
    
    struct Config {
        uint32_t min_buffer_ms = 20;      // Minimum buffer size
        uint32_t max_buffer_ms = 200;     // Maximum buffer size
        uint32_t target_buffer_ms = 60;   // Target buffer size
        uint32_t late_packet_threshold_ms = 100; // Packets older than this are dropped
    };
    
    JitterBuffer();
    explicit JitterBuffer(const Config& config);
    ~JitterBuffer();
    
    /**
     * Add a packet to the jitter buffer
     * @param data Encoded audio data
     * @param sequence_number Packet sequence number
     * @param frame_size_samples Number of audio samples in this packet
     * @return true if packet was accepted, false if dropped (too late)
     */
    bool addPacket(const uint8_t* data, size_t data_size, 
                  uint32_t sequence_number, uint32_t frame_size_samples);
    
    /**
     * Get the next packet ready for playback
     * @param timeout_ms Maximum time to wait for a packet
     * @return Packet if available, nullptr if timeout
     */
    std::unique_ptr<Packet> getNextPacket(uint32_t timeout_ms = 0);
    
    /**
     * Get current buffer statistics
     */
    struct Statistics {
        uint32_t current_size_ms = 0;
        uint32_t packets_buffered = 0;
        uint64_t packets_received = 0;
        uint64_t packets_dropped_late = 0;
        uint64_t packets_reordered = 0;
        double average_jitter_ms = 0.0;
        uint32_t max_jitter_ms = 0;
    };
    
    Statistics getStatistics() const;
    
    /**
     * Reset the jitter buffer
     */
    void reset();
    
    /**
     * Adjust buffer size based on network conditions
     */
    void adapt();

private:
    Config config_;
    
    // Packet storage (ordered by sequence number)
    std::deque<std::unique_ptr<Packet>> buffer_;
    mutable std::mutex buffer_mutex_;
    
    // Sequence tracking
    uint32_t expected_sequence_;
    uint32_t last_sequence_;
    
    // Timing
    std::chrono::high_resolution_clock::time_point last_packet_time_;
    std::vector<double> jitter_samples_;  // For calculating average jitter
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    
    // Helper functions
    bool isPacketLate(const Packet& packet) const;
    void updateJitterEstimate(uint64_t packet_timestamp_us);
    uint32_t calculateCurrentBufferSizeMs() const;
    void cleanupLatePackets();
};

} // namespace voip
} // namespace xipher

#endif // JITTER_BUFFER_HPP
