#include "pch.hpp"

#include "onVariant/ConsoleMessage.hpp"
#include "tools/time.hpp"
#include "ready.hpp"

#include <ctime>

void ready(ENetEvent &event, const std::string_view)
{
    if (!event.peer) return;
    auto *player = static_cast<::peer*>(event.peer->data);
    if (!player) return;

    if (player->role < DEVELOPER)
    {
        on::ConsoleMessage(event.peer, "`4You do not have permission to ready a world.``");
        return;
    }

    const std::string &world_name = player->recent_worlds.back();
    if (world_name.empty())
    {
        on::ConsoleMessage(event.peer, "`4Enter a world before using /ready.``");
        return;
    }

    auto world = std::ranges::find(worlds, world_name, &::world::name);
    if (world == worlds.end())
    {
        on::ConsoleMessage(event.peer, "`4Could not find the current world.``");
        return;
    }

    const u_int now = static_cast<u_int>(std::time(nullptr));
    std::size_t ready_trees{};
    for (::tree &tree : world->trees)
    {
        const int x = tree.pos.x_int();
        const int y = tree.pos.y_int();
        if (x < 0 || x >= 100 || y < 0 || y >= 60) continue;

        ::block &block = world->blocks[cord(x, y)];
        const ::item &seed = id_to_item(block.fg);
        if (seed.type != type::SEED) continue;

        const u_int grow_time = static_cast<u_int>(std::max(seed.tick, 0));
        tree.tick = (grow_time <= now) ? now - grow_time : 0u;
        send_tile_update(event, ::gamePacket{.id = block.fg, .punch = ::pos{x, y}}, block, *world);
        ++ready_trees;
    }

    world->provider_cooldowns.clear();
    world->save_provider_cooldowns();

    std::size_t refreshed_providers{};
    for (std::size_t index = 0; index < world->blocks.size(); ++index)
    {
        ::block &block = world->blocks[index];
        if (block.fg == 0 || id_to_item(block.fg).type != type::PROVIDER) continue;

        const ::pos pos{static_cast<int>(index % 100), static_cast<int>(index / 100)};
        send_tile_update(event, ::gamePacket{.id = block.fg, .punch = pos}, block, *world);
        ++refreshed_providers;
    }

    on::ConsoleMessage(event.peer, std::format(
        "`2Ready {} trees and refreshed {} provider tiles in `{}`.``",
        ready_trees, refreshed_providers, world->name));
}
