#include "pch.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "refresh_item_data.hpp"

void action::refresh_item_data(ENetEvent& event, const std::string& header) 
{
    on::ConsoleMessage(event.peer, "One moment, updating item data...");
    ::blob item_db_packet = compress_state(::gamePacket{
        .type = 0x10, // PACKET_SEND_ITEM_DATABASE_DATA
        .size = static_cast<u_int>(im_data.size())
    });
    item_db_packet.data().insert(item_db_packet.data().end(), im_data.begin(), im_data.end());

    ENetPacket *packet = enet_packet_create(
        item_db_packet.data().data(),
        item_db_packet.size(),
        ENET_PACKET_FLAG_RELIABLE
    );
    if (!packet)
    {
        puts("[items] failed to allocate item database packet");
        return;
    }
    const int result = enet_peer_send(event.peer, 0, packet);
    printf("[items] refresh request: packet=%zu bytes, enet_peer_send=%d\n", item_db_packet.size(), result);
}
