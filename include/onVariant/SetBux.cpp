#include "pch.hpp"
#include "SetBux.hpp"

void on::SetBux(ENetPeer& peer)
{
    ::peer *pPeer = static_cast<::peer*>(peer.data);
    if (!pPeer) return;

    pPeer->gems = std::clamp(pPeer->gems, 0, std::numeric_limits<signed>::max());
    pPeer->mysql_update<signed>("gems", pPeer->gems);

    send_varlist(&peer, { "OnSetBux", pPeer->gems, 1, 1 });
}

void on::SetBux(ENetEvent& event)
{
    if (event.peer) SetBux(*event.peer);
}
