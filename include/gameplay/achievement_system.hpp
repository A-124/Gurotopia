#pragma once
#include <string>
#include <unordered_map>
#include "core/event_bus.hpp"
namespace achievement_system {
struct achievement { int id{}; std::string name{}; event_bus::type trigger{}; int target{}; };
bool reload();
const achievement* find(int id) noexcept;
void on_event(const event_bus::event& event);
const std::unordered_map<int,achievement>& all() noexcept;
}
