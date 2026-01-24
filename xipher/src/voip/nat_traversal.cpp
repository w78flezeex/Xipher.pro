#include "../../include/voip/nat_traversal.hpp"
#include "../../include/utils/logger.hpp"
#include <rtc/rtc.hpp>
#include <chrono>
#include <random>
#include <sstream>

namespace xipher {
namespace voip {

NatTraversal::NatTraversal()
    : connection_state_("new") {
}

NatTraversal::~NatTraversal() {
    close();
    cleanup();
}

bool NatTraversal::initialize(const Config& config) {
    const uint64_t generation = generation_.fetch_add(1) + 1;
    cleanupClosingPeers();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        closing_.store(false);
        active_generation_.store(generation);
        connection_state_ = "new";
    }
    
    // Initialize libdatachannel
    if (!initializeLibDataChannel(generation)) {
        active_generation_.store(0);
        Logger::getInstance().error("Failed to initialize libdatachannel");
        return false;
    }
    
    Logger::getInstance().info("NAT Traversal initialized: " + 
                              std::to_string(config_.stun_servers.size()) + " STUN servers, " +
                              std::to_string(config_.turn_servers.size()) + " TURN servers");
    
    return true;
}

bool NatTraversal::initialize() {
    return initialize(Config{});
}

bool NatTraversal::isActiveGeneration(uint64_t generation) const {
    return active_generation_.load() == generation;
}

void NatTraversal::markClosed(uint64_t generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : closing_peers_) {
        if (entry.generation == generation) {
            entry.closed = true;
            entry.closed_at = std::chrono::steady_clock::now();
            break;
        }
    }
}

void NatTraversal::cleanupClosingPeers() {
    constexpr auto kReleaseDelay = std::chrono::seconds(2);
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = closing_peers_.begin();
    while (it != closing_peers_.end()) {
        if (it->closed && (now - it->closed_at) >= kReleaseDelay) {
            it = closing_peers_.erase(it);
        } else {
            ++it;
        }
    }
}

bool NatTraversal::initializeLibDataChannel(uint64_t generation) {
    try {
        // Initialize libdatachannel
        rtc::InitLogger(rtc::LogLevel::Info);
        
        // Create peer connection configuration
        rtc::Configuration rtc_config;
        
        // Add STUN servers
        for (const auto& stun_url : config_.stun_servers) {
            rtc_config.iceServers.emplace_back(stun_url);
            Logger::getInstance().info("Added STUN server: " + stun_url);
        }
        
        // Add TURN servers
        for (const auto& turn_url : config_.turn_servers) {
            rtc_config.iceServers.emplace_back(turn_url);
            Logger::getInstance().info("Added TURN server: " + turn_url);
        }
        
        // Create peer connection
        auto pc = std::make_shared<rtc::PeerConnection>(rtc_config);
        
        // Set callbacks
        pc->onLocalDescription([this, generation](rtc::Description description) {
            if (!isActiveGeneration(generation)) {
                return;
            }
            OnLocalDescriptionCallback callback;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                callback = on_local_description_;
            }
            if (callback) {
                const std::string sdp = description.generateSdp();
                callback(sdp, description.typeString());
            }
        });
        
        pc->onLocalCandidate([this, generation](rtc::Candidate candidate) {
            if (!isActiveGeneration(generation)) {
                return;
            }
            OnIceCandidateCallback callback;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                callback = on_ice_candidate_;
            }
            if (callback) {
                IceCandidate ice_candidate;
                ice_candidate.candidate = candidate.candidate();
                ice_candidate.sdp_mid = candidate.mid();
                ice_candidate.sdp_mline_index = candidate.sdpMLineIndex();
                callback(ice_candidate);
            }
        });
        
        pc->onStateChange([this, generation](rtc::PeerConnection::State state) {
            // Connection state changed
            std::string state_str;
            switch (state) {
                case rtc::PeerConnection::State::New:
                    state_str = "new";
                    break;
                case rtc::PeerConnection::State::Connecting:
                    state_str = "connecting";
                    break;
                case rtc::PeerConnection::State::Connected:
                    state_str = "connected";
                    break;
                case rtc::PeerConnection::State::Disconnected:
                    state_str = "disconnected";
                    break;
                case rtc::PeerConnection::State::Failed:
                    state_str = "failed";
                    break;
                case rtc::PeerConnection::State::Closed:
                    state_str = "closed";
                    break;
            }
            if (isActiveGeneration(generation)) {
                OnConnectionStateCallback callback;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    connection_state_ = state_str;
                    callback = on_connection_state_;
                }
                if (callback) {
                    callback(state_str);
                }
                Logger::getInstance().info("Peer connection state: " + state_str);
            } else if (state == rtc::PeerConnection::State::Closed) {
                markClosed(generation);
            }
        });
        
        pc->onTrack([this, generation](std::shared_ptr<rtc::Track> track) {
            if (!isActiveGeneration(generation)) {
                return;
            }
            if (!track) {
                return;
            }
            if (track->description().type() != "audio") {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (audio_track_) {
                    auto localTrack = std::static_pointer_cast<rtc::Track>(audio_track_);
                    if (localTrack && localTrack.get() == track.get()) {
                        return;
                    }
                }
            }
            auto depacketizer = std::make_shared<rtc::OpusRtpDepacketizer>();
            track->setMediaHandler(depacketizer);
            track->onFrame([this, generation](rtc::binary data, rtc::FrameInfo) {
                if (!isActiveGeneration(generation)) {
                    return;
                }
                OnAudioFrameCallback callback;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    callback = on_audio_frame_;
                }
                if (callback) {
                    callback(data.data(), data.size());
                }
            });
        });

        {
            std::lock_guard<std::mutex> lock(mutex_);
            peer_connection_ = pc;
        }
        if (!setupAudioTrack()) {
            Logger::getInstance().error("Failed to setup audio track");
            return false;
        }

        return true;
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("libdatachannel initialization error: " + std::string(e.what()));
        return false;
    }
}

