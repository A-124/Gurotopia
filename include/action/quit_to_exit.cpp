#include "pch.hpp"
#include <chrono>
#include <deque>
#include "onVariant/RequestWorldSelectMenu.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "dialog_return/trade.hpp"
#include "quit_to_exit.hpp"

namespace
{
/* someone left: the others see the "left" bubble first, and the avatar is removed a moment later */
struct pending_remove
{
    std::chrono::steady_clock::time_point at;
    ENetPeer *to;
    int to_uid;
    std::string world, netid, pid;
    int leaver;
};
std::deque<pending_remove> pending{};

void send_remove(const pending_remove &r)
{
    if (r.to->state != ENET_PEER_STATE_CONNECTED || r.to->data == nullptr) return;
    const ::peer *t = static_cast<::peer*>(r.to->data);
    if (t->user_id != r.to_uid) return; // @note the slot was reused by another connection
    if (t->netid == 0 || t->recent_worlds.back() != r.world) return; // @note they left too, nothing to remove
    send_varlist(r.to, { "OnRemove", r.netid, r.pid });
}
}

bool g_silent_move = false;

/* the same account is entering this world again: finish its pending removals first so the new avatar isn't removed */
void leave_flush(const std::string &world, int user_id)
{
    for (auto it = pending.begin(); it != pending.end(); )
    {
        if (it->leaver == user_id && it->world == world) { send_remove(*it); it = pending.erase(it); }
        else ++it;
    }
}

/* every loop of the server */
void leave_pump()
{
    const auto now = std::chrono::steady_clock::now();
    while (!pending.empty() && pending.front().at <= now)
    {
        send_remove(pending.front());
        pending.pop_front();
    }
}

void action::quit_to_exit(ENetEvent& event, const std::string& header, bool skip_selection = false)
{
    if (!event.peer || !event.peer->data) return; // @note not logged in / already cleaned up
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    // @note a peer that is not inside a world (netid == 0) has nothing to leave.
    //       Without this guard a repeated quit_to_exit (client resend, trade cancel, kick + disconnect)
    //       decremented visitors again and could delete a world that still had players in it.
    //       Re-created world => netid_counter restarts at 1 => two players share a netid => they look like one player.
    if (pPeer->netid == 0) return;

    trade::close(event.peer, true); // @note leaving the world always cancels any open trade

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) { pPeer->netid = 0; return; } // @note peer was not in a world, therefore nothing to exit from.

    const std::string world_name = world->name;
    const bool hidden = (pPeer->state & S_INVISIBLE) == S_INVISIBLE || g_silent_move; // @note /invis players leave silently
    const std::string netid = std::format("netID|{}\n", pPeer->netid);
    const std::string pId = std::format("pId|{}\n", pPeer->user_id); // @note this is found during OnSpawn 'eid', the value is the same for user_id.
    const int leaver_netid = pPeer->netid;
    const auto when = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);

    pPeer->netid = 0; // @note this will fix any packets being sent outside of world; this can also be used to check if peer is not in a world.

    // @note count the REAL remaining players instead of trusting a counter that can drift.
    const std::vector<ENetPeer*> remaining = peers(world_name, PEER_SAME_WORLD);
    const int others = static_cast<int>(remaining.size());
    const std::string message = std::format("`5<{} left, `w{}`` others here>``", pPeer->display_growid, others);
    if (!hidden)
    {
        for (ENetPeer *other_p : remaining)
        {
            if (other_p == event.peer) continue;
            on::ConsoleMessage(other_p, message); // @note tell the OTHERS (was sent to the leaver before)
            send_varlist(other_p, { "OnTalkBubble", leaver_netid, message, 1u }); // @note same bubble as when someone enters
            pending.push_back({ when, other_p, static_cast<::peer*>(other_p->data)->user_id, world_name, netid, pId, pPeer->user_id });
        }
    }

    if (others <= 0)
    {
        // @note std::vector::erase shifts the elements after it, and ~world() only runs on the LAST element,
        //       so the world being removed would never be saved unless we do it explicitly here.
        world->save_metadata();
        world->save_provider_cooldowns();
        world->save_vending_machines();
        world->save_blocks();
        world->save_objects();
        worlds.erase(world); // @note nobody is left, delete memory copy of world.
    }
    else world->visitors = others;

    pPeer->update_display_growid();
    if (!skip_selection) on::RequestWorldSelectMenu(event);
}
