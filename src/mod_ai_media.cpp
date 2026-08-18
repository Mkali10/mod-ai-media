extern "C" {
#include <switch.h>
}

#include "ai_media/session.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

namespace {
constexpr const char* kBugName = "mod_ai_media";
constexpr const char* kPrivateKey = "mod_ai_media_bug";
constexpr const char* kSyntax = "<uuid> <start <ws-url> [metadata]|stop|clear|pause|resume|status>";
using SessionPtr = std::shared_ptr<ai_media::Session>;

SessionPtr get_session(switch_media_bug_t* bug) {
    if (!bug) return {};
    auto* holder = static_cast<SessionPtr*>(switch_core_media_bug_get_user_data(bug));
    return holder ? *holder : SessionPtr{};
}

switch_bool_t media_callback(switch_media_bug_t* bug, void* user_data, switch_abc_type_t type) {
    auto* holder = static_cast<SessionPtr*>(user_data);
    if (type == SWITCH_ABC_TYPE_CLOSE) {
        if (holder) {
            (*holder)->disconnect();
            delete holder;
        }
        return SWITCH_TRUE;
    }
    if (!holder || !*holder) return SWITCH_TRUE;
    auto session = *holder;
    if (type == SWITCH_ABC_TYPE_READ) {
        switch_frame_t frame = {};
        while (switch_core_media_bug_read(bug, &frame, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
            if (frame.data && frame.datalen) session->send_caller_audio(frame.data, frame.datalen);
        }
    } else if (type == SWITCH_ABC_TYPE_WRITE_REPLACE) {
        switch_frame_t* frame = switch_core_media_bug_get_write_replace_frame(bug);
        if (frame && frame->data && frame->buflen) {
            const std::size_t wanted = std::min<std::size_t>(frame->datalen, frame->buflen);
            const std::size_t got = session->read_playback(frame->data, wanted);
            if (got < wanted) {
                std::memset(static_cast<std::uint8_t*>(frame->data) + got, 0, wanted - got);
            }
            switch_core_media_bug_set_write_replace_frame(bug, frame);
        }
    }
    return SWITCH_TRUE;
}

void start_media(switch_core_session_t* fs_session, const char* url, const char* metadata,
                 switch_stream_handle_t* stream) {
    switch_channel_t* channel = switch_core_session_get_channel(fs_session);
    if (switch_channel_get_private(channel, kPrivateKey)) {
        stream->write_function(stream, "-ERR already started\n");
        return;
    }
    if (!url || std::strncmp(url, "wss://", 6) != 0) {
        stream->write_function(stream, "-ERR ws-url must begin with wss://\n");
        return;
    }
    if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
        stream->write_function(stream, "-ERR channel could not be pre-answered\n");
        return;
    }
    switch_codec_implementation_t read_impl = {};
    switch_codec_implementation_t write_impl = {};
    switch_core_session_get_read_impl(fs_session, &read_impl);
    switch_core_session_get_write_impl(fs_session, &write_impl);
    if (!read_impl.actual_samples_per_second || !write_impl.actual_samples_per_second) {
        stream->write_function(stream, "-ERR channel codecs unavailable\n");
        return;
    }
    const std::uint32_t input_rate = read_impl.actual_samples_per_second;
    const std::uint32_t output_rate = write_impl.actual_samples_per_second;
    const std::size_t capacity = static_cast<std::size_t>(output_rate) * 2U * 2U;
    auto session = ai_media::Session::create(switch_core_session_get_uuid(fs_session), url,
        input_rate, output_rate, metadata ? metadata : "", capacity);
    auto* holder = new SessionPtr(std::move(session));
    switch_media_bug_t* bug = nullptr;
    auto flags = static_cast<switch_media_bug_flag_t>(
        SMBF_READ_STREAM | SMBF_READ_PING | SMBF_WRITE_REPLACE);
    if (switch_core_media_bug_add(fs_session, kBugName, nullptr, media_callback, holder, 0,
                                  flags, &bug) != SWITCH_STATUS_SUCCESS) {
        delete holder;
        stream->write_function(stream, "-ERR could not attach media bug\n");
        return;
    }
    switch_channel_set_private(channel, kPrivateKey, bug);
    if (!(*holder)->connect()) {
        switch_channel_set_private(channel, kPrivateKey, nullptr);
        switch_core_media_bug_remove(fs_session, &bug);
        stream->write_function(stream, "-ERR WebSocket connection could not start\n");
        return;
    }
    stream->write_function(stream, "+OK started input=%uHz output=%uHz pcm_s16le\n",
                           input_rate, output_rate);
}

void stop_media(switch_core_session_t* fs_session, switch_stream_handle_t* stream) {
    switch_channel_t* channel = switch_core_session_get_channel(fs_session);
    auto* bug = static_cast<switch_media_bug_t*>(switch_channel_get_private(channel, kPrivateKey));
    if (!bug) {
        stream->write_function(stream, "-ERR not started\n");
        return;
    }
    auto session = get_session(bug);
    if (session) session->disconnect();
    switch_channel_set_private(channel, kPrivateKey, nullptr);
    switch_core_media_bug_remove(fs_session, &bug);
    stream->write_function(stream, "+OK stopped\n");
}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_ai_media_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ai_media_shutdown);
SWITCH_MODULE_DEFINITION(mod_ai_media, mod_ai_media_load, mod_ai_media_shutdown, nullptr);