std::string NatTraversal::createOffer() {
    if (closing_.load()) {
        return "";
    }
    const uint64_t generation = active_generation_.load();
    if (!isActiveGeneration(generation)) {
        return "";
    }
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::static_pointer_cast<rtc::PeerConnection>(peer_connection_);
    }
    if (!pc || !isActiveGeneration(generation)) {
        Logger::getInstance().error("Peer connection not initialized");
        return "";
    }
    
    try {
        std::shared_ptr<rtc::Track> track;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            track = std::static_pointer_cast<rtc::Track>(audio_track_);
        }
        if (!track) {
            if (!setupAudioTrack()) {
                Logger::getInstance().error("Failed to setup audio track for offer");
                return "";
            }
        }
        // Create offer
        pc->setLocalDescription();
        return "";

    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to create offer: " + std::string(e.what()));
        return "";
    }
}

bool NatTraversal::setRemoteAnswer(const std::string& answer_sdp) {
    if (closing_.load()) {
        return false;
    }
    const uint64_t generation = active_generation_.load();
    if (!isActiveGeneration(generation)) {
        return false;
    }
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::static_pointer_cast<rtc::PeerConnection>(peer_connection_);
    }
    if (!pc || !isActiveGeneration(generation)) {
        return false;
    }
    
    try {
        rtc::Description description(answer_sdp, rtc::Description::Type::Answer);
        pc->setRemoteDescription(description);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to set remote answer: " + std::string(e.what()));
        return false;
    }
}

bool NatTraversal::setRemoteOffer(const std::string& offer_sdp) {
    if (closing_.load()) {
        return false;
    }
    const uint64_t generation = active_generation_.load();
    if (!isActiveGeneration(generation)) {
        return false;
    }
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::static_pointer_cast<rtc::PeerConnection>(peer_connection_);
    }
    if (!pc || !isActiveGeneration(generation)) {
        return false;
    }
    
    try {
        rtc::Description description(offer_sdp, rtc::Description::Type::Offer);
        pc->setRemoteDescription(description);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to set remote offer: " + std::string(e.what()));
        return false;
    }
}

std::string NatTraversal::createAnswer() {
    if (closing_.load()) {
        return "";
    }
    const uint64_t generation = active_generation_.load();
    if (!isActiveGeneration(generation)) {
        return "";
    }
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::static_pointer_cast<rtc::PeerConnection>(peer_connection_);
    }
    if (!pc || !isActiveGeneration(generation)) {
        return "";
    }
    
    try {
        std::shared_ptr<rtc::Track> track;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            track = std::static_pointer_cast<rtc::Track>(audio_track_);
        }
        if (!track) {
            if (!setupAudioTrack()) {
                Logger::getInstance().error("Failed to setup audio track for answer");
                return "";
            }
        }
        pc->setLocalDescription();
        return "";
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to create answer: " + std::string(e.what()));
        return "";
    }
}

