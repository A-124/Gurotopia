#include "pch.hpp"
#include "item_registry.hpp"
namespace item_registry {
const ::item* find(u_short id) noexcept { return id < items.size() ? &items[id] : nullptr; }
bool exists(u_short id) noexcept { return id < items.size(); }
std::size_t size() noexcept { return items.size(); }
}
