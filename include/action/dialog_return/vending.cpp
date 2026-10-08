#include "pch.hpp"

#include <charconv>
#include <limits>

#include "vending.hpp"
#include "tools/create_dialog.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/SetBux.hpp"

namespace
{
    constexpr short world_lock_id = 242;
    constexpr short diamond_lock_id = 1796;
    constexpr int wl_per_diamond = 100;

    struct vending_selection
    {
        std::string world_name;
        ::pos pos{};
        short item_id{};
    };

    std::unordered_map<ENetPeer*, vending_selection> pending_selections;

    ::world *find_world(::peer &player)
    {
        if (player.recent_worlds.back().empty()) return nullptr;
        auto world = std::ranges::find(worlds, player.recent_worlds.back(), &::world::name);
        return world == worlds.end() ? nullptr : &*world;
    }

    ::vending_machine_state *find_machine(::world &world, const ::pos &pos)
    {
        auto machine = std::ranges::find(world.vending_machines, pos, &::vending_machine_state::pos);
        return machine == world.vending_machines.end() ? nullptr : &*machine;
    }

    bool can_manage(const ::peer &player, const ::world &world)
    {
        return player.role >= MODERATOR ||
            (player.user_id != 0 && world.owner != 0 && (world.owner == player.user_id ||
                std::ranges::find(world.access, player.user_id) != world.access.end()));
    }

    bool parse_integer(std::string_view text, int &value)
    {
        if (text.empty()) return false;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc{} && end == text.data() + text.size();
    }

    std::string picker_value(const ::hPipe &hPipe)
    {
        std::string value = hPipe["itemID"];
        if (value.empty()) value = hPipe["stock_item"];
        return value;
    }

    int pending_item(ENetPeer *connection, const ::world &world, const ::pos &pos)
    {
        const auto it = pending_selections.find(connection);
        if (it == pending_selections.end() || it->second.world_name != world.name || it->second.pos != pos) return 0;
        return it->second.item_id;
    }

    std::string price_description(int price, bool legacy = false)
    {
        if (legacy) return std::format("{} Gems per item (old vending data)", price);
        if (price > 0) return std::format("{} World Lock{} per item", price, price == 1 ? "" : "s");
        if (price < 0)
        {
            const long long count = -static_cast<long long>(price);
            return std::format("{} item{} per World Lock", count, count == 1 ? "" : "s");
        }
        return "Out of order";
    }

    bool expected_machine_matches(const ::hPipe &hPipe, const ::vending_machine_state &machine)
    {
        int expected_item{}, expected_price{};
        const std::string item_text = hPipe["expectitem"];
        const std::string price_text = hPipe["expectprice"];
        if (!item_text.empty() && (!parse_integer(item_text, expected_item) || expected_item != machine.item_id)) return false;
        if (!price_text.empty() && (!parse_integer(price_text, expected_price) || expected_price != machine.price)) return false;
        return true;
    }

    void update_machine_tile(ENetEvent &event, ::world &world, const ::pos &pos, short block_id)
    {
        ::block &block = world.blocks[cord(pos.x, pos.y)];
        send_tile_update(event, ::gamePacket{.id = block_id, .punch = pos}, block, world);
    }

    short inventory_capacity_for(const ::peer &player, short item_id)
    {
        const auto held = std::ranges::find(player.slots, item_id, &::slot::id);
        if (held != player.slots.end()) return static_cast<short>(std::max(0, 200 - held->count));
        if (player.slots.size() >= static_cast<std::size_t>(std::max(player.slot_size, 0))) return 0;
        return 200;
    }

    bool get_purchase_price(int unit_price, int amount, int &world_locks)
    {
        if (unit_price == 0 || unit_price == std::numeric_limits<int>::min() || amount <= 0) return false;
        const long long cost = unit_price > 0
            ? static_cast<long long>(unit_price) * amount
            : (amount % -unit_price == 0 ? amount / -unit_price : -1);
        if (cost <= 0 || cost > std::numeric_limits<int>::max()) return false;
        world_locks = static_cast<int>(cost);
        return true;
    }

    struct lock_funds
    {
        int wls{};
        int dls{};
        long long total{}; // wls + dls * 100, in World Lock value
    };

