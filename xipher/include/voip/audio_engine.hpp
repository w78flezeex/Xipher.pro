#ifndef AUDIO_ENGINE_HPP
#define AUDIO_ENGINE_HPP

#include <cstdint>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <string>
#include <miniaudio.h>
#include "lockfree_ring_buffer.hpp"
#include "jitter_buffer.hpp"

namespace xipher {
namespace voip {

/**
 * High-Performance C++ Audio Engine for VoIP
 * 
 * Features:
 * - Low-latency audio capture/playback using miniaudio
 * - Opus codec encoding/decoding
 * - Adaptive jitter buffer
 * - Packet loss concealment (PLC)
 * - Lock-free SPSC ring buffer for thread-safe audio processing
 */
class AudioEngine {
public:
    using AudioCallback = std::function<void(const int16_t* samples, size_t frame_count)>;
    using NetworkCallback = std::function<void(const uint8_t* encoded_data, size_t data_size)>;
    
    struct Config {
        uint32_t sample_rate = 48000;        // Opus supports 8, 12, 16, 24, 48 kHz
        uint8_t channels = 1;                 // Mono (1) or Stereo (2)
        uint32_t frame_size_ms = 20;          // 20ms frames (standard for VoIP)
        uint32_t bitrate = 32000;             // 32 kbps (good quality for voice)
        bool enable_fec = true;               // Forward Error Correction
        bool enable_dtx = false;              // Discontinuous Transmission
        uint32_t jitter_buffer_min_ms = 20;  // Minimum jitter buffer size
        uint32_t jitter_buffer_max_ms = 200;  // Maximum jitter buffer size
    };
    
    AudioEngine();
    ~AudioEngine();
    
    /**
     * Initialize the audio engine with configuration
     */
    bool initialize();
    bool initialize(const Config& config);
    
    /**
     * Start audio capture and playback
     */
    bool start();
    
    /**
     * Stop audio capture and playback
     */
    void stop();
    
    /**
     * Check if engine is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * Set callback for captured audio (called from capture thread)
     */
    void setCaptureCallback(AudioCallback callback);
    
    /**
     * Set callback for encoded audio ready to send over network
     */
    void setNetworkCallback(NetworkCallback callback);
    
    /**
     * Feed received encoded audio data (from network thread)
     * This will decode and queue for playback
     */
    void feedEncodedAudio(const uint8_t* encoded_data, size_t data_size);
    
    /**
     * Get current audio statistics
     */
    struct Statistics {
        uint64_t packets_sent = 0;
        uint64_t packets_received = 0;
        uint64_t packets_lost = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        double jitter_ms = 0.0;
        double packet_loss_percent = 0.0;
        uint32_t current_jitter_buffer_size_ms = 0;
    };
    
    Statistics getStatistics() const;
    
    /**
     * Reset statistics
     */
    void resetStatistics();

private:
    Config config_;
    std::atomic<bool> running_;
    
    // miniaudio context and device handles
    void* ma_context_;      // ma_context*
    void* ma_capture_device_;  // ma_device*
    void* ma_playback_device_; // ma_device*
    
    // Opus encoder/decoder
    void* opus_encoder_;    // OpusEncoder*
    void* opus_decoder_;    // OpusDecoder*
    
    // Threads
    std::thread capture_thread_;
    std::thread playback_thread_;
    std::thread encode_thread_;
    std::thread decode_thread_;
    
    // Lock-free SPSC ring buffers
    std::unique_ptr<LockFreeRingBuffer<int16_t>> capture_buffer_;  // Capture -> Encode
    std::unique_ptr<LockFreeRingBuffer<int16_t>> playback_buffer_;  // Decode -> Playback
    std::unique_ptr<LockFreeRingBuffer<int16_t>> network_buffer_; // Encode -> Network
    
    // Callbacks
    AudioCallback capture_callback_;
    NetworkCallback network_callback_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    
    // Jitter buffer
    std::unique_ptr<JitterBuffer> jitter_buffer_;
    
    // Internal processing functions
    void captureLoop();
    void playbackLoop();
    void encodeLoop();
    void decodeLoop();
    
    // miniaudio callbacks (C-style)
    static void maDataCallback(ma_device* pDevice, void* pOutput, 
                               const void* pInput, ma_uint32 frameCount);
    
    // Helper functions
    bool initializeMiniaudio();
    bool initializeOpus();
    void cleanup();
    
    // Frame size in samples
    uint32_t getFrameSizeSamples() const {
        return (config_.sample_rate * config_.frame_size_ms) / 1000;
    }
};

} // namespace voip
} // namespace xipher

#endif // AUDIO_ENGINE_HPP
