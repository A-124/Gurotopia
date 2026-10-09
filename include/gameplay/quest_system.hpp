#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include "core/event_bus.hpp"
namespace quest_system {
enum class condition : unsigned char { item_changed, block_changed, player_entered_world };
struct quest { int id{}; std::string name{}; condition trigger{}; int target{}; int required{1}; };
bool reload();
const quest* find(int id) noexcept;
void on_event(const event_bus::event& event);
const std::unordered_map<int, quest>& all() noexcept;
}