    lock_funds lock_funds_of(const ::peer &player)
    {
        lock_funds funds{};
        if (const auto wl = std::ranges::find(player.slots, world_lock_id, &::slot::id); wl != player.slots.end())
            funds.wls = std::max<int>(wl->count, 0);
        if (const auto dl = std::ranges::find(player.slots, diamond_lock_id, &::slot::id); dl != player.slots.end())
            funds.dls = std::max<int>(dl->count, 0);
        funds.total = static_cast<long long>(funds.wls) + static_cast<long long>(funds.dls) * wl_per_diamond;
        return funds;
    }

    void take_world_locks_chunked(ENetEvent &event, short item_id, int amount)
    {
        for (int remaining = amount; remaining > 0;)
        {
            const short chunk = static_cast<short>(std::min(remaining, 200));
            modify_item_inventory(event, ::slot(item_id, static_cast<short>(-chunk)));
            remaining -= chunk;
        }
    }

    // Drains World Locks first, then shatters Diamond Locks one at a time
    // (1 DL = 100 WL, change kept as WLs). Returns false if funds ran out.
    bool take_world_locks(ENetEvent &event, int cost)
    {
        ::peer *player = static_cast<::peer*>(event.peer->data);
        if (!player || cost <= 0 || lock_funds_of(*player).total < cost) return false;
        int remaining = cost;
        if (const auto wl = std::ranges::find(player->slots, world_lock_id, &::slot::id); wl != player->slots.end() && wl->count > 0)
        {
            const int take = std::min(static_cast<int>(wl->count), remaining);
            take_world_locks_chunked(event, world_lock_id, take);
            remaining -= take;
        }
        while (remaining > 0)
        {
            const auto dl = std::ranges::find(player->slots, diamond_lock_id, &::slot::id);
            if (dl == player->slots.end() || dl->count <= 0) return false; // @note pre-checked, should not happen
            modify_item_inventory(event, ::slot(diamond_lock_id, -1));
            modify_item_inventory(event, ::slot(world_lock_id, wl_per_diamond));
            const int take = std::min(wl_per_diamond, remaining);
            take_world_locks_chunked(event, world_lock_id, take);
            remaining -= take;
        }
        return true;
    }

    void show_purchase_dialog(ENetPeer *connection, const ::peer &buyer,
        const ::vending_machine_state &machine, const ::pos &pos, short block_id)
    {
        const ::item &goods = id_to_item(machine.item_id);
        const lock_funds funds = lock_funds_of(buyer);

        const int default_amount = machine.price < 0
            ? static_cast<int>(std::min<long long>({-static_cast<long long>(machine.price), machine.stock, 200})) : 1;

        // Short price text; the WL icon beside it carries the currency.
        const std::string price_line = machine.price > 0
            ? std::format("`oPrice:`` `w{}`` each", machine.price)
            : std::format("`oPrice:`` `w{}`` for 1", -static_cast<long long>(machine.price));

        auto dialog = ::create_dialog()
            .set_default_color("`o")
            .add_label_with_icon("big", "`wBuy from Vending Machine``", block_id)
            .add_spacer("small")
            .add_label_with_icon("small", std::format("`w{}`` `o(x{})``", goods.raw_name, machine.stock), machine.item_id)
            .add_label_with_icon("small", price_line, world_lock_id)
            .add_label_with_icon("small", std::format("`w{}``", funds.wls), world_lock_id)
            .add_label_with_icon("small", std::format("`w{}``", funds.dls), diamond_lock_id)
            .add_text_input("buycount", "Amount",
                std::to_string(std::max(default_amount, 1)), 4)
            .embed_data("tilex", static_cast<int>(pos.x))
            .embed_data("tiley", static_cast<int>(pos.y))
            .embed_data("expectitem", static_cast<int>(machine.item_id))
            .embed_data("expectprice", machine.price)
            .embed_data("buy_dialog", 1)
            .add_quick_exit();
        send_varlist(connection, {"OnDialogRequest", dialog.end_dialog("vending", "Close", "Purchase")});
    }
}

