#include "pch.hpp"
#include "onVariant/SetBux.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include <limits>
#include "core/event_bus.hpp"

#include "item_activate_object.hpp"

void item_activate_object(ENetEvent& event, ::gamePacket gamePacket) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    auto object = std::ranges::find(world->objects, static_cast<u_int>(gamePacket.id), &::object::uid);
    if (object == world->objects.end()) return; // @note already collected by someone else (or a stale/spoofed uid)

    if (object->id != 112/*gem*/)
    {
        const ::item &item = id_to_item(object->id);

        // @note a new item type needs a free backpack slot; leave the drop on the ground instead of overfilling the backpack.
        const auto held = std::ranges::find(pPeer->slots, object->id, &::slot::id);
        const bool has_room = held != pPeer->slots.end()
            ? held->count < 200
            : pPeer->slots.size() < static_cast<std::size_t>(std::max(pPeer->slot_size, 0));
        if (!has_room)
        {
            on::ConsoleMessage(event.peer, "`4Your backpack is full.``");
            return;
        }

        const u_short remember = object->count;
        const u_short remains = pPeer->emplace(::slot(object->id, object->count)); // @return remains after reaching 200
        const u_short collected = remember - remains;
        if (collected == 0) return;

        on::ConsoleMessage(event.peer, (item.rarity >= 999) ?
            std::format("Collected `w{} {}``.",                collected, item.raw_name) :
            std::format("Collected `w{} {}``. Rarity: `w{}``", collected, item.raw_name, item.rarity)
        );

        if (remains > 0)
        {
            // @note partial pickup: shrink the SAME object in place. (re-adding it merged it into itself and duplicated the leftover)
            object->count = remains;
            item_change_object(event, ::gamePacket{
                .netid = (int)0xfffffffd,
                .uid   = (int)object->uid,
                .count = static_cast<float>(object->count),
                .id    = object->id,
                .pos   = object->pos
            });
            world->save_objects();
            return;
        }
        object->count = 0;
    }
    else 
    {
        // @note gem pickup: clamp to INT_MAX so huge merged gem piles can't overflow.
        const long long room = static_cast<long long>(std::numeric_limits<signed>::max()) - std::max(pPeer->gems, 0);
        const int take = static_cast<int>(std::min<long long>(object->count, std::max(room, 0ll)));
        pPeer->gems += take;
        object->count -= take;
        on::SetBux(event);
        if (take > 0)
        {
            on::ConsoleMessage(event.peer, std::format("Collected `w{}`` gems.", take));
            event_bus::emit({ event_bus::type::item_changed, event.peer, {}, 112, take }); // @note quests & achievements. (gems cannot be dropped, so this can't be farmed)
        }
        if (object->count > 0)
        {
            on::ConsoleMessage(event.peer, "`4Your gem balance is full — leftover gems stay on the ground.``");
            item_change_object(event, ::gamePacket{
                .netid = (int)0xfffffffd,
                .uid   = (int)object->uid,
                .count = static_cast<float>(object->count),
                .id    = object->id,
                .pos   = object->pos
            });
            world->save_objects();
            return;
        }
    }
    remove_object(event, object->uid);

    world->objects.erase(object);
    world->save_objects();
}
