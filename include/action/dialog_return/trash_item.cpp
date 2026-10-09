#include "pch.hpp"
#include "database/custom_content.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include "trash_item.hpp"

void trash_item(ENetEvent& event, const ::hPipe &hPipe)
{
    if (!event.peer) return;
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    const int requested_id = std::atoi(hPipe["itemID"].c_str());
    const int requested_count = std::atoi(hPipe["count"].c_str());
    const bool valid_id = requested_id > 0 &&
        (requested_id < static_cast<int>(items.size()) || custom_content::is_custom_item(requested_id));
    if (!valid_id || requested_count <= 0 || requested_count > 200) return;
    const short itemID = static_cast<short>(requested_id);

    const ::item &item = id_to_item(static_cast<u_short>(requested_id));
    if (item.id != requested_id) return;
    if ((item.cat & CAT_UNTRADEABLE) && hPipe["buttonClicked"] != "recycle_untradeable")
    {
        send_varlist(event.peer, {"OnTextOverlay", "Confirm recycling this untradeable item first."});
        return;
    }
    const auto held = std::ranges::find(pPeer->slots, itemID, &::slot::id);
    if (held == pPeer->slots.end() || requested_count > held->count) return;
    const short count = static_cast<short>(requested_count);

    modify_item_inventory(event, ::slot(itemID, -count));
    on::ConsoleMessage(event.peer, std::format("{} `w{}`` recycled, `w0`` gems earned.", count, item.raw_name));
}
