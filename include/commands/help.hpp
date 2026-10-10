#pragma once
#include <string_view>

/* /help opens a categorised, role-aware command guide */
extern void command_help(ENetEvent &event, std::string_view text);
extern void help_dialog_return(ENetEvent &event, const ::hPipe &pipe);
