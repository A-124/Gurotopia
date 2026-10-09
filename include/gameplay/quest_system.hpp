#pragma once
#include <string>
#include <unordered_map>
#include "core/event_bus.hpp"
#include "gameplay/goals.hpp"

/* @brief repeatable-once objectives with rewards, defined in resources/quests.txt (see goals.hpp for the line format) */
namespace quest_system {
using quest = goals::goal;

bool reload();
const quest* find(int id) noexcept;
void on_event(const event_bus::event& event);
const std::unordered_map<int, quest>& all() noexcept;
}
