#pragma once

#include <ctime>
#include <string_view>

/* Event state */
struct event_state
{
    bool active{ false };
    float gem_mult{ 1.0f };
    float xp_mult{ 1.0f };
    std::time_t end_time{ 0 };
};

extern event_state g_event_state;

/* Commands */
extern void event_start_command(ENetEvent& event, const std::string_view text);
extern void event_stop_command(ENetEvent& event, const std::string_view text);
extern void event_show_command(ENetEvent& event, const std::string_view text);

/* Multiplier getters - call these from gem/xp reward code */
extern float get_gem_multiplier();
extern float get_xp_multiplier();
extern bool  is_event_active();

/* Timer - call this from the server main loop */
extern void event_manager_tick();