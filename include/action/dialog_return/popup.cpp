#include "pch.hpp"

#include "popup.hpp"
#include "trade.hpp"
#include "gameplay/content_commands.hpp"

void popup(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    // @note wrench "Trade" button opens the P2P trade window.
    if (hPipe["buttonClicked"] == "trade")
    {
        trade::handle(event, hPipe);
        return;
    }
    if (hPipe["buttonClicked"] == "alist")
    {
        // Reuse the existing achievement system and its progress dialog.
        achievements_command(event, "");
        return;
    }
    if (hPipe["buttonClicked"] == "goals")
    {
        // The wrench menu Goals & Quests entry uses the existing quest tracker.
        quests_command(event, "");
        return;
    }
    if (hPipe["buttonClicked"] == "bonus")
    {
        // Reuse the daily rewards implementation instead of leaving the button inert.
        daily_command(event, "");
        return;
    }
    if (hPipe["buttonClicked"] == "trade_scan")
    {
        std::unordered_map<short, int> totals;
        for (const auto &slot : pPeer->slots)
        {
            if (slot.id <= 0 || slot.count <= 0 ||
                slot.id >= static_cast<int>(items.size()))
                continue;
            const auto &item = id_to_item(static_cast<u_short>(slot.id));
            if (item.id != slot.id || (item.cat & CAT_UNTRADEABLE))
                continue;
            totals[slot.id] += slot.count;
        }

        std::vector<std::pair<short, int>> sorted(totals.begin(), totals.end());
        std::ranges::sort(sorted, [](const auto &a, const auto &b)
        {
            return a.first < b.first;
        });

        std::string dialog =
            "set_bg_color|15,50,75,235|\n"
            "set_border_color|75,205,230,255|\n"
            "add_label_with_icon|big|Trade-Scan|left|6016|\n"
            "add_textbox|Items in your backpack that are not marked untradeable.|left|\n"
            "add_spacer|small|\n";
        if (sorted.empty())
            dialog += "add_textbox|No tradeable items found in your backpack.|left|\n";
        for (const auto &[id, count] : sorted)
        {
            const auto &item = id_to_item(static_cast<u_short>(id));
            dialog += std::format("add_label_with_icon|small|{} x{}|left|{}|\n",
                item.raw_name, count, id);
        }
        dialog += "add_spacer|small|\nend_dialog|trade_scan||Close|\nadd_quick_exit|\n";
        send_varlist(event.peer, {"OnDialogRequest", std::move(dialog)});
        return;
    }
    if (hPipe["buttonClicked"] == "my_worlds")
    {
        auto section = [](const auto &range) 
        {
            std::string result;
            for (const std::string &name : range)
                if (!name.empty())
                    result.append(std::format("add_button|{0}|{0}|noflags|0|0|\n", name));
            return result;
        };
        send_varlist(event.peer, {
            "OnDialogRequest",
            std::format(
                "set_default_color|`o\n"
                "start_custom_tabs|\n"
                "add_custom_button|myWorldsUiTab_0|image:interface/large/btn_tabs2.rttex;image_size:228,92;frame:1,0;width:0.15;|\n"
                "add_custom_button|myWorldsUiTab_1|image:interface/large/btn_tabs2.rttex;image_size:228,92;frame:0,1;width:0.15;|\n"
                "add_custom_button|myWorldsUiTab_2|image:interface/large/btn_tabs2.rttex;image_size:228,92;frame:0,2;width:0.15;|\n"
                "end_custom_tabs|\n"
                "add_label|big|Locked Worlds|left|0|\n"
                "add_spacer|small|\n"
                "add_textbox|Place a World Lock in a world to lock it. Break your World Lock to unlock a world.|left|\n"
                "add_spacer|small|\n"
                "{}\n"
                "add_spacer|small|\n"
                "end_dialog|worlds_list||Back|\n"
                "add_quick_exit|\n",
                section(pPeer->my_worlds)
            )
        });
    }
    else if (hPipe["buttonClicked"] == "billboard_edit")
    {
        const ::item &item = id_to_item(pPeer->billboard.id);

        send_varlist(event.peer, {
            "OnDialogRequest",
            std::format(
                "set_default_color|`o\n"
                "add_label_with_icon|big|`wTrade Billboard``|left|8282|\n"
                "add_spacer|small|\n"
                "{}"
                "add_item_picker|billboard_item|`wSelect Billboard Item``|Choose an item to put on your billboard!|\n"
                "add_spacer|small|\n"
                "add_checkbox|billboard_toggle|`$Show Billboard``|{}\n"
                "add_checkbox|billboard_buying_toggle|`$Is Buying``|{}\n"
                "add_text_input|setprice|Price of item:|{}|5|\n"
                "add_checkbox|chk_peritem|World Locks per Item|{}\n"
                "add_checkbox|chk_perlock|Items per World Lock|{}\n"
                "add_spacer|small|\n"
                "end_dialog|billboard_edit|Close|Update|\n",
                (pPeer->billboard.id == 0) ? 
                    "" : 
                    std::format("add_label_with_icon|small|`w{}``|left|{}|\n", item.raw_name, pPeer->billboard.id),
                to_char(pPeer->billboard.show),
                to_char(pPeer->billboard.isBuying),
                pPeer->billboard.price,
                to_char(pPeer->billboard.perItem),
                to_char(!pPeer->billboard.perItem)
            )
        });
    }
    else if (hPipe["buttonClicked"] == "seed_diary_customization")
    {
        send_varlist(event.peer, { "OnDialogRequestRML", "show_seed_diary_ui" });
    }
}