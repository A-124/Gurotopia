#pragma once
#include <string_view>

extern void command_notebook(ENetEvent &event, std::string_view text); // /notebook
extern void command_playtime(ENetEvent &event, std::string_view text); // /playtime
