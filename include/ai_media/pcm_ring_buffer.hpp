#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace ai_media {

class PcmRingBuffer {
public:
    explicit PcmRingBuffer(std::size_t capacity_bytes);

    PcmRingBuffer(const PcmRingBuffer&) = delete;
    PcmRingBuffer& operator=(const PcmRingBuffer&) = delete;

    std::size_t push(const std::uint8_t* data, std::size_t size) noexcept;
    std::size_t pop(std::uint8_t* output, std::size_t size) noexcept;
    void clear() noexcept;

    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;
    std::size_t free_space() const noexcept;

private:
    std::unique_ptr<std::uint8_t[]> data_;
    const std::size_t capacity_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

}  // namespace ai_media