void open_vending_dialog(ENetPeer *connection, ::peer &player, ::world &world, const ::pos &pos, short block_id)
{
    auto *machine = find_machine(world, pos);
    const short item_id = machine ? machine->item_id : 0;
    const int stock = machine ? machine->stock : 0;
    const int price = machine ? machine->price : 0;
    const int bank = machine ? machine->bank : 0;
    const bool manager = can_manage(player, world);

    // Visitors can wrench a stocked machine to open checkout directly.
    if (!manager && machine && !machine->legacy_gem_currency && machine->stock > 0 && machine->price != 0 &&
        machine->item_id > 0 && static_cast<std::size_t>(machine->item_id) < items.size() &&
        !(items[machine->item_id].cat & CAT_UNTRADEABLE))
    {
        show_purchase_dialog(connection, player, *machine, pos, block_id);
        return;
    }

    auto dialog = ::create_dialog()
        .set_default_color("`o")
        .add_label_with_icon("big", "`wVending Machine``", block_id)
        .add_spacer("small")
        .embed_data("tilex", static_cast<int>(pos.x))
        .embed_data("tiley", static_cast<int>(pos.y));

    if (machine && machine->legacy_gem_currency)
    {
        dialog.add_textbox("This machine uses the old Gem-based data and is paused until its owner converts it.");
        if (item_id > 0 && stock > 0)
            dialog.add_label_with_icon("small", std::format("{} remaining: {}", id_to_item(item_id).raw_name, stock), item_id);
        if (manager)
        {
            if (stock > 0) dialog.add_button("pullstock", "Pull Remaining Stock");
            if (bank > 0) dialog.add_button("withdraw_gems", std::format("Withdraw Legacy {} Gems", bank));
            if (stock == 0 && bank == 0) dialog.add_button("convert_vending", "Convert to World Lock Pricing");
        }
    }
    else
    {
        if (machine && item_id > 0)
        {
            dialog.add_label_with_icon("small", std::format("{} in stock: {}", id_to_item(item_id).raw_name, stock), item_id)
                .add_textbox(price_description(price));
        }
        else dialog.add_textbox("This machine is empty.");

        if (manager)
        {
            const int selected_item = pending_item(connection, world, pos);
            dialog.add_spacer("small")
                .add_textbox("Set a price in World Locks. Use a negative number for items per World Lock.")
                .add_item_picker("itemID", "Select item from your inventory", "Choose the item you want to sell.")
                .add_text_input("amount", "Amount to add", 1, 4)
                .add_text_input("price", "WL per item (+) / items per WL (-)",
                    std::to_string(price == 0 && stock == 0 ? 1 : price), 10)
                .add_button("addstock", "Restock / Set Price")
                .add_button("setprice", "Set Price Only");
            if (selected_item > 0 && static_cast<std::size_t>(selected_item) < items.size())
                dialog.add_label_with_icon("small", std::format("Selected: `{}``", id_to_item(static_cast<u_short>(selected_item)).raw_name), selected_item);
            if (stock > 0) dialog.add_button("pullstock", "Pull Remaining Stock");
            if (bank > 0) dialog.add_button("withdraw", std::format("Collect {} World Locks", bank));
            if (machine && player.user_id != world.owner && stock > 0 && price != 0 && !machine->legacy_gem_currency)
                dialog.add_button("open_buy", "Buy");
        }
        else if (machine && stock > 0 && price != 0)
        {
            dialog.add_spacer("small")
                .embed_data("expectitem", static_cast<int>(item_id))
                .embed_data("expectprice", price);
        }
    }

    if (manager)
        send_varlist(connection, {"OnDialogRequest", dialog.end_dialog("vending", "Close", "")});
    else if (machine && machine->stock > 0 && machine->price != 0 && !machine->legacy_gem_currency)
        send_varlist(connection, {"OnDialogRequest", dialog.end_dialog("vending", "Close", "Buy")});
    else
        send_varlist(connection, {"OnDialogRequest", dialog.end_dialog("vending", "Close", "")});
}

