#include "pch.hpp"
#include "onVariant/SetBux.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include <limits>

#include "item_activate_object.hpp"

void item_activate_object(ENetEvent& event, ::gamePacket gamePacket) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    auto object = std::ranges::find(world->objects, gamePacket.id, &::object::uid);
    if (object->id != 112/*gem*/)
    {
        const ::item &item = id_to_item(object->id);

        u_short remember = object->count;
        object->count = pPeer->emplace(::slot(object->id, object->count)); // @return remains after reaching 200
        if (object->count > 0)
        {
            add_object(event, ::slot(object->id, object->count), object->pos, *world);
        }
        u_short collected = remember - object->count;
        if (collected ==/*unsigned*/ 0) return; // @todo

        on::ConsoleMessage(event.peer, (item.rarity >= 999) ?
            std::format("Collected `w{} {}``.",                collected, item.raw_name) :
            std::format("Collected `w{} {}``. Rarity: `w{}``", collected, item.raw_name, item.rarity)
        );
    }
    else 
    {
        // @note gem pickup: clamp to INT_MAX so huge merged gem piles can't overflow.
        const long long room = static_cast<long long>(std::numeric_limits<signed>::max()) - std::max(pPeer->gems, 0);
        const int take = static_cast<int>(std::min<long long>(object->count, std::max(room, 0ll)));
        pPeer->gems += take;
        object->count -= take;
        on::SetBux(event);
        if (take > 0) on::ConsoleMessage(event.peer, std::format("Collected `w{}`` gems.", take));
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

    if (object->count == 0) world->objects.erase(object);
    world->save_objects();
}
