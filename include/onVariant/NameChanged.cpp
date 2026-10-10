#include "pch.hpp"
#include "NameChanged.hpp"

void on::NameChanged(ENetEvent& event) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    send_varlist(event.peer, {
        "OnNameChanged",
        pPeer->nametag()
    }, pPeer->netid);
}