void vending(ENetEvent &event, const ::hPipe &hPipe)
{
    if (!event.peer) return;
    auto *player = static_cast<::peer*>(event.peer->data);
    if (!player) return;
    auto *world = find_world(*player);
    if (!world) return;

    int x{}, y{};
    if (!parse_integer(hPipe["tilex"], x) || !parse_integer(hPipe["tiley"], y) || x < 0 || x >= 100 || y < 0 || y >= 60) return;
    const ::pos pos{x, y};
    const short block_id = world->blocks[cord(x, y)].fg;
    if (block_id <= 0 || static_cast<std::size_t>(block_id) >= items.size() || items[block_id].type != type::VENDING_MACHINE) return;
    const std::string action = hPipe["buttonClicked"];
    bool purchase_completed = false;
    auto *machine = find_machine(*world, pos);

    // The inventory picker can post its selection as a separate dialog return.
    // Keep it server-side because re-opening this dialog resets the picker's UI state.
    const std::string selected_text = picker_value(hPipe);
    int selected_item{};
    if (!selected_text.empty() && parse_integer(selected_text, selected_item) &&
        selected_item > 0 && selected_item < static_cast<int>(items.size()) && items[selected_item].id == selected_item)
    {
        const auto held = std::ranges::find(player->slots, static_cast<short>(selected_item), &::slot::id);
        if (held != player->slots.end() && held->count > 0)
            pending_selections[event.peer] = {world->name, pos, static_cast<short>(selected_item)};
    }

    if (action == "convert_vending")
    {
        if (!machine || !machine->legacy_gem_currency || !can_manage(*player, *world))
            on::ConsoleMessage(event.peer, "`4Only the world owner or staff can convert this machine.``");
        else if (machine->stock != 0 || machine->bank != 0)
            on::ConsoleMessage(event.peer, "`4Pull the remaining stock and withdraw its legacy Gem balance first.``");
        else
        {
            machine->legacy_gem_currency = false;
            machine->price = 0;
            world->save_vending_machines();
            update_machine_tile(event, *world, pos, block_id);
            on::ConsoleMessage(event.peer, "`2Vending Machine converted to World Lock pricing.``");
        }
    }
    else if (action == "pullstock")
    {
        if (!machine || !can_manage(*player, *world))
            on::ConsoleMessage(event.peer, "`4Only the world owner or staff can pull vending stock.``");
        else if (machine->stock <= 0)
            on::ConsoleMessage(event.peer, "`4There is no stock to pull.``");
        else
        {
            const short capacity = inventory_capacity_for(*player, machine->item_id);
            const short amount = static_cast<short>(std::min<int>(machine->stock, capacity));
            if (amount <= 0) on::ConsoleMessage(event.peer, "`4Your backpack cannot hold this stock.``");
            else
            {
                const short item_id = machine->item_id;
                modify_item_inventory(event, ::slot(item_id, amount));
                machine->stock = static_cast<short>(machine->stock - amount);
                world->save_vending_machines();
                update_machine_tile(event, *world, pos, block_id);
                on::ConsoleMessage(event.peer, std::format("`2Pulled {} {} from the machine.``", amount, id_to_item(item_id).raw_name));
            }
        }
    }
    else if (action == "withdraw_gems")
    {
        if (!machine || !machine->legacy_gem_currency || !can_manage(*player, *world))
            on::ConsoleMessage(event.peer, "`4Only the world owner or staff can withdraw this legacy balance.``");
        else if (machine->bank <= 0)
            on::ConsoleMessage(event.peer, "`4This machine has no stored Gems.``");
        else if (machine->bank > std::numeric_limits<int>::max() - player->gems)
            on::ConsoleMessage(event.peer, "`4Your Gem balance cannot hold the legacy payout.``");
        else
        {
            const int payout = machine->bank;
            machine->bank = 0;
            player->gems += payout;
            on::SetBux(event);
            world->save_vending_machines();
            on::ConsoleMessage(event.peer, std::format("`2Withdrew {} legacy Gems.``", payout));
        }
    }
    else if (action == "withdraw")
    {
        if (!machine || machine->legacy_gem_currency || !can_manage(*player, *world))
            on::ConsoleMessage(event.peer, "`4Only the world owner or staff can collect World Locks.``");
        else if (machine->bank <= 0)
            on::ConsoleMessage(event.peer, "`4This machine has no stored World Locks.``");
        else
        {
            const short capacity = inventory_capacity_for(*player, world_lock_id);
            const short payout = static_cast<short>(std::min({machine->bank, static_cast<int>(capacity), 200}));
            if (payout <= 0) on::ConsoleMessage(event.peer, "`4Your backpack cannot hold any World Locks.``");
            else
            {
                modify_item_inventory(event, ::slot(world_lock_id, payout));
                machine->bank -= payout;
                world->save_vending_machines();
                update_machine_tile(event, *world, pos, block_id);
                on::ConsoleMessage(event.peer, std::format("`2Collected {} World Lock{} from the machine.``", payout, payout == 1 ? "" : "s"));
            }
        }
    }
    else if (action == "setprice")
    {
        if ((machine && machine->legacy_gem_currency) || !can_manage(*player, *world))
            on::ConsoleMessage(event.peer, "`4Convert this machine and make sure you have permission before setting its price.``");
        else
        {
            int price{};
            const std::string item_text = picker_value(hPipe);
            int chosen_item = pending_item(event.peer, *world, pos);
            if (chosen_item <= 0 && machine) chosen_item = machine->item_id;
            const bool valid_item = item_text.empty() ? chosen_item > 0 : parse_integer(item_text, chosen_item);
            if (!valid_item || chosen_item <= 0 || chosen_item >= static_cast<int>(items.size()) || items[chosen_item].id != chosen_item)
                on::ConsoleMessage(event.peer, "`4Tap the item picker and select an item from your inventory first.``");
            else if (!parse_integer(hPipe["price"], price) || price == std::numeric_limits<int>::min())
                on::ConsoleMessage(event.peer, "`4Enter a valid price: positive = WL per item, negative = items per WL, 0 = out of order.``");
            else if (machine && machine->stock > 0 && machine->item_id != chosen_item)
                on::ConsoleMessage(event.peer, "`4Pull the current stock before changing the item.``");
            else
            {
                if (!machine)
                {
                    world->vending_machines.emplace_back(pos, static_cast<short>(chosen_item), 0, price, 0);
                    machine = &world->vending_machines.back();
                }
                machine->item_id = static_cast<short>(chosen_item);
                machine->price = price;
                world->save_vending_machines();
                update_machine_tile(event, *world, pos, block_id);
                on::ConsoleMessage(event.peer, price == 0 ? "`2Machine set to Out of Order.``" :
                    std::format("`2Price set: {}.``", price_description(price)));
            }
        }
    }
    else if (action == "addstock" || action == "vending_restock")
    {
        if (!can_manage(*player, *world))
            on::ConsoleMessage(event.peer, "`4Only the world owner or staff can manage this machine.``");
        else if (machine && machine->legacy_gem_currency)
            on::ConsoleMessage(event.peer, "`4Convert this old machine to World Lock pricing before adding stock.``");
        else
        {
            int amount{}, price{};
            const std::string item_text = picker_value(hPipe);
            int item_id = pending_item(event.peer, *world, pos);
            if (item_id <= 0 && machine) item_id = machine->item_id;
            const bool valid_item = item_text.empty() ? item_id > 0 : parse_integer(item_text, item_id);
            const std::string amount_text = hPipe["amount"].empty() ? hPipe["stock_count"] : hPipe["amount"];
            const std::string price_text = hPipe["price"].empty() ? hPipe["stock_price"] : hPipe["price"];
            if (!valid_item || item_id <= 0 || item_id >= static_cast<int>(items.size()) || items[item_id].id != item_id)
                on::ConsoleMessage(event.peer, "`4Tap the item picker and select an item from your inventory first.``");
            else if (!parse_integer(amount_text, amount) || amount <= 0 || amount > 200)
                on::ConsoleMessage(event.peer, "`4Enter an amount from 1 to 200.``");
            else if (!parse_integer(price_text, price) || price == std::numeric_limits<int>::min())
                on::ConsoleMessage(event.peer, "`4Enter a valid price: positive = WL per item, negative = items per WL, 0 = out of order.``");
            else if (price == 0)
                on::ConsoleMessage(event.peer, "`4Set a non-zero price before adding stock. Use Set Price Only with 0 to leave the machine out of order.``");
            else if (items[item_id].cat & CAT_UNTRADEABLE)
                on::ConsoleMessage(event.peer, "`4That item is untradeable and cannot be sold in a vending machine.``");
            else if (machine && machine->stock > 0 && machine->item_id != item_id)
                on::ConsoleMessage(event.peer, "`4Pull the current stock before changing the item.``");
            else if (machine && machine->stock + amount > 200)
                on::ConsoleMessage(event.peer, "`4A vending machine can hold at most 200 items.``");
            else
            {
                const auto held = std::ranges::find(player->slots, static_cast<short>(item_id), &::slot::id);
                if (held == player->slots.end() || held->count < amount)
                    on::ConsoleMessage(event.peer, "`4You do not have enough of that item to stock the machine.``");
                else
                {
                    modify_item_inventory(event, ::slot(static_cast<short>(item_id), static_cast<short>(-amount)));
                    if (!machine)
                    {
                        world->vending_machines.emplace_back(pos, static_cast<short>(item_id), 0, price, 0);
                        machine = &world->vending_machines.back();
                    }
                    machine->item_id = static_cast<short>(item_id);
                    machine->stock = static_cast<short>(machine->stock + amount);
                    machine->price = price;
                    world->save_vending_machines();
                    update_machine_tile(event, *world, pos, block_id);
                    pending_selections.erase(event.peer);
                    on::ConsoleMessage(event.peer, std::format("`2Added {} {} to the vending machine at {}.``", amount,
                        items[item_id].raw_name, price_description(price)));
                }
            }
        }
    }
    else
    {
        std::string buycount_text = hPipe["buycount"];
        if (buycount_text.empty()) buycount_text = hPipe["amount"];
        if (buycount_text.empty()) buycount_text = hPipe["count"];
        // Quick-buy buttons arrive as buttonClicked|buyamount_N. Prefer N over the
        // typed buycount so tapping "Buy 10" never buys the textbox value instead.
        int quick_amount = 0;
        constexpr std::string_view quick_prefix = "buyamount_";
        if (action.starts_with(quick_prefix))
        {
            const std::string_view number(action.data() + quick_prefix.size(), action.size() - quick_prefix.size());
            int parsed = 0;
            const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), parsed);
            if (error == std::errc{} && end == number.data() + number.size() && parsed > 0) quick_amount = parsed;
        }
        const bool has_buy_dialog = hPipe["buy_dialog"] == "1";
        // The client's end_dialog OK button ("Buy"/"Purchase") does not arrive as
        // buttonClicked the way custom add_button clicks do: it resubmits the dialog
        // fields with an empty (or "OK") buttonClicked. Detect intent from the
        // dialog contents so wrench -> Buy -> Purchase actually completes.
        const bool has_manage_fields = !hPipe["price"].empty() || !hPipe["stock_price"].empty() ||
            !picker_value(hPipe).empty() || pending_item(event.peer, *world, pos) > 0;
        const bool is_confirmation = hPipe["verify"] == "1" || action == "OK" || action.empty();
        const bool opening_buy_dialog = action == "Buy" || action == "open_buy" || action == "buy" ||
            ((action.empty() || action == "OK") && !has_buy_dialog && !has_manage_fields);
        const bool submitting_purchase = quick_amount > 0 || action == "Purchase" || action == "vending_buy" ||
            ((is_confirmation || action == "Buy" || action == "buy" || action == "open_buy" ||
              action == "Purchase" || action == "vending_buy") && has_buy_dialog) ||
            (has_buy_dialog && !buycount_text.empty());
        if (opening_buy_dialog && hPipe["buy_dialog"] != "1")
        {
            if (!machine || machine->legacy_gem_currency || machine->stock <= 0 || machine->price == 0)
                on::ConsoleMessage(event.peer, "`4This machine is empty or out of service.``");
            else if (machine->price == std::numeric_limits<int>::min())
                on::ConsoleMessage(event.peer, "`4This machine has an invalid price. Contact the world owner.``");
            else if (machine->item_id <= 0 || static_cast<std::size_t>(machine->item_id) >= items.size() ||
                     (items[machine->item_id].cat & CAT_UNTRADEABLE))
                on::ConsoleMessage(event.peer, "`4This item cannot be sold through a vending machine.``");
            else if (player->user_id == world->owner)
                on::ConsoleMessage(event.peer, "`4You cannot buy from your own world vending machine.``");
            else if (!expected_machine_matches(hPipe, *machine))
                on::ConsoleMessage(event.peer, "`4The vending price or item changed. Please reopen the machine.``");
            else
            {
                show_purchase_dialog(event.peer, *player, *machine, pos, block_id);
                return;
            }
        }
        else if (submitting_purchase)
        {
            if (!machine || machine->legacy_gem_currency || machine->stock <= 0 || machine->price == 0)
                on::ConsoleMessage(event.peer, "`4This machine is empty or out of service.``");
            else if (machine->price == std::numeric_limits<int>::min())
                on::ConsoleMessage(event.peer, "`4This machine has an invalid price. Contact the world owner.``");
            else if (machine->item_id <= 0 || static_cast<std::size_t>(machine->item_id) >= items.size() ||
                     (items[machine->item_id].cat & CAT_UNTRADEABLE))
                on::ConsoleMessage(event.peer, "`4This item cannot be sold through a vending machine.``");
            else if (player->user_id == world->owner)
                on::ConsoleMessage(event.peer, "`4You cannot buy from your own world vending machine.``");
            else if (!expected_machine_matches(hPipe, *machine))
                on::ConsoleMessage(event.peer, "`4The vending price or item changed. Please reopen the machine.``");
            else
            {
                int amount = quick_amount;
                bool valid_amount = amount > 0;
                if (!valid_amount)
                {
                    valid_amount = !buycount_text.empty() && parse_integer(buycount_text, amount);
                }
                if (!valid_amount || amount <= 0 || amount > 200 || amount > machine->stock)
                    on::ConsoleMessage(event.peer, "`4Enter a valid amount available in this machine.``");
                else
                {
                    int lock_cost{};
                    if (!get_purchase_price(machine->price, amount, lock_cost))
                        on::ConsoleMessage(event.peer, "`4For an items-per-lock price, buy an exact multiple of the listed amount.``");
                    else
                    {
                        const lock_funds funds = lock_funds_of(*player);
                        if (funds.total < lock_cost)
                            on::ConsoleMessage(event.peer, std::format("`4You need {} World Lock{} for this purchase (have {} WL + {} DL).``",
                                lock_cost, lock_cost == 1 ? "" : "s", funds.wls, funds.dls));
                        else if (inventory_capacity_for(*player, machine->item_id) < amount)
                            on::ConsoleMessage(event.peer, "`4Your backpack does not have enough room for that amount.``");
                        else if (machine->bank > std::numeric_limits<int>::max() - lock_cost)
                            on::ConsoleMessage(event.peer, "`4The machine's World Lock bank is full.``");
                        else if (!take_world_locks(event, lock_cost))
                            on::ConsoleMessage(event.peer, "`4Payment failed. Please reopen the machine and try again.``");
                        else
                        {
                            const short item_id = machine->item_id;
                            modify_item_inventory(event, ::slot(item_id, static_cast<short>(amount)));
                            machine->stock = static_cast<short>(machine->stock - amount);
                            machine->bank += lock_cost;
                            world->save_vending_machines();
                            update_machine_tile(event, *world, pos, block_id);
                            purchase_completed = true;
                            on::ConsoleMessage(event.peer, std::format("`2Purchased {} {} for {} World Lock{}.``", amount,
                                id_to_item(item_id).raw_name, lock_cost, lock_cost == 1 ? "" : "s"));
                            // @note vending log for scam disputes: buyer, owner, world, item, amount, price
                            std::printf("[vend] %s (UID %d) bought %dx%d from %s @%d,%d in %s (owner UID %d) for %d WL\n",
                                player->growid.c_str(), player->user_id, amount, item_id,
                                world->name.c_str(), x, y, world->name.c_str(), world->owner, lock_cost);
                        }
                    }
                }
            }
        }
    }

    if (action == "Close" || action == "Cancel" || action == "Back")
    {
        pending_selections.erase(event.peer);
        return;
    }

    if (!purchase_completed && (action == "Purchase" || action == "vending_buy" ||
        ((action == "Buy" || action == "buy" || action == "OK" || action.empty()) && hPipe["buy_dialog"] == "1")))
    {
        if (machine && machine->stock > 0 && machine->price != 0 && !machine->legacy_gem_currency &&
            expected_machine_matches(hPipe, *machine))
            show_purchase_dialog(event.peer, *player, *machine, pos, block_id);
        else
            open_vending_dialog(event.peer, *player, *world, pos, block_id);
        return;
    }

    open_vending_dialog(event.peer, *player, *world, pos, block_id);
}
