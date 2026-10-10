#pragma once
#include <string_view>
extern void command_setlevel(ENetEvent&, std::string_view);
extern void command_kick(ENetEvent&, std::string_view);
extern void command_ban(ENetEvent&, std::string_view);
extern void command_unban(ENetEvent&, std::string_view);
extern void command_pull(ENetEvent&, std::string_view);
extern bool is_world_banned(std::string_view, int);
/* @return the account name for a user id (online players first, then the database); empty if unknown */
extern std::string username_for_uid(int uid);
