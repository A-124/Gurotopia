#pragma once
#include <string_view>

/* staff and quality-of-life tools */
extern void command_mute(ENetEvent&, std::string_view);    // /mute <player> [minutes]   (moderator)
extern void command_unmute(ENetEvent&, std::string_view);  // /unmute <player>           (moderator)
extern void command_pinfo(ENetEvent&, std::string_view);   // /pinfo <player>            (moderator)
extern void command_nick(ENetEvent&, std::string_view);    // /nick <name>               (moderator)
extern void command_default(ENetEvent&, std::string_view); // /default                   reset /nick
extern void command_status(ENetEvent&, std::string_view);  // /status                    your own stats
