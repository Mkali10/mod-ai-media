#include "ai_media/session.hpp"
#include "ai_media/protocol.hpp"
#include "WebSocketClient.h"
#include <sstream>
#include <utility>

namespace ai_media {
namespace {
std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const unsigned char c : value) {
        switch (c) {
            case '\"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    return out.str();
}
}

std::shared_ptr<Session> Session::create(std::string uuid, std::string url,
    std::uint32_t input_rate, std::uint32_t output_rate,
    std::string metadata, std::size_t capacity) {
    auto value = std::shared_ptr<Session>(new Session(std::move(uuid), std::move(url),
        input_rate, output_rate, std::move(metadata), capacity));
    value->bind_callbacks();
    return value;
}

Session::Session(std::string uuid, std::string url, std::uint32_t input_rate,
    std::uint32_t output_rate, std::string metadata, std::size_t capacity)
    : uuid_(std::move(uuid)), websocket_url_(std::move(url)), metadata_(std::move(metadata)),
      input_sample_rate_(input_rate), output_sample_rate_(output_rate), playback_(capacity),
      client_(std::make_unique<WebSocketClient>()) {
    client_->setUrl(websocket_url_);
    client_->enableCompression(false);
}

Session::~Session() { disconnect(); }

void Session::bind_callbacks() {
    std::weak_ptr<Session> weak = shared_from_this();
    client_->setOpenCallback([weak]() {
        if (auto self = weak.lock()) {
            self->connected_.store(true, std::memory_order_release);
            auto message = self->start_message();
            self->client_->sendMessage(message.data(), message.size());
        }
    });
    client_->setBinaryCallback([weak](const void* data, std::size_t size) {
        if (auto self = weak.lock()) {
            if (!data || !size || (size % 2) || self->closing() || self->paused()) return;
            auto written = self->playback_.push(static_cast<const std::uint8_t*>(data), size);
            self->dropped_bytes_.fetch_add(size - written, std::memory_order_relaxed);
        }
    });
    client_->setMessageCallback([weak](const std::string& message) {
        if (auto self = weak.lock()) {
            switch (parse_control_command(message)) {
                case ControlCommand::Clear: self->clear_playback(); break;
                case ControlCommand::Pause: self->set_paused(true); break;
                case ControlCommand::Resume: self->set_paused(false); break;
                case ControlCommand::Stop: self->disconnect(); break;
                default: break;
            }
        }
    });
    client_->setCloseCallback([weak](int, const std::string&) {
        if (auto self = weak.lock()) self->connected_.store(false, std::memory_order_release);
    });
    client_->setErrorCallback([weak](int, const std::string&) {
        if (auto self = weak.lock()) self->connected_.store(false, std::memory_order_release);
    });
}

bool Session::connect() { if (closing()) return false; client_->connect(); return true; }
void Session::disconnect() noexcept {
    if (closing_.exchange(true, std::memory_order_acq_rel)) return;
    connected_.store(false, std::memory_order_release);
    client_->setBinaryCallback({}); client_->setMessageCallback({});
    client_->setOpenCallback({}); client_->setCloseCallback({}); client_->setErrorCallback({});
    client_->disconnect(); clear_playback();
}
void Session::send_caller_audio(const void* data, std::size_t size) noexcept {
    if (connected() && !closing() && !paused() && data && size)
        client_->sendBinary(static_cast<std::uint8_t*>(const_cast<void*>(data)), size);
}
std::size_t Session::read_playback(void* output, std::size_t size) noexcept {
    return (!output || !size || paused() || closing()) ? 0 :
        playback_.pop(static_cast<std::uint8_t*>(output), size);
}
void Session::clear_playback() noexcept { playback_.clear(); }
void Session::set_paused(bool value) noexcept { paused_.store(value, std::memory_order_release); }
bool Session::connected() const noexcept { return connected_.load(std::memory_order_acquire); }
bool Session::paused() const noexcept { return paused_.load(std::memory_order_acquire); }
bool Session::closing() const noexcept { return closing_.load(std::memory_order_acquire); }
std::size_t Session::queued_bytes() const noexcept { return playback_.size(); }

std::string Session::start_message() const {
    std::ostringstream out;
    out << "{\"type\":\"start\",\"version\":1,\"callId\":\"" << json_escape(uuid_)
        << "\",\"input\":{\"encoding\":\"pcm_s16le\",\"sampleRate\":" << input_sample_rate_
        << ",\"channels\":1,\"frameDurationMs\":20},\"output\":{\"encoding\":\"pcm_s16le\","
        << "\"sampleRate\":" << output_sample_rate_ << ",\"channels\":1},\"metadata\":\""
        << json_escape(metadata_) << "\"}";
    return out.str();
}
}
