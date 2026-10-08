#include "pch.hpp"
#include "item_registry.hpp"
#include "custom_content.hpp"
namespace item_registry {
const ::item* find(u_short id) noexcept { return id < items.size() ? &items[id] : nullptr; }
bool exists(u_short id) noexcept { return id < items.size() || custom_content::is_custom_item(id); }
std::size_t size() noexcept { return items.size() + custom_content::items().size(); }
}
