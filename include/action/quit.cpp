#include "pch.hpp"
#include "action/quit_to_exit.hpp"
#include "dialog_return/trade.hpp"
#include "quit.hpp"

void action::quit(ENetEvent& event, const std::string& header) 
{
    if (event.peer == nullptr) return;
    if (event.peer->data != nullptr) 
    {
        trade::close(event.peer, true); // @note cancel any open trade first
        action::quit_to_exit(event, "", true);

        delete static_cast<::peer*>(event.peer->data);
        event.peer->data = nullptr;
    }
    else trade::close(event.peer, false); // @note drop any stale session keyed to this ENet slot
    enet_peer_reset(event.peer);
}
