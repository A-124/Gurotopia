#include "pch.hpp"
#include "action/quit_to_exit.hpp"
#include "commands/trade.hpp"
#include "commands/legendary.hpp"
#include "quit.hpp"

void action::quit(ENetEvent& event, const std::string& header) 
{
    if (event.peer == nullptr) return;
    if (event.peer->data != nullptr) 
    {
        trade_end_for(*static_cast<::peer*>(event.peer->data), "left the game"); // @note also drops trade requests
        action::quit_to_exit(event, "", true);
        legend_save(*static_cast<::peer*>(event.peer->data)); // @note Legendary Quest progress and titles

        delete static_cast<::peer*>(event.peer->data);
        event.peer->data = nullptr;
    }
    enet_peer_reset(event.peer);
}
