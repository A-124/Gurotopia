#include "pch.hpp"

#include "AddNotification.hpp"

void on::AddNotification(ENetPeer *peer, const std::string &message, const std::string &audio)
{
    if (!peer) return;
    send_varlist(peer, { "OnAddNotification", std::string{ "interface/atlas_buttons.rttex" }, message, audio, 0 });
}
