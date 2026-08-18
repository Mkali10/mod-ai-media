#include "ai_media/protocol.hpp"

namespace ai_media {

ControlCommand parse_control_command(std::string_view json) noexcept {
    if (json.find("playback.clear") != std::string_view::npos) return ControlCommand::Clear;
    if (json.find("playback.pause") != std::string_view::npos) return ControlCommand::Pause;
    if (json.find("playback.resume") != std::string_view::npos) return ControlCommand::Resume;
    if (json.find("playback.stop") != std::string_view::npos) return ControlCommand::Stop;
    if (json.find("\"ping\"") != std::string_view::npos) return ControlCommand::Ping;
    return ControlCommand::Unknown;
}

const char* command_name(ControlCommand command) noexcept {
    switch (command) {
        case ControlCommand::Clear: return "clear";
        case ControlCommand::Pause: return "pause";
        case ControlCommand::Resume: return "resume";
        case ControlCommand::Stop: return "stop";
        case ControlCommand::Ping: return "ping";
        default: return "unknown";
    }
}

}  // namespace ai_media

