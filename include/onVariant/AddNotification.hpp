#pragma once

namespace on
{
    /* @brief the toast that slides in at the top of the screen (same as Growtopia's "OnAddNotification"). */
    extern void AddNotification(ENetPeer *peer, const std::string &message, const std::string &audio = "audio/hub_open.wav");
}
