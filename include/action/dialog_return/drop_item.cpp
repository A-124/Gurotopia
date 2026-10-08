#include "pch.hpp"

#include "drop_item.hpp"

void drop_item(ENetEvent& event, const ::hPipe &hPipe)
{
    if (!event.peer) return;
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    const int requested_id = std::atoi(hPipe["itemID"].c_str());
    const int requested_count = std::atoi(hPipe["count"].c_str());
    if (requested_id <= 0 || requested_id >= static_cast<int>(items.size()) || requested_count <= 0 || requested_count > 200) return;
    const short itemID = static_cast<short>(requested_id);
    const ::item &item = id_to_item(itemID);
    if (item.cat & CAT_UNTRADEABLE)
    {
        send_varlist(event.peer, {"OnTextOverlay", "You can't drop that."});
        return;
    }
    const auto held = std::ranges::find(pPeer->slots, itemID, &::slot::id);
    if (held == pPeer->slots.end() || requested_count > held->count) return;
    const short count = static_cast<short>(requested_count);

    modify_item_inventory(event, ::slot(itemID, -count));

    float x_nabor = (pPeer->facing_left) ? pPeer->pos.x - 32 : pPeer->pos.x + 32; // @note peer's naboring tile (drop position)
    add_drop(event, {itemID, count}, {x_nabor, pPeer->pos.y}, *world);
}
