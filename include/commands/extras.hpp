#pragma once
#include <string_view>

/* small quality-of-life commands */
extern void command_flip(ENetEvent&, std::string_view);    // /flip           coin flip shown above your head
extern void command_ping(ENetEvent&, std::string_view);    // /ping           your connection latency
extern void command_uptime(ENetEvent&, std::string_view);  // /uptime         how long the server has been running
extern void command_count(ENetEvent&, std::string_view);   // /count          players online and in your world

extern void command_lsb(ENetEvent&, std::string_view); // /lsb legendary-item global broadcast with cooldown

extern void command_scanitem(ENetEvent&, std::string_view); // /scanitem configures and scans online inventories
