#include "pch.hpp"

#include "onVariant/ConsoleMessage.hpp"
#include "display_edit.hpp"

void display_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    if (hPipe["buttonClicked"] != "take_display") return;

    auto *player = static_cast<::peer*>(event.peer->data);
    if (!player) return;

    auto world = std::ranges::find(worlds, player->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;
    if (world->owner && !world->is_public && player->role == PLAYER && player->user_id != world->owner &&
        std::ranges::find(world->access, player->user_id) == world->access.end())
        return;

    const int x = std::atoi(hPipe["tilex"].c_str());
    const int y = std::atoi(hPipe["tiley"].c_str());
    if (x < 0 || x >= 100 || y < 0 || y >= 60) return;
    const ::pos tile{x, y};
    auto display = std::ranges::find(world->displays, tile, &::display::pos);
    if (display == world->displays.end()) return;

    const short item_id = static_cast<short>(display->id);
    const auto existing_item = std::ranges::find(player->slots, item_id, &::slot::id);
    if (existing_item == player->slots.end() && player->slots.size() >= static_cast<std::size_t>(player->slot_size))
    {
        on::ConsoleMessage(event.peer, "`4Your backpack is full.``");
        return;
    }
    if (existing_item != player->slots.end() && existing_item->count >= 200)
    {
        on::ConsoleMessage(event.peer, "`4Your stack of this item is full.``");
        return;
    }

    player->emplace(::slot(item_id, 1));
    world->displays.erase(display);
    ::block &block = world->blocks[cord(x, y)];
    send_tile_update(event, { .id = block.fg, .punch = tile }, block, *world);
    on::ConsoleMessage(event.peer, std::format("`2You took {} from the display.``", id_to_item(item_id).raw_name));
}
