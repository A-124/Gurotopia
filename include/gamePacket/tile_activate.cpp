#include "pch.hpp"
#include "action/join_request.hpp"
#include "action/quit_to_exit.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "tile_activate.hpp"

namespace
{
    bool move_to_door(ENetEvent &event, ::peer &player, ::world &world, std::string_view id)
    {
        const auto target = std::ranges::find(world.doors, id, &::door::id);
        if (target == world.doors.end()) return false;

        player.pos = target->pos.by_32(false);
        send_varlist(event.peer, {
            "OnSetPos",
            CL_Vec2f{player.pos.x, player.pos.y}
        }, player.netid);
        return true;
    }
}

void tile_activate(ENetEvent& event, ::gamePacket gamePacket)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;
    if (!tile_in_bounds(gamePacket.punch.x, gamePacket.punch.y)) return; // @note forged coordinates would index blocks[] out of bounds

    ::block &block = world->blocks[cord(gamePacket.punch.x, gamePacket.punch.y)];
    const ::item &item = id_to_item(block.fg);
    switch (item.type)
    {
        case type::MAIN_DOOR:
        {
            action::quit_to_exit(event, "", false);
            break;
        }
        case type::DOOR:
        case type::PORTAL:
        {
            auto door = std::ranges::find(world->doors, gamePacket.punch, &::door::pos);
            if (door != world->doors.end() && !door->dest.empty())
            {
                const std::string destination = door->dest;
                const std::size_t separator = destination.rfind(':');
                if (separator == std::string::npos)
                {
                    action::quit_to_exit(event, "", true);
                    action::join_request(event, "", destination);
                }
                else
                {
                    std::string destination_world = destination.substr(0, separator);
                    const std::string destination_id = destination.substr(separator + 1);
                    if (destination_world.empty()) destination_world = world->name;

                    if (destination_world == world->name)
                    {
                        if (!move_to_door(event, *pPeer, *world, destination_id))
                            on::ConsoleMessage(event.peer, "`4That door ID was not found in this world.``");
                    }
                    else
                    {
                        action::quit_to_exit(event, "", true);
                        action::join_request(event, "", destination_world);
                        if (!destination_id.empty())
                        {
                            auto destination_world_it = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
                            if (destination_world_it != worlds.end() &&
                                !move_to_door(event, *pPeer, *destination_world_it, destination_id))
                                on::ConsoleMessage(event.peer, "`4That door ID was not found in the destination world.``");
                        }
                    }
                }
            }
            else {
                send_varlist(event.peer, ::VariantList{
                    "OnSetPos", 
                    CL_Vec2f{pPeer->rest_pos.x, pPeer->rest_pos.y}
                }, pPeer->netid);
                send_varlist(event.peer, ::VariantList{
                    "OnZoomCamera",
                    10000.0f,
                    1000u
                });
                send_varlist(event.peer, ::VariantList{
                    "OnSetFreezeState", 
                    0u
                }, pPeer->netid);
                
                // audio/teleport.wav
            }
            break;
        }
        case type::CHECKPOINT:
        {
            const ::pos previous_checkpoint = pPeer->rest_pos.by_32(true);
            if (!tile_in_bounds(previous_checkpoint.x, previous_checkpoint.y)) break;
            ::block &checkpoint = world->blocks[cord(previous_checkpoint.x, previous_checkpoint.y)]; // @note get previous checkpoint from respawn position

            checkpoint.state[2] &= ~S_TOGGLE;
            send_tile_update(event, ::gamePacket{.id = block.fg/*has to be 'block' or else iterfere with main door*/, .punch = pPeer->rest_pos.by_32(true)}, checkpoint, *world);

            pPeer->rest_pos = gamePacket.punch.by_32();
            block.state[2] |= S_TOGGLE; // @note toggle current checkpoint
            send_tile_update(event, ::gamePacket{.id = block.fg, .punch = gamePacket.punch}, block, *world);
            break;
        }
    }

    state_visuals(*event.peer, std::move(gamePacket)); // finished.
}
