#pragma once

#include <string_view>

namespace ai_media {

enum class ControlCommand { Clear, Pause, Resume, Stop, Ping, Unknown };

ControlCommand parse_control_command(std::string_view json) noexcept;
const char* command_name(ControlCommand command) noexcept;

}  // namespace ai_media

