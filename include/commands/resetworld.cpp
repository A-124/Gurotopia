#include "pch.hpp"

#include "action/join_request.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "resetworld.hpp"

void resetworld(ENetEvent &event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    if (pPeer->role < DEVELOPER)
    {
        on::ConsoleMessage(event.peer, "`4You do not have permission to reset worlds.``");
        return;
    }

    const std::string world_name = pPeer->recent_worlds.back();
    if (world_name.empty())
    {
        on::ConsoleMessage(event.peer, "`4Enter a world before using /resetworld.``");
        return;
    }

    auto world_it = std::ranges::find(worlds, world_name, &::world::name);
    if (world_it == worlds.end())
    {
        on::ConsoleMessage(event.peer, "`4Could not find the current world.``");
        return;
    }

    if (peers(world_name, PEER_SAME_WORLD).size() != 1)
    {
        on::ConsoleMessage(event.peer, "`4Everyone else must leave the world before it can be reset.``");
        return;
    }

    world_it->owner = 0;
    world_it->access.fill(0);
    world_it->is_public = false;
    world_it->lock_state = 0;
    world_it->minimum_entry_level = 1;
    world_it->doors.clear();
    world_it->signs.clear();
    world_it->trees.clear();
    world_it->displays.clear();
    world_it->random_blocks.clear();
    world_it->provider_cooldowns.clear();
    world_it->vending_machines.clear();
    world_it->objects.clear();
    world_it->last_object_uid = 0;
    world_it->weather = {0, 0};
    generate_world(*world_it);
    world_it->save_metadata();
    world_it->save_provider_cooldowns();
    world_it->save_vending_machines();
    world_it->save_blocks();
    world_it->save_objects();

    on::ConsoleMessage(event.peer, std::format("`2World `{}`` reset. Rejoining the fresh world...``", world_name));
    worlds.erase(world_it); // destructor saves the reset world to MariaDB.
    pPeer->netid = 0;
    action::join_request(event, "", world_name);
}
