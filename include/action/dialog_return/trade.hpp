#pragma once

#include <array>
#include <ctime>
#include <memory>
#include <unordered_map>

/* Real-time P2P trade, GT-style: wrench player -> Trade -> tap inventory
 * items to add them to your offer, both Accept.
 * NOTE: this is a server-driven dialog flow (custom UI), not an items.dat
 * feature — items.dat has no P2P trade window. Validation (ownership,
 * CAT_UNTRADEABLE, stack/space caps) is enforced server-side on every step. */
namespace trade
{
    struct offer
    {
        short item{};
        short count{};
    };

    struct session
    {
        ENetPeer *a{};
        ENetPeer *b{};
        std::array<offer, 4> offers_a{};
        std::array<offer, 4> offers_b{};
        bool accept_a{};
        bool accept_b{};
        bool locked{};
        int seq{}; // @note bumped on every mutation so each re-render payload differs (forces client redraw)
        std::time_t last_active{}; // @note last time anyone touched this trade (to detect abandoned trades)
    };

    /* one shared session keyed under BOTH peers, so either side's dialog
       return always hits the same state. */
    extern std::unordered_map<ENetPeer*, std::shared_ptr<session>> sessions;

    extern session *find(ENetPeer *p);
    extern void close(ENetPeer *p, bool notify = true);
    extern void open(ENetEvent &event, int target_netid);
    extern void handle(ENetEvent &event, const ::hPipe &hPipe);
}