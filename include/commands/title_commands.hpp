#pragma once
#include <string_view>

extern void command_titles(ENetEvent &event, std::string_view text);     // /titles
extern void command_givetitle(ENetEvent &event, std::string_view text);  // /givetitle <player|me> <id>  (developer)
extern void command_playtime(ENetEvent &event, std::string_view text);   // /playtime
extern void command_notebook(ENetEvent &event, std::string_view text);   // /notebook
