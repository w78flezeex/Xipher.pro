#ifndef NAT_TRAVERSAL_HPP
#define NAT_TRAVERSAL_HPP

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace xipher {
namespace voip {

/**
 * NAT Traversal Manager using libdatachannel (WebRTC)
 * 
 * Handles ICE (Interactive Connectivity Establishment) for P2P connections:
 * - STUN server discovery
 * - TURN server fallback
 * - ICE candidate gathering
 * - Connection establishment
 */
class NatTraversal {
public:
    struct IceCandidate {
        std::string candidate;
        std::string sdp_mid;
        int sdp_mline_index;
    };
    
    struct Config {
        std::vector<std::string> stun_servers;
        std::vector<std::string> turn_servers = {
            "turn:zuhgyghj:zxcvbnma_zuhgyghj@turn.xipher.pro:3478?transport=udp",
            "turn:zuhgyghj:zxcvbnma_zuhgyghj@turn.xipher.pro:3478?transport=tcp",
            "turns:zuhgyghj:zxcvbnma_zuhgyghj@turn.xipher.pro:5349?transport=tcp"
        };  // Format: "turn:username:password@server:port?transport=udp|tcp|tls"
        bool enable_ipv6 = true;
    };
    
    using OnIceCandidateCallback = std::function<void(const IceCandidate& candidate)>;
    using OnConnectionStateCallback = std::function<void(const std::string& state)>;
    using OnAudioFrameCallback = std::function<void(const uint8_t* data, size_t size)>;
    using OnLocalDescriptionCallback = std::function<void(const std::string& sdp, const std::string& type)>;
    
    NatTraversal();
    ~NatTraversal();
    
    /**
     * Initialize NAT traversal with configuration
     */
    bool initialize();
    bool initialize(const Config& config);
    
    /**
     * Create an offer for WebRTC connection
     * @return SDP offer string
     */
    std::string createOffer();
    
    /**
     * Set remote SDP answer
     */
    bool setRemoteAnswer(const std::string& answer_sdp);
    
    /**
     * Set remote SDP offer (when acting as answerer)
     */
    bool setRemoteOffer(const std::string& offer_sdp);
    
    /**
     * Create an answer to a remote offer
     * @return SDP answer string
     */
    std::string createAnswer();
    
    /**
     * Add remote ICE candidate
     */
    bool addIceCandidate(const IceCandidate& candidate);
    
    /**
     * Set callbacks
     */
    void setOnIceCandidate(OnIceCandidateCallback callback);
    void setOnConnectionState(OnConnectionStateCallback callback);
    void setOnAudioFrame(OnAudioFrameCallback callback);
    void setOnLocalDescription(OnLocalDescriptionCallback callback);
    
    /**
     * Send data over the data channel
     */
    bool sendAudioFrame(const uint8_t* data, size_t size, double timestampSeconds);
    
    /**
     * Get current connection state
     * States: "new", "connecting", "connected", "disconnected", "failed", "closed"
     */
    std::string getConnectionState() const;
    
    /**
     * Close the connection
     */
    void close();
    void reset();

private:
    Config config_;
    std::shared_ptr<void> peer_connection_;
    std::shared_ptr<void> audio_track_;
    struct ClosingEntry {
        uint64_t generation = 0;
        std::shared_ptr<void> peer;
        std::shared_ptr<void> track;
        bool closed = false;
        std::chrono::steady_clock::time_point closed_at{};
    };
    std::vector<ClosingEntry> closing_peers_;

    OnIceCandidateCallback on_ice_candidate_;
    OnConnectionStateCallback on_connection_state_;
    OnAudioFrameCallback on_audio_frame_;
    OnLocalDescriptionCallback on_local_description_;
    
    std::string connection_state_;
    mutable std::mutex mutex_;
    std::atomic<bool> closing_{false};
    std::atomic<uint64_t> generation_{0};
    std::atomic<uint64_t> active_generation_{0};
    
    bool initializeLibDataChannel(uint64_t generation);
    bool setupAudioTrack();
    void cleanup();
    bool isActiveGeneration(uint64_t generation) const;
    void markClosed(uint64_t generation);
    void cleanupClosingPeers();
};

} // namespace voip
} // namespace xipher

#endif // NAT_TRAVERSAL_HPP
