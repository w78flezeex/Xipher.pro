#ifndef RTC_RTC_HPP
#define RTC_RTC_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <chrono>
#include <cstdint>

namespace rtc {

using binary = std::vector<uint8_t>;
using message_variant = std::variant<std::string, binary>;
using SSRC = uint32_t;

enum class LogLevel { Info };

inline void InitLogger(LogLevel) {}

struct Candidate {
    Candidate() = default;
    Candidate(const std::string& cand, const std::string& mid)
        : cand_(cand), mid_(mid) {}

    std::string candidate() const { return cand_; }
    std::string mid() const { return mid_; }
    int sdpMLineIndex() const { return 0; }

private:
    std::string cand_;
    std::string mid_;
};

struct Description {
    enum class Type { Offer, Answer };
    enum class Direction { SendRecv, SendOnly, RecvOnly, Inactive };

    struct Audio {
        Audio(const std::string&, Direction) {}
        void addOpusCodec(int) {}
        void addSSRC(SSRC, const std::string&, const std::string&, const std::string&) {}
    };

    Description() = default;
    Description(const std::string& sdp, Type type) : sdp_(sdp), type_(type) {}

    std::string generateSdp() const { return sdp_; }
    std::string typeString() const { return type_ == Type::Offer ? "offer" : "answer"; }

private:
    std::string sdp_;
    Type type_ = Type::Offer;
};

struct Configuration {
    std::vector<std::string> iceServers;
};

struct TrackDescription {
    explicit TrackDescription(std::string type = "audio") : type_(std::move(type)) {}
    std::string type() const { return type_; }

private:
    std::string type_;
};

struct FrameInfo {
    explicit FrameInfo(std::chrono::duration<double>) {}
};

class MediaHandler {
public:
    virtual ~MediaHandler() = default;
};

class RtpPacketizationConfig {
public:
    RtpPacketizationConfig(SSRC, const std::string&, int, int) {}
};

class OpusRtpPacketizer : public MediaHandler {
public:
    static constexpr int DefaultClockRate = 48000;
    explicit OpusRtpPacketizer(const std::shared_ptr<RtpPacketizationConfig>&) {}
    void addToChain(const std::shared_ptr<MediaHandler>&) {}
};

class RtcpSrReporter : public MediaHandler {
public:
    explicit RtcpSrReporter(const std::shared_ptr<RtpPacketizationConfig>&) {}
};

class RtcpNackResponder : public MediaHandler {
};

class OpusRtpDepacketizer : public MediaHandler {
};

class Track {
public:
    TrackDescription description() const { return TrackDescription(); }
    void setMediaHandler(const std::shared_ptr<MediaHandler>&) {}
    void onFrame(std::function<void(binary, FrameInfo)> cb) { on_frame_ = std::move(cb); }
    void onOpen(std::function<void()> cb) {
        on_open_ = std::move(cb);
        if (on_open_) {
            on_open_();
        }
    }
    bool isOpen() const { return true; }
    void sendFrame(binary, FrameInfo) {}

private:
    std::function<void(binary, FrameInfo)> on_frame_;
    std::function<void()> on_open_;
};

class DataChannel {
public:
    void onMessage(std::function<void(message_variant)> cb) { on_message_ = std::move(cb); }
    void onOpen(std::function<void()> cb) { on_open_ = std::move(cb); }
    void send(const binary&) {}

private:
    std::function<void(message_variant)> on_message_;
    std::function<void()> on_open_;
};

class PeerConnection {
public:
    enum class State { New, Connecting, Connected, Disconnected, Failed, Closed };

    explicit PeerConnection(const Configuration&) {}

    void onLocalDescription(std::function<void(Description)> cb) { on_local_description_ = std::move(cb); }
    void onLocalCandidate(std::function<void(Candidate)> cb) { on_local_candidate_ = std::move(cb); }
    void onStateChange(std::function<void(State)> cb) { on_state_change_ = std::move(cb); }
    void onDataChannel(std::function<void(std::shared_ptr<DataChannel>)> cb) { on_data_channel_ = std::move(cb); }
    void onTrack(std::function<void(std::shared_ptr<Track>)> cb) { on_track_ = std::move(cb); }

    std::shared_ptr<DataChannel> createDataChannel(const std::string&) {
        auto dc = std::make_shared<DataChannel>();
        if (on_data_channel_) {
            on_data_channel_(dc);
        }
        return dc;
    }

    std::shared_ptr<Track> addTrack(const Description::Audio&) {
        auto track = std::make_shared<Track>();
        return track;
    }

    void setLocalDescription() {}
    void setRemoteDescription(const Description&) {}
    void addRemoteCandidate(const Candidate&) {}
    void close() {}

private:
    std::function<void(Description)> on_local_description_;
    std::function<void(Candidate)> on_local_candidate_;
    std::function<void(State)> on_state_change_;
    std::function<void(std::shared_ptr<DataChannel>)> on_data_channel_;
    std::function<void(std::shared_ptr<Track>)> on_track_;
};

} // namespace rtc

#endif // RTC_RTC_HPP
