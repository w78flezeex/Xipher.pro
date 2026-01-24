#include "../../include/voip/audio_engine.hpp"
#include "../../include/voip/lockfree_ring_buffer.hpp"
#include "../../include/voip/jitter_buffer.hpp"
#include "../../include/utils/logger.hpp"
#include <opus/opus.h>
#include <miniaudio.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <algorithm>

namespace xipher {
namespace voip {

// Forward declarations for miniaudio callbacks
struct AudioEngineContext {
    AudioEngine* engine;
    LockFreeRingBuffer<int16_t>* capture_buffer;
    LockFreeRingBuffer<int16_t>* playback_buffer;
};

// miniaudio data callback
void AudioEngine::maDataCallback(ma_device* pDevice, void* pOutput, 
                                  const void* pInput, ma_uint32 frameCount) {
    AudioEngineContext* ctx = static_cast<AudioEngineContext*>(pDevice->pUserData);
    AudioEngine* engine = ctx->engine;
    (void)engine;
    
    // Capture input
    if (pInput != nullptr && ctx->capture_buffer != nullptr) {
        const int16_t* input_samples = static_cast<const int16_t*>(pInput);
        size_t written = ctx->capture_buffer->push(input_samples, frameCount);
        if (written < frameCount) {
            Logger::getInstance().warning("Capture buffer overflow: " + 
                                         std::to_string(frameCount - written) + " samples dropped");
        }
    }
    
    // Playback output
    if (pOutput != nullptr && ctx->playback_buffer != nullptr) {
        int16_t* output_samples = static_cast<int16_t*>(pOutput);
        size_t read = ctx->playback_buffer->pop(output_samples, frameCount);
        
        if (read < frameCount) {
            // Underrun - fill with silence
            std::memset(output_samples + read, 0, (frameCount - read) * sizeof(int16_t));
            Logger::getInstance().debug("Playback buffer underrun: " + 
                                       std::to_string(frameCount - read) + " samples filled with silence");
        }
    }
}

AudioEngine::AudioEngine()
    : running_(false)
    , ma_context_(nullptr)
    , ma_capture_device_(nullptr)
    , ma_playback_device_(nullptr)
    , opus_encoder_(nullptr)
    , opus_decoder_(nullptr) {
}

AudioEngine::~AudioEngine() {
    stop();
    cleanup();
}

bool AudioEngine::initialize(const Config& config) {
    config_ = config;
    
    // Validate configuration
    if (config_.sample_rate != 8000 && config_.sample_rate != 12000 && 
        config_.sample_rate != 16000 && config_.sample_rate != 24000 && 
        config_.sample_rate != 48000) {
        Logger::getInstance().error("Invalid sample rate: " + std::to_string(config_.sample_rate));
        return false;
    }
    
    if (config_.channels != 1 && config_.channels != 2) {
        Logger::getInstance().error("Invalid channel count: " + std::to_string(config_.channels));
        return false;
    }
    
    // Initialize miniaudio
    if (!initializeMiniaudio()) {
        Logger::getInstance().error("Failed to initialize miniaudio");
        return false;
    }
    
    // Initialize Opus
    if (!initializeOpus()) {
        Logger::getInstance().error("Failed to initialize Opus");
        return false;
    }
    
    // Initialize ring buffers
    // Size: 2 seconds of audio data (safety margin)
    size_t buffer_size = (config_.sample_rate * 2) / (1000 / config_.frame_size_ms);
    buffer_size = (buffer_size + 63) & ~63; // Round up to power of 2 for efficiency
    
    capture_buffer_ = std::make_unique<LockFreeRingBuffer<int16_t>>(buffer_size);
    playback_buffer_ = std::make_unique<LockFreeRingBuffer<int16_t>>(buffer_size);
    
    // Initialize jitter buffer
    JitterBuffer::Config jb_config;
    jb_config.min_buffer_ms = config_.jitter_buffer_min_ms;
    jb_config.max_buffer_ms = config_.jitter_buffer_max_ms;
    jb_config.target_buffer_ms = (config_.jitter_buffer_min_ms + config_.jitter_buffer_max_ms) / 2;
    jitter_buffer_ = std::make_unique<JitterBuffer>(jb_config);
    
    Logger::getInstance().info("AudioEngine initialized: " + 
                               std::to_string(config_.sample_rate) + "Hz, " +
                               std::to_string(config_.channels) + "ch, " +
                               std::to_string(config_.frame_size_ms) + "ms frames");
    
    return true;
}

bool AudioEngine::initialize() {
    return initialize(Config{});
}

bool AudioEngine::start() {
    if (running_) {
        return true;
    }
    
    running_ = true;
    
    // Start miniaudio devices
    ma_result result;
    
    if (ma_capture_device_) {
        result = ma_device_start(static_cast<ma_device*>(ma_capture_device_));
        if (result != MA_SUCCESS) {
            Logger::getInstance().error("Failed to start capture device: " + std::to_string(result));
            running_ = false;
            return false;
        }
    }
    
    if (ma_playback_device_) {
        result = ma_device_start(static_cast<ma_device*>(ma_playback_device_));
        if (result != MA_SUCCESS) {
            Logger::getInstance().error("Failed to start playback device: " + std::to_string(result));
            running_ = false;
            return false;
        }
    }
    
    // Start processing threads
    encode_thread_ = std::thread(&AudioEngine::encodeLoop, this);
    decode_thread_ = std::thread(&AudioEngine::decodeLoop, this);
    
    Logger::getInstance().info("AudioEngine started");
    return true;
}

void AudioEngine::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Stop miniaudio devices
    if (ma_capture_device_) {
        ma_device_stop(static_cast<ma_device*>(ma_capture_device_));
    }
    if (ma_playback_device_) {
        ma_device_stop(static_cast<ma_device*>(ma_playback_device_));
    }
    
