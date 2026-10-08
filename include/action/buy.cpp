#include "pch.hpp"

#include <cstdlib>
#include <limits>

#include "store.hpp"
#include "onVariant/SetBux.hpp"
#include "database/shouhin.hpp"
#include "automate/holiday.hpp"
#include "buy.hpp"

namespace
{
    bool is_holiday_item(short item_id)
    {
        return item_id > 0 && static_cast<std::size_t>(item_id) < items.size() &&
            (id_to_item(static_cast<u_short>(item_id)).cat & CAT_HOLIDAY) != 0;
    }

    bool contains_unavailable_holiday_item(const ::shouhin &product)
    {
        return !is_holiday_item_creation_season() &&
            std::ranges::any_of(product.items, [](const auto &entry) { return is_holiday_item(entry.first); });
    }

    int backpack_upgrade_number(int slot_size)
    {
        return std::max(1, (slot_size - 16) / 10 + 1);
    }

    int backpack_upgrade_cost(int slot_size)
    {
        const int number = backpack_upgrade_number(slot_size);
        return 100 * number * number - 200 * number + 200;
    }

    void append_random_items(::shouhin &product, const std::vector<::item> &catalog)
    {
        std::vector<short> candidates{};
        if (product.btn == "basic_splice")
        {
            product.items.emplace_back(11, 10);
            candidates = {3567, 2793, 57, 13, 17, 21, 101, 381, 1139};
            for (int i = 0; i < 10; ++i)
                product.items.emplace_back(candidates[std::rand() % candidates.size()], 1);
        }
        else if (product.btn == "rare_seed")
        {
            for (const ::item &candidate : catalog)
                if (candidate.type == type::SEED && candidate.rarity >= 13 && candidate.rarity <= 60 &&
                    (is_holiday_item_creation_season() || !(candidate.cat & CAT_HOLIDAY)))
                    candidates.emplace_back(candidate.id);
            for (int i = 0; i < 5 && !candidates.empty(); ++i)
                product.items.emplace_back(candidates[std::rand() % candidates.size()], 1);
        }
        else if (product.btn == "clothes_pack" || product.btn == "rare_clothes_pack")
        {
            for (const ::item &candidate : catalog)
                if (candidate.type == type::CLOTHING &&
                    (is_holiday_item_creation_season() || !(candidate.cat & CAT_HOLIDAY)) &&
                    ((product.btn == "clothes_pack" && candidate.rarity <= 10) ||
                     (product.btn == "rare_clothes_pack" && candidate.rarity >= 11 && candidate.rarity <= 60)))
                    candidates.emplace_back(candidate.id);
            for (int i = 0; i < 3 && !candidates.empty(); ++i)
                product.items.emplace_back(candidates[std::rand() % candidates.size()], 1);
        }
    }
}

