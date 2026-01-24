#include "../../include/voip/jitter_buffer.hpp"
#include "../../include/utils/logger.hpp"
#include <algorithm>
#include <cmath>

namespace xipher {
namespace voip {

JitterBuffer::JitterBuffer()
    : JitterBuffer(Config{}) {}

JitterBuffer::JitterBuffer(const Config& config)
    : config_(config)
    , expected_sequence_(0)
    , last_sequence_(0) {
    reset();
}

JitterBuffer::~JitterBuffer() = default;

bool JitterBuffer::addPacket(const uint8_t* data, size_t data_size,
                            uint32_t sequence_number, uint32_t frame_size_samples) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    auto packet = std::make_unique<Packet>();
    packet->data.assign(data, data + data_size);
    packet->sequence_number = sequence_number;
    packet->frame_size_samples = frame_size_samples;
    packet->timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    packet->is_valid = true;
    
    // Check if packet is too late
    if (isPacketLate(*packet)) {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.packets_dropped_late++;
        Logger::getInstance().debug("Dropped late packet: seq=" + std::to_string(sequence_number));
        return false;
    }
    
    // Update jitter estimate
    updateJitterEstimate(packet->timestamp_us);
    
    // Insert packet in order (by sequence number)
    auto it = std::lower_bound(buffer_.begin(), buffer_.end(), packet,
        [](const std::unique_ptr<Packet>& a, const std::unique_ptr<Packet>& b) {
            return a->sequence_number < b->sequence_number;
        });
    
    // Check if this is a reordered packet
    if (it != buffer_.end() && (*it)->sequence_number == sequence_number) {
        // Duplicate packet, ignore
        return false;
    }
    
    if (it != buffer_.begin()) {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.packets_reordered++;
    }
    
    buffer_.insert(it, std::move(packet));
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.packets_received++;
        stats_.packets_buffered = buffer_.size();
        stats_.current_size_ms = calculateCurrentBufferSizeMs();
    }
    
    // Adapt buffer size if needed
    adapt();
    
    return true;
}

std::unique_ptr<JitterBuffer::Packet> JitterBuffer::getNextPacket(uint32_t timeout_ms) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (buffer_.empty()) {
        return nullptr;
    }
    
    // Check if we have the expected packet
    if (!buffer_.empty() && buffer_.front()->sequence_number == expected_sequence_) {
        auto packet = std::move(buffer_.front());
        buffer_.pop_front();
        expected_sequence_++;
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.packets_buffered = buffer_.size();
            stats_.current_size_ms = calculateCurrentBufferSizeMs();
        }
        
        return packet;
    }
    
    // If we don't have the expected packet, check if we should wait or skip
    // For now, return nullptr if expected packet is not available
    return nullptr;
}

JitterBuffer::Statistics JitterBuffer::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void JitterBuffer::reset() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    buffer_.clear();
    expected_sequence_ = 0;
    last_sequence_ = 0;
    jitter_samples_.clear();
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_ = Statistics();
}

void JitterBuffer::adapt() {
    // Adaptive buffer sizing based on jitter
    // If jitter is high, increase buffer size
    // If jitter is low, decrease buffer size
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    if (jitter_samples_.size() < 10) {
        return; // Need more samples
    }
    
    // Calculate average jitter
    double avg_jitter = 0.0;
    for (double j : jitter_samples_) {
        avg_jitter += j;
    }
    avg_jitter /= jitter_samples_.size();
    
    stats_.average_jitter_ms = avg_jitter;
    
    // Adjust target buffer size based on jitter
    // This is a simplified adaptation algorithm
    if (avg_jitter > config_.target_buffer_ms) {
        // Increase buffer
        config_.target_buffer_ms = std::min(
            config_.target_buffer_ms + 10,
            config_.max_buffer_ms
        );
    } else if (avg_jitter < config_.target_buffer_ms / 2) {
        // Decrease buffer
        config_.target_buffer_ms = std::max(
            config_.target_buffer_ms - 10,
            config_.min_buffer_ms
        );
    }
}

bool JitterBuffer::isPacketLate(const Packet& packet) const {
    if (buffer_.empty()) {
        return false;
    }
    
    // Compare with oldest packet in buffer
    const auto& oldest = buffer_.front();
    uint64_t age_us = packet.timestamp_us - oldest->timestamp_us;
    uint32_t age_ms = age_us / 1000;
    
    return age_ms > config_.late_packet_threshold_ms;
}

void JitterBuffer::updateJitterEstimate(uint64_t packet_timestamp_us) {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();
    
    if (last_packet_time_.time_since_epoch().count() > 0) {
        uint64_t last_us = std::chrono::duration_cast<std::chrono::microseconds>(
            last_packet_time_.time_since_epoch()
        ).count();
        
        // Calculate inter-arrival jitter
        uint64_t inter_arrival = now_us - last_us;
        double jitter_ms = std::abs(static_cast<double>(inter_arrival) / 1000.0 - 20.0); // Assuming 20ms packets
        
        jitter_samples_.push_back(jitter_ms);
        
        // Keep only last 50 samples
        if (jitter_samples_.size() > 50) {
            jitter_samples_.erase(jitter_samples_.begin());
        }
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        if (jitter_ms > stats_.max_jitter_ms) {
            stats_.max_jitter_ms = static_cast<uint32_t>(jitter_ms);
        }
    }
    
    last_packet_time_ = now;
}

uint32_t JitterBuffer::calculateCurrentBufferSizeMs() const {
    if (buffer_.empty()) {
        return 0;
    }
    
    // Estimate based on number of packets (assuming 20ms per packet)
    return static_cast<uint32_t>(buffer_.size() * 20);
}

void JitterBuffer::cleanupLatePackets() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();
    
    auto it = buffer_.begin();
    while (it != buffer_.end()) {
        uint64_t age_us = now_us - (*it)->timestamp_us;
        uint32_t age_ms = age_us / 1000;
        
        if (age_ms > config_.late_packet_threshold_ms) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.packets_dropped_late++;
            it = buffer_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace voip
} // namespace xipher
