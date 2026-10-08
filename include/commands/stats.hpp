#pragma once

#include <string_view>

/* command handler for /online and /on */
extern void stats_command(ENetEvent& event, const std::string_view text);