#pragma once
#include <string>
#include <unordered_map>
#include "core/event_bus.hpp"
#include "gameplay/goals.hpp"

/* @brief long-term milestones defined in resources/achievements.txt (same line format as quests, see goals.hpp) */
namespace achievement_system {
using achievement = goals::goal;

bool reload();
const achievement* find(int id) noexcept;
void on_event(const event_bus::event& event);
const std::unordered_map<int, achievement>& all() noexcept;
}
