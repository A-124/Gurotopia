#include "pch.hpp"
#include "onVariant/RequestWorldSelectMenu.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "dialog_return/trade.hpp"
#include "quit_to_exit.hpp"

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
    std::string netid = std::format("netID|{}\n", pPeer->netid);
    std::string pId = std::format("pId|{}\n", pPeer->user_id); // @note this is found during OnSpawn 'eid', the value is the same for user_id.

    pPeer->netid = 0; // @note this will fix any packets being sent outside of world; this can also be used to check if peer is not in a world.

    // @note count the REAL remaining players instead of trusting a counter that can drift.
    const std::vector<ENetPeer*> remaining = peers(world_name, PEER_SAME_WORLD);
    const int others = static_cast<int>(remaining.size());
    const std::string message = std::format("`5<{} left, `w{}`` others here>``", pPeer->display_growid, others);
    for (ENetPeer *other_p : remaining)
    {
        if (other_p == event.peer) continue;
        on::ConsoleMessage(other_p, message); // @note tell the OTHERS (was sent to the leaver before)
        send_varlist(other_p, { "OnRemove", netid, pId });
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