    // Wait for threads
    if (encode_thread_.joinable()) {
        encode_thread_.join();
    }
    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }
    
    Logger::getInstance().info("AudioEngine stopped");
}

void AudioEngine::feedEncodedAudio(const uint8_t* encoded_data, size_t data_size) {
    if (!jitter_buffer_ || !running_) {
        return;
    }
    
    // Generate sequence number (in production, extract from RTP header)
    static uint32_t sequence_number = 0;
    uint32_t frame_size_samples = getFrameSizeSamples();
    
    // Add to jitter buffer
    jitter_buffer_->addPacket(encoded_data, data_size, sequence_number++, frame_size_samples);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.packets_received++;
        stats_.bytes_received += data_size;
    }
}

AudioEngine::Statistics AudioEngine::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    Statistics stats = stats_;
    
    if (jitter_buffer_) {
        auto jb_stats = jitter_buffer_->getStatistics();
        stats.jitter_ms = jb_stats.average_jitter_ms;
        stats.current_jitter_buffer_size_ms = jb_stats.current_size_ms;
        stats.packets_lost = jb_stats.packets_dropped_late;
    }
    
    // Calculate packet loss percentage
    if (stats.packets_received > 0) {
        stats.packet_loss_percent = (stats.packets_lost * 100.0) / 
                                   (stats.packets_received + stats.packets_lost);
    }
    
    return stats;
}

void AudioEngine::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Statistics();
}

