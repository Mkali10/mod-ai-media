#include "ai_media/pcm_ring_buffer.hpp"
#include "ai_media/protocol.hpp"

#include <cassert>
#include <cstdint>

int main() {
    ai_media::PcmRingBuffer buffer(9);
    const std::uint8_t input[] = {1, 2, 3, 4, 5, 6};
    std::uint8_t output[6] = {};

    assert(buffer.capacity() == 8);
    assert(buffer.push(input, 6) == 6);
    assert(buffer.size() == 6);
    assert(buffer.pop(output, 4) == 4);
    assert(output[0] == 1 && output[3] == 4);
    assert(buffer.push(input, 6) == 6);
    assert(buffer.size() == 8);
    buffer.clear();
    assert(buffer.size() == 0);

    assert(ai_media::parse_control_command(R"({"type":"playback.clear"})") ==
           ai_media::ControlCommand::Clear);
    assert(ai_media::parse_control_command(R"({"type":"unknown"})") ==
           ai_media::ControlCommand::Unknown);
    return 0;
}

