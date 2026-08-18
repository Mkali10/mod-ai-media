extern "C" {
#include <switch.h>
}

SWITCH_MODULE_LOAD_FUNCTION(mod_ai_media_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ai_media_shutdown);
SWITCH_MODULE_DEFINITION(mod_ai_media, mod_ai_media_load, mod_ai_media_shutdown, nullptr);

SWITCH_STANDARD_API(uuid_ai_media_api) {
    if (!cmd || !*cmd) {
        stream->write_function(stream, "-USAGE uuid_ai_media <uuid> <start|stop|clear|pause|resume|status> [args]\n");
        return SWITCH_STATUS_SUCCESS;
    }

    stream->write_function(
        stream,
        "-ERR mod_ai_media milestone-0 scaffold: media actions are not implemented yet\n"
    );
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_ai_media_load) {
    switch_api_interface_t* api_interface = nullptr;
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(
        api_interface,
        "uuid_ai_media",
        "Bidirectional raw audio control",
        uuid_ai_media_api,
        "<uuid> <start|stop|clear|pause|resume|status> [args]"
    );
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_ai_media milestone-0 loaded (media disabled)\n");
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ai_media_shutdown) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_ai_media unloaded\n");
    return SWITCH_STATUS_SUCCESS;
}