// ============================================================================
// HOT LOOP: Encode Loop (Capture -> Encode -> Network)
// ============================================================================
void AudioEngine::encodeLoop() {
    const size_t frame_size = getFrameSizeSamples();
    std::vector<int16_t> pcm_buffer(frame_size * config_.channels);
    std::vector<uint8_t> encoded_buffer(4000); // Max Opus frame size
    
    Logger::getInstance().info("Encode loop started, frame size: " + std::to_string(frame_size));
    
    while (running_) {
        // Pop from capture buffer
        size_t read = capture_buffer_->pop(pcm_buffer.data(), frame_size);
        
        if (read == 0) {
            // Buffer empty - sleep briefly
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }
        
        if (read < frame_size) {
            // Partial frame - pad with silence
            std::memset(pcm_buffer.data() + read, 0, 
                       (frame_size - read) * config_.channels * sizeof(int16_t));
        }
        
        // Encode with Opus
        int encoded_bytes = opus_encode(static_cast<OpusEncoder*>(opus_encoder_),
                                       pcm_buffer.data(),
                                       frame_size,
                                       encoded_buffer.data(),
                                       encoded_buffer.size());
        
        if (encoded_bytes > 0) {
            // Send to network via callback
            if (network_callback_) {
                network_callback_(encoded_buffer.data(), encoded_bytes);
            }
            
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.packets_sent++;
                stats_.bytes_sent += encoded_bytes;
            }
        } else {
            Logger::getInstance().warning("Opus encode error: " + std::to_string(encoded_bytes));
        }
        
        // Sleep for frame duration to maintain timing
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.frame_size_ms));
    }
    
    Logger::getInstance().info("Encode loop stopped");
}

// ============================================================================
// HOT LOOP: Decode Loop (Network -> JitterBuffer -> Decode -> Playback)
// ============================================================================
void AudioEngine::decodeLoop() {
    const size_t frame_size = getFrameSizeSamples();
    std::vector<int16_t> pcm_buffer(frame_size * config_.channels);
    
    Logger::getInstance().info("Decode loop started, frame size: " + std::to_string(frame_size));
    
    while (running_) {
        // Get next packet from jitter buffer
        auto packet = jitter_buffer_->getNextPacket(0); // Non-blocking
        
        if (packet == nullptr || !packet->is_valid) {
            // No packet available - use PLC (Packet Loss Concealment)
            // Opus decoder will handle PLC internally if configured
            // For now, output silence
            std::memset(pcm_buffer.data(), 0, frame_size * config_.channels * sizeof(int16_t));
            
            // Push silence to playback buffer
            playback_buffer_->push(pcm_buffer.data(), frame_size);
            
            // Sleep for frame duration
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.frame_size_ms));
            continue;
        }
        
        // Decode with Opus
        int decoded_samples = opus_decode(static_cast<OpusDecoder*>(opus_decoder_),
                                         packet->data.data(),
                                         packet->data.size(),
                                         pcm_buffer.data(),
                                         frame_size,
                                         0); // No FEC
        
        if (decoded_samples > 0) {
            // Push to playback buffer
            size_t written = playback_buffer_->push(pcm_buffer.data(), frame_size);
            
            if (written < frame_size) {
                Logger::getInstance().warning("Playback buffer overflow: " + 
                                             std::to_string(frame_size - written) + " samples dropped");
            }
        } else {
            Logger::getInstance().warning("Opus decode error: " + std::to_string(decoded_samples));
            // Output silence on decode error
            std::memset(pcm_buffer.data(), 0, frame_size * config_.channels * sizeof(int16_t));
            playback_buffer_->push(pcm_buffer.data(), frame_size);
        }
        
        // Sleep for frame duration
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.frame_size_ms));
    }
    
    Logger::getInstance().info("Decode loop stopped");
}

