#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "trash.hpp"

void action::trash(ENetEvent& event, const std::string& header)
{
    const std::string &itemID = readch(header, '|')[4];

    const ::item &item = id_to_item(atoi(itemID.c_str()));

    if (item.type == type::FIST || item.type == type::WRENCH)
    {
        send_varlist(event.peer, { "OnTextOverlay", "You'd be sorry if you lost that!" });
        return;
    }
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    for (const ::slot &slot : pPeer->slots)
        if (slot.id == item.id)
        {
            auto dialog = create_dialog()
                    .set_default_color("`o")
                    .add_label_with_icon("big", std::format("`4Recycle`` `w{}``", item.raw_name), slot.id)
                    .add_textbox(std::format("How many to `4destroy``? (you have {})", slot.count))
                    .add_text_input("count", "", 0, 5)
                    .embed_data("itemID", slot.id);
            if (item.cat & CAT_UNTRADEABLE)
                dialog.add_textbox("`4This item is marked untradeable. Confirm below if you still want to recycle it.``");
            if (item.cat & CAT_UNTRADEABLE)
                dialog.add_button("recycle_untradeable", "Recycle Anyway");
            send_varlist(event.peer, {"OnDialogRequest", dialog.end_dialog("trash_item")});
            return;
        }
}
