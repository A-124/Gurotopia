#include "pch.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "refresh_item_data.hpp"

void action::refresh_item_data(ENetEvent& event, const std::string& header) 
{
    on::ConsoleMessage(event.peer, "One moment, updating item data...");
    ENetPacket *packet = enet_packet_create(im_data.data(), im_data.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet)
    {
        puts("[items] failed to allocate item database packet");
        return;
    }
    const int result = enet_peer_send(event.peer, 0, packet);
    printf("[items] refresh request: packet=%zu bytes, enet_peer_send=%d\n", im_data.size(), result);
}
