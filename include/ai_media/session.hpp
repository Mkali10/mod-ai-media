#pragma once
#include "ai_media/pcm_ring_buffer.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class WebSocketClient;

namespace ai_media {
class Session : public std::enable_shared_from_this<Session> {
public:
    static std::shared_ptr<Session> create(std::string uuid, std::string websocket_url,
        std::uint32_t input_sample_rate, std::uint32_t output_sample_rate,
        std::string metadata, std::size_t playback_capacity_bytes);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    bool connect();
    void disconnect() noexcept;
    void send_caller_audio(const void* data, std::size_t size) noexcept;
    std::size_t read_playback(void* output, std::size_t size) noexcept;
    void clear_playback() noexcept;
    void set_paused(bool paused) noexcept;
    bool connected() const noexcept;
    bool paused() const noexcept;
    bool closing() const noexcept;
    std::size_t queued_bytes() const noexcept;
private:
    Session(std::string uuid, std::string websocket_url,
        std::uint32_t input_sample_rate, std::uint32_t output_sample_rate,
        std::string metadata, std::size_t playback_capacity_bytes);
    void bind_callbacks();
    std::string start_message() const;
    std::string uuid_, websocket_url_, metadata_;
    std::uint32_t input_sample_rate_, output_sample_rate_;
    PcmRingBuffer playback_;
    std::unique_ptr<WebSocketClient> client_;
    std::atomic<bool> connected_{false}, paused_{false}, closing_{false};
    std::atomic<std::uint64_t> dropped_bytes_{0};
};
}
