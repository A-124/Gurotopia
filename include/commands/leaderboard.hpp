#pragma once
#include <string_view>

/* /leaderboard (also /top, /lb): top players by level and by play time */
extern void command_leaderboard(ENetEvent &event, std::string_view text);
extern void leaderboard_dialog_return(ENetEvent &event, const ::hPipe &pipe);
