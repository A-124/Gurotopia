#pragma once
#include <string_view>
#include <enet/enet.h>
void content_status(ENetEvent& event, const std::string_view text);
void quests_command(ENetEvent& event, const std::string_view text);
void achievements_command(ENetEvent& event, const std::string_view text);
void daily_command(ENetEvent& event, const std::string_view text);
void features_command(ENetEvent& event, const std::string_view text);
void craft_dialog_command(ENetEvent& event, const std::string_view text);
void content_dialog_command(ENetEvent& event, const std::string_view text);
void handle_content_dialog_return(ENetEvent& event, const ::hPipe& pipe);