void action::buy(ENetEvent& event, const std::string& header, const std::string_view selection)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    ::hPipe hPipe{ header };
    const std::string item = hPipe["item"];
    auto growtoken = std::ranges::find(pPeer->slots, 1486, &::slot::id);
    const int growtoken_count = growtoken == pPeer->slots.end() ? 0 : growtoken->count;

    u_short tab{};
    if (item == "main") action::store(event, "");
    else if (item == "locks") tab = 1;
    else if (item == "itempack") tab = 2;
    else if (item == "bigitems") tab = 3;
    else if (item == "weather") tab = 4;
    else if (item == "token") tab = 5;

    if (tab != 0)
    {
        std::string StoreRequest{};
        StoreRequest.append(
            (tab == 1) ? "set_description_text|`2Locks And Stuff!``  Select the item you'd like more info on, or BACK to go back.\n" :
            (tab == 2) ? "set_description_text|`2Item Packs!``  Select the item you'd like more info on, or BACK to go back.\n" :
            (tab == 3) ? "set_description_text|`2Awesome Items!``  Select the item you'd like more info on, or BACK to go back.\n" :
            (tab == 4) ? "set_description_text|`2Weather Machines!``  Select the item you'd like more info on, or BACK to go back.\n" :
            (tab == 5) ? std::format(
                "set_description_text|`2Spend your Growtokens!`` (You have `5{}``) You earn Growtokens from Crazy Jim and Sales-Man. Select the item you'd like more info on, or BACK to go back.\n",
                growtoken_count) : "");
        StoreRequest.append("enable_tabs|1\nadd_tab_button|main_menu|Home|interface/large/btn_shop.rttex||0|0|0|0||||-1|-1|||0|0|CustomParams:|\n");
        StoreRequest.append(std::format(
            "add_tab_button|locks_menu|Locks And Stuff|interface/large/btn_shop.rttex||{}|1|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|itempack_menu|Item Packs|interface/large/btn_shop.rttex||{}|3|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|bigitems_menu|Awesome Items|interface/large/btn_shop.rttex||{}|4|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|weather_menu|Weather Machines|interface/large/btn_shop.rttex|Tired of the same sunny sky?  We offer alternatives within...|{}|5|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|token_menu|Growtoken Items|interface/large/btn_shop.rttex||{}|2|0|0||||-1|-1|||0|0|CustomParams:|\n",
            tab == 1 ? "1" : "0", tab == 2 ? "1" : "0", tab == 3 ? "1" : "0", tab == 4 ? "1" : "0", tab == 5 ? "1" : "0"));

        for (const auto &[store_tab, product] : shouhin_tachi)
        {
            if (store_tab != tab) continue;
            if (contains_unavailable_holiday_item(product)) continue;
            int cost = product.cost;
            if (product.btn == "upgrade_backpack")
            {
                if (backpack_upgrade_number(pPeer->slot_size) > 38) continue;
                cost = backpack_upgrade_cost(pPeer->slot_size);
            }
            StoreRequest.append(std::format(
                "add_button|{}|{}|{}|{}|{}|{}|{}|0|||-1|-1||-1|-1||1||||||0|0|CustomParams:|\n",
                product.btn, product.name, product.rttx, product.description, product.tex1, product.tex2, cost));
        }
        if (!selection.empty()) StoreRequest.append(std::format("select_item|{}\n", selection));
        send_varlist(event.peer, { "OnStoreRequest", StoreRequest });
        return;
    }

    for (const auto &[store_tab, stored_product] : shouhin_tachi)
    {
        if (item != stored_product.btn) continue;

        ::shouhin product = stored_product; // Per-purchase copy: random packs and prices must not mutate the shared catalog.
        const bool growtoken_purchase = store_tab == 5;
        int price = growtoken_purchase ? std::abs(product.cost) : product.cost;
        if (product.btn == "upgrade_backpack")
        {
            if (backpack_upgrade_number(pPeer->slot_size) > 38)
            {
                send_varlist(event.peer, { "OnStorePurchaseResult", "Your backpack is already at the maximum size." });
                return;
            }
            price = backpack_upgrade_cost(pPeer->slot_size);
        }

        if (growtoken_purchase ? (price <= 0 || growtoken_count < price) : (price < 0 || pPeer->gems < price))
        {
            const int short_by = growtoken_purchase ? price - growtoken_count : price - pPeer->gems;
            send_varlist(event.peer, { "OnStorePurchaseResult", growtoken_purchase ?
                std::format("You can't afford `0{}``! You're `${}`` `2Growtokens`` short.", product.name, short_by) :
                std::format("You can't afford `0{}``! You're `${}`` Gems short.", product.name, short_by) });
            return;
        }

        append_random_items(product, items);
        if (contains_unavailable_holiday_item(product))
        {
            send_varlist(event.peer, { "OnStorePurchaseResult", "This item can only be created during Halloween or WinterFest." });
            return;
        }
        if ((product.btn == "rare_seed" || product.btn == "clothes_pack" || product.btn == "rare_clothes_pack") && product.items.empty())
        {
            send_varlist(event.peer, { "OnStorePurchaseResult", "This item pack is unavailable right now. No currency was taken." });
            return;
        }

        int remaining_growtokens = growtoken_count;
        if (growtoken_purchase)
        {
            // Remove tokens before adding rewards; adding a new slot can invalidate the old inventory iterator.
            remaining_growtokens -= price;
            modify_item_inventory(event, ::slot(1486, static_cast<short>(-price)));
        }
        else
        {
            pPeer->gems -= price;
            on::SetBux(event);
        }

        std::string received{};
        bool backpack_upgraded = false;
        for (const auto &[item_id, amount] : product.items)
        {
            if (item_id == 9412)
            {
                pPeer->slot_size += 10;
                backpack_upgraded = true;
                received.append(std::format("{} additional backpack slots, ", amount));
            }
            else
            {
                modify_item_inventory(event, ::slot(item_id, amount));
                received.append(std::format("{}, ", id_to_item(item_id).raw_name));
            }
        }
        if (backpack_upgraded)
        {
            pPeer->save_inventory();
            send_inventory_state(event);
        }

        if (growtoken_purchase)
        {
            send_varlist(event.peer, { "OnStorePurchaseResult", std::format(
                "You've purchased `0{}`` for `${}`` `2Growtokens``.\nYou have `${}`` `2Growtokens`` left.\n\n`5Received: ```0{}``",
                product.name, price, remaining_growtokens, received) });
        }
        else
        {
            send_varlist(event.peer, { "OnStorePurchaseResult", std::format(
                "You've purchased `0{}`` for `${}`` Gems.\nYou have `${}`` Gems left.\n\n`5Received: ```0{}``",
                product.name, price, pPeer->gems, received) });
        }
        if (backpack_upgraded)
        {
            // Re-send the current tab after the slot size changes so the displayed next price updates immediately.
            action::buy(event, "item|locks", "upgrade_backpack");
        }
        return;
    }

    send_varlist(event.peer, { "OnStorePurchaseResult", "That item is not available in this store tab." });
}