SWITCH_STANDARD_API(uuid_ai_media_api) {
    if (!cmd || !*cmd) {
        stream->write_function(stream, "-USAGE uuid_ai_media %s\n", kSyntax);
        return SWITCH_STATUS_SUCCESS;
    }
    char* command = strdup(cmd);
    char* argv[4] = {};
    const int argc = switch_separate_string(command, ' ', argv, 4);
    if (argc < 2) {
        free(command);
        stream->write_function(stream, "-USAGE uuid_ai_media %s\n", kSyntax);
        return SWITCH_STATUS_SUCCESS;
    }
    switch_core_session_t* fs_session = switch_core_session_locate(argv[0]);
    if (!fs_session) {
        free(command);
        stream->write_function(stream, "-ERR session not found\n");
        return SWITCH_STATUS_SUCCESS;
    }
    switch_channel_t* channel = switch_core_session_get_channel(fs_session);
    auto* bug = static_cast<switch_media_bug_t*>(switch_channel_get_private(channel, kPrivateKey));
    const std::string action(argv[1]);
    if (action == "start") {
        start_media(fs_session, argc > 2 ? argv[2] : nullptr, argc > 3 ? argv[3] : nullptr, stream);
    } else if (action == "stop") {
        stop_media(fs_session, stream);
    } else if (!bug) {
        stream->write_function(stream, "-ERR not started\n");
    } else {
        auto session = get_session(bug);
        if (!session) stream->write_function(stream, "-ERR session state unavailable\n");
        else if (action == "clear") { session->clear_playback(); stream->write_function(stream, "+OK playback cleared\n"); }
        else if (action == "pause") { session->set_paused(true); session->clear_playback(); stream->write_function(stream, "+OK paused\n"); }
        else if (action == "resume") { session->set_paused(false); stream->write_function(stream, "+OK resumed\n"); }
        else if (action == "status") stream->write_function(stream,
            "+OK connected=%s paused=%s queued_bytes=%zu\n",
            session->connected() ? "true" : "false", session->paused() ? "true" : "false",
            session->queued_bytes());
        else stream->write_function(stream, "-USAGE uuid_ai_media %s\n", kSyntax);
    }
    switch_core_session_rwunlock(fs_session);
    free(command);
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_ai_media_load) {
    switch_api_interface_t* api_interface = nullptr;
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(api_interface, "uuid_ai_media", "Bidirectional raw PCM media",
                   uuid_ai_media_api, kSyntax);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_ai_media loaded\n");
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ai_media_shutdown) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_ai_media unloaded\n");
    return SWITCH_STATUS_SUCCESS;
}
