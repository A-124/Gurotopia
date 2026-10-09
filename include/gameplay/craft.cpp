#include "pch.hpp"
#include <algorithm>
#include <sstream>
#include "database/peer.hpp"
#include "database/custom_content.hpp"
#include "database/item_registry.hpp"
#include "gameplay/craft.hpp"
#include "core/event_bus.hpp"

namespace {
int inventory_count(const peer& player, int id) {
    int total = 0;
    for (const auto& slot : player.slots)
        if (slot.id == id) total += slot.count;
    return total;
}
}

void craft(ENetEvent& event, const std::string_view text) {
    auto* player = event.peer ? static_cast<peer*>(event.peer->data) : nullptr;
    if (!player) return;

    std::istringstream input{std::string(text)};
    std::string command;
    int result_id{}, amount{1};
    input >> command >> result_id >> amount;

    if (!custom_content::find_recipe(result_id)) {
        send_varlist(event.peer, {"OnConsoleMessage", "No recipe exists for that item."});
        return;
    }
    if (amount < 1 || amount > 100) {
        send_varlist(event.peer, {"OnConsoleMessage", "Craft amount must be between 1 and 100."});
        return;
    }

    const auto* recipe = custom_content::find_recipe(result_id);
    for (const auto& [ingredient_id, ingredient_amount] : recipe->ingredients) {
        const long long required = static_cast<long long>(ingredient_amount) * amount;
        if (required > 2000000000LL || inventory_count(*player, ingredient_id) < required) {
            send_varlist(event.peer, {"OnConsoleMessage",
                std::format("Not enough ingredient {}.", ingredient_id)});
            return;
        }
    }

    const long long total_output = static_cast<long long>(recipe->amount) * amount;
    if (total_output > 200) {
        send_varlist(event.peer, {"OnConsoleMessage",
            std::format("That craft exceeds the 200-item stack limit. Craft at most {} at a time.",
                200 / recipe->amount)});
        return;
    }

    // Client-visible output must be a real items.dat item until the custom
    // item protocol is implemented. Server-only custom IDs are rejected.
    if (result_id < 0 || !item_registry::exists(static_cast<u_short>(result_id))) {
        send_varlist(event.peer, {"OnConsoleMessage",
            "This recipe produces a server-only custom item and cannot be crafted yet."});
        return;
    }

    for (const auto& [ingredient_id, ingredient_amount] : recipe->ingredients)
        player->emplace(slot{static_cast<short>(ingredient_id),
            static_cast<short>(-ingredient_amount * amount)});

    const int output = recipe->amount * amount;
    player->emplace(slot{static_cast<short>(result_id),
        static_cast<short>(output)});
    send_inventory_state(event);
    send_varlist(event.peer, {"OnConsoleMessage",
        std::format("Crafted {} x{}.", result_id, output)});
    event_bus::emit({ event_bus::type::item_changed, event.peer, {}, result_id, output }); // @note quests & achievements
}
