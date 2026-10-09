#pragma once
#include <string_view>
#include <enet/enet.h>
void content_status(ENetEvent& event, const std::string_view text);

/* player commands */
void quests_command(ENetEvent& event, const std::string_view text);       // @note /quests
void achievements_command(ENetEvent& event, const std::string_view text); // @note /achievements
void daily_command(ENetEvent& event, const std::string_view text);        // @note /daily
