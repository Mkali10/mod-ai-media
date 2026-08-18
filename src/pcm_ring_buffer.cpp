#include "ai_media/pcm_ring_buffer.hpp"

#include <algorithm>
#include <stdexcept>

namespace ai_media {

PcmRingBuffer::PcmRingBuffer(std::size_t capacity_bytes)
    : data_(capacity_bytes ? std::make_unique<std::uint8_t[]>(capacity_bytes) : nullptr),
      capacity_(capacity_bytes) {
    if (capacity_bytes < 2) {
        throw std::invalid_argument("ring buffer capacity must be at least 2 bytes");
    }
}

std::size_t PcmRingBuffer::size() const noexcept {
    const auto head = head_.load(std::memory_order_acquire);
    const auto tail = tail_.load(std::memory_order_acquire);
    return head >= tail ? head - tail : capacity_ - (tail - head);
}

std::size_t PcmRingBuffer::capacity() const noexcept { return capacity_ - 1; }

std::size_t PcmRingBuffer::free_space() const noexcept { return capacity() - size(); }

std::size_t PcmRingBuffer::push(const std::uint8_t* input, std::size_t count) noexcept {
    if (!input || count == 0) return 0;
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_acquire);
    const auto used = head >= tail ? head - tail : capacity_ - (tail - head);
    const auto writable = std::min(count, (capacity_ - 1) - used);
    for (std::size_t i = 0; i < writable; ++i) data_[(head + i) % capacity_] = input[i];
    head_.store((head + writable) % capacity_, std::memory_order_release);
    return writable;
}

std::size_t PcmRingBuffer::pop(std::uint8_t* output, std::size_t count) noexcept {
    if (!output || count == 0) return 0;
    const auto tail = tail_.load(std::memory_order_relaxed);
    const auto head = head_.load(std::memory_order_acquire);
    const auto available = head >= tail ? head - tail : capacity_ - (tail - head);
    const auto readable = std::min(count, available);
    for (std::size_t i = 0; i < readable; ++i) output[i] = data_[(tail + i) % capacity_];
    tail_.store((tail + readable) % capacity_, std::memory_order_release);
    return readable;
}

void PcmRingBuffer::clear() noexcept {
    tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
}

}  // namespace ai_media

