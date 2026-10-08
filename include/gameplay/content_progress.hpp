#pragma once
#include <string>
#include <unordered_map>
#include "core/event_bus.hpp"
namespace content_progress {
struct player_state { std::unordered_map<int,int> quest_progress; std::unordered_map<int,bool> achievements; };
void clear();
void on_event(const event_bus::event& event);
int progress(int quest_id) noexcept;
bool completed(int achievement_id) noexcept;
}
