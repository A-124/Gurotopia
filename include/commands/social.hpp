#pragma once
#include <string_view>

/* player-to-player and world-info commands */
extern void command_msg(ENetEvent&, std::string_view);       // /msg <player> <message>  (also /w)
extern void command_reply(ENetEvent&, std::string_view);     // /r <message>
extern void command_mods(ENetEvent&, std::string_view);      // /mods
extern void command_rules(ENetEvent&, std::string_view);     // /rules  (reads resources/rules.txt when present)
extern void command_worldinfo(ENetEvent&, std::string_view); // /worldinfo
extern void command_roll(ENetEvent&, std::string_view);      // /roll [sides]