bool AudioEngine::initializeMiniaudio() {
    ma_context_config context_config = ma_context_config_init();
    ma_context* context = new ma_context();
    
    ma_result result = ma_context_init(nullptr, 0, &context_config, context);
    if (result != MA_SUCCESS) {
        Logger::getInstance().error("Failed to initialize miniaudio context: " + std::to_string(result));
        delete context;
        return false;
    }
    
    ma_context_ = context;
    
    // Create capture device
    ma_device_config capture_config = ma_device_config_init(ma_device_type_capture);
    capture_config.capture.format = ma_format_s16;
    capture_config.capture.channels = config_.channels;
    capture_config.sampleRate = config_.sample_rate;
    capture_config.dataCallback = maDataCallback;
    
    AudioEngineContext* ctx = new AudioEngineContext();
    ctx->engine = this;
    ctx->capture_buffer = capture_buffer_.get();
    capture_config.pUserData = ctx;
    
    ma_device* capture_device = new ma_device();
    result = ma_device_init(context, &capture_config, capture_device);
    if (result != MA_SUCCESS) {
        Logger::getInstance().warning("Failed to initialize capture device: " + std::to_string(result) + 
                                     " (capture may not be available)");
        delete capture_device;
    } else {
        ma_capture_device_ = capture_device;
    }
    
    // Create playback device
    ma_device_config playback_config = ma_device_config_init(ma_device_type_playback);
    playback_config.playback.format = ma_format_s16;
    playback_config.playback.channels = config_.channels;
    playback_config.sampleRate = config_.sample_rate;
    playback_config.dataCallback = maDataCallback;
    
    AudioEngineContext* playback_ctx = new AudioEngineContext();
    playback_ctx->engine = this;
    playback_ctx->playback_buffer = playback_buffer_.get();
    playback_config.pUserData = playback_ctx;
    
    ma_device* playback_device = new ma_device();
    result = ma_device_init(context, &playback_config, playback_device);
    if (result != MA_SUCCESS) {
        Logger::getInstance().warning("Failed to initialize playback device: " + std::to_string(result) + 
                                     " (playback may not be available)");
        delete playback_device;
    } else {
        ma_playback_device_ = playback_device;
    }
    
    return true;
}

bool AudioEngine::initializeOpus() {
    int error;
    
    // Create encoder
    OpusEncoder* encoder = opus_encoder_create(config_.sample_rate,
                                               config_.channels,
                                               OPUS_APPLICATION_VOIP,
                                               &error);
    if (error != OPUS_OK) {
        Logger::getInstance().error("Failed to create Opus encoder: " + std::string(opus_strerror(error)));
        return false;
    }
    
    // Configure encoder
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(config_.bitrate));
    opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(config_.enable_fec ? 1 : 0));
    opus_encoder_ctl(encoder, OPUS_SET_DTX(config_.enable_dtx ? 1 : 0));
    opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(5)); // Assume 5% packet loss
    
    // Enable low-latency mode
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5)); // Lower complexity = lower latency
    
    opus_encoder_ = encoder;
    
    // Create decoder
    OpusDecoder* decoder = opus_decoder_create(config_.sample_rate,
                                              config_.channels,
                                              &error);
    if (error != OPUS_OK) {
        Logger::getInstance().error("Failed to create Opus decoder: " + std::string(opus_strerror(error)));
        opus_encoder_destroy(encoder);
        return false;
    }
    
    opus_decoder_ = decoder;
    
    Logger::getInstance().info("Opus initialized: " + std::to_string(config_.bitrate) + " bps, " +
                               "FEC: " + std::string(config_.enable_fec ? "enabled" : "disabled"));
    
    return true;
}

void AudioEngine::cleanup() {
    if (opus_encoder_) {
        opus_encoder_destroy(static_cast<OpusEncoder*>(opus_encoder_));
        opus_encoder_ = nullptr;
    }
    
    if (opus_decoder_) {
        opus_decoder_destroy(static_cast<OpusDecoder*>(opus_decoder_));
        opus_decoder_ = nullptr;
    }
    
    if (ma_capture_device_) {
        ma_device_uninit(static_cast<ma_device*>(ma_capture_device_));
        delete static_cast<ma_device*>(ma_capture_device_);
        ma_capture_device_ = nullptr;
    }
    
    if (ma_playback_device_) {
        ma_device_uninit(static_cast<ma_device*>(ma_playback_device_));
        delete static_cast<ma_device*>(ma_playback_device_);
        ma_playback_device_ = nullptr;
    }
    
    if (ma_context_) {
        ma_context_uninit(static_cast<ma_context*>(ma_context_));
        delete static_cast<ma_context*>(ma_context_);
        ma_context_ = nullptr;
    }
}

} // namespace voip
} // namespace xipher