bool NatTraversal::addIceCandidate(const IceCandidate& candidate) {
    if (closing_.load()) {
        return false;
    }
    const uint64_t generation = active_generation_.load();
    if (!isActiveGeneration(generation)) {
        return false;
    }
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::static_pointer_cast<rtc::PeerConnection>(peer_connection_);
    }
    if (!pc || !isActiveGeneration(generation)) {
        return false;
    }
    
    try {
        rtc::Candidate rtc_candidate(candidate.candidate, candidate.sdp_mid);
        pc->addRemoteCandidate(rtc_candidate);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to add ICE candidate: " + std::string(e.what()));
        return false;
    }
}

void NatTraversal::setOnIceCandidate(OnIceCandidateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_ice_candidate_ = callback;
}

void NatTraversal::setOnConnectionState(OnConnectionStateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_connection_state_ = callback;
}

void NatTraversal::setOnAudioFrame(OnAudioFrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_audio_frame_ = callback;
}

void NatTraversal::setOnLocalDescription(OnLocalDescriptionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_local_description_ = callback;
}

bool NatTraversal::sendAudioFrame(const uint8_t* data, size_t size, double timestampSeconds) {
    if (closing_.load()) {
        return false;
    }
    const uint64_t generation = active_generation_.load();
    if (!isActiveGeneration(generation)) {
        return false;
    }
    std::shared_ptr<rtc::Track> track;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        track = std::static_pointer_cast<rtc::Track>(audio_track_);
    }
    if (!track || !isActiveGeneration(generation)) {
        return false;
    }

    try {
        if (!track->isOpen()) {
            return false;
        }
        rtc::binary payload(data, data + size);
        rtc::FrameInfo frameInfo{std::chrono::duration<double>(timestampSeconds)};
        track->sendFrame(std::move(payload), frameInfo);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to send audio frame: " + std::string(e.what()));
        return false;
    }
}

std::string NatTraversal::getConnectionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_state_;
}

void NatTraversal::close() {
    closing_.store(true);
    const uint64_t closing_generation = active_generation_.exchange(0);
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::Track> track;
    cleanupClosingPeers();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::static_pointer_cast<rtc::PeerConnection>(peer_connection_);
        track = std::static_pointer_cast<rtc::Track>(audio_track_);
        peer_connection_.reset();
        audio_track_.reset();
        connection_state_ = "closed";
        if (pc && closing_generation != 0) {
            ClosingEntry entry;
            entry.generation = closing_generation;
            entry.peer = pc;
            entry.track = track;
            closing_peers_.push_back(std::move(entry));
        }
    }
    if (pc) {
        try {
            pc->close();
        } catch (...) {
            // Ignore errors during close
        }
    }
}

void NatTraversal::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    audio_track_.reset();
    peer_connection_.reset();
    closing_peers_.clear();
    active_generation_.store(0);
}

bool NatTraversal::setupAudioTrack() {
    const uint64_t generation = active_generation_.load();
    if (!isActiveGeneration(generation)) {
        return false;
    }
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::static_pointer_cast<rtc::PeerConnection>(peer_connection_);
    }
    if (!pc || !isActiveGeneration(generation)) {
        return false;
    }

    const rtc::SSRC ssrc = static_cast<rtc::SSRC>(std::random_device{}());
    const std::string cname = "xipher-audio";
    const std::string msid = "xipher";
    rtc::Description::Audio audio("audio", rtc::Description::Direction::SendRecv);
    audio.addOpusCodec(111);
    audio.addSSRC(ssrc, cname, msid, cname);

    auto track = pc->addTrack(audio);
    if (!track) {
        return false;
    }

    auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
        ssrc, cname, 111, rtc::OpusRtpPacketizer::DefaultClockRate);
    auto packetizer = std::make_shared<rtc::OpusRtpPacketizer>(rtpConfig);
    auto srReporter = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
    packetizer->addToChain(srReporter);
    auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
    packetizer->addToChain(nackResponder);
    auto depacketizer = std::make_shared<rtc::OpusRtpDepacketizer>();
    packetizer->addToChain(depacketizer);

    track->setMediaHandler(packetizer);
    track->onOpen([]() {
        Logger::getInstance().info("Audio track opened");
    });
    track->onFrame([this, generation](rtc::binary data, rtc::FrameInfo) {
        if (!isActiveGeneration(generation)) {
            return;
        }
        OnAudioFrameCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = on_audio_frame_;
        }
        if (callback) {
            callback(data.data(), data.size());
        }
    });

    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_track_ = track;
    }
    return true;
}

void NatTraversal::reset() {
    close();
}

} // namespace voip
} // namespace xipher
