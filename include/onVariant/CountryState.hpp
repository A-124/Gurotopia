#pragma once

namespace on
{
    extern void CountryState(ENetEvent& event);
    extern void CountryStateOf(ENetPeer &to, const ::peer &who); // @note someone's flag, sent to one player
}
