#include "pch.hpp"
#include <charconv>
#include "database/item_registry.hpp"
#include "find_item.hpp"

void find_item(ENetEvent& event, const ::hPipe& pipe)
{
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    const std::string button = pipe["buttonClicked"];
    if (!button.starts_with("find_pick_")) return;
    const std::string_view raw(button.data() + 10, button.size() - 10);
    int id{};
    const auto [end, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), id);
    if (ec != std::errc{} || end != raw.data() + raw.size() || id <= 0 ||
        !item_registry::exists(static_cast<u_short>(id))) return;
    modify_item_inventory(event, ::slot(id, 200));
}