#include "pch.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include "lock_edit.hpp"

void lock_edit(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;
    if (world->owner != pPeer->user_id && pPeer->role == PLAYER) return;

    ::pos pos{};
    pos.x = atoi(hPipe["tilex"].c_str());
    pos.y = atoi(hPipe["tiley"].c_str());
    
    world->is_public = atoi(hPipe["checkbox_public"].c_str());
    if (atoi(hPipe["checkbox_disable_music"].c_str()) != 0)
        world->lock_state |= DISABLE_MUSIC;
    else world->lock_state &= ~DISABLE_MUSIC;
    const int minimum_level = std::clamp(atoi(hPipe["minimum_entry_level"].c_str()), 1, 125);
    world->minimum_entry_level = static_cast<u_char>(minimum_level);
    const unsigned saved_minimum_level = world->minimum_entry_level;
    world->mysql_update<unsigned>("minimum_entry_level", saved_minimum_level);

    const std::string selected_netid = hPipe["playerNetID"];
    if (!selected_netid.empty())
    {
        const int netid = std::atoi(selected_netid.c_str());
        int selected_user_id{};
        peers(world->name, PEER_SAME_WORLD, [&](ENetPeer &connection)
        {
            auto *candidate = static_cast<::peer*>(connection.data);
            if (candidate && candidate->netid == netid)
                selected_user_id = candidate->user_id;
        });

        if (selected_user_id > 0 && selected_user_id != world->owner)
        {
            auto current_access = std::ranges::find(world->access, selected_user_id);
            if (current_access != world->access.end())
            {
                *current_access = 0;
                ::blob saved_access{};
                for (int user_id : world->access)
                    if (user_id != 0) saved_access.i32(user_id);
                world->mysql_update<::blob>("access", saved_access);
                on::ConsoleMessage(event.peer, "`2Player removed from the world lock access list.``");
            }
            else
            {
                auto empty_slot = std::ranges::find(world->access, 0);
                if (empty_slot != world->access.end())
                {
                    *empty_slot = selected_user_id;
                    ::blob saved_access{};
                    for (int user_id : world->access)
                        if (user_id != 0) saved_access.i32(user_id);
                    world->mysql_update<::blob>("access", saved_access);
                    on::ConsoleMessage(event.peer, "`2Player added to the world lock access list.``");
                }
                else on::ConsoleMessage(event.peer, "`4The world lock access list is full.``");
            }
        }
    }

    ::block &block = world->blocks[cord(pos.x, pos.y)];

    if (world->is_public) 
         block.state[2] |= S_PUBLIC;
    else block.state[2] &= ~S_PUBLIC;
    on::ConsoleMessage(event.peer, std::format("`2{}`` has set the `$World Lock`` to {}``", pPeer->growid, (block.state[2] & S_PUBLIC) ? "`$PUBLIC" : "`4PRIVATE"));

    send_tile_update(event, {
        .id = block.fg,
        .punch = pos
    }, block, *world);
}
