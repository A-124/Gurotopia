#include "pch.hpp"
#include "onVariant/EmoticonDataChanged.hpp"
#include "onVariant/Spawn.hpp"
#include "onVariant/BillboardChange.hpp"
#include "onVariant/SetClothing.hpp"
#include "onVariant/CountryState.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "commands/weather.hpp"
#include "commands/lockaccess.hpp"
#include "commands/curse.hpp"
#include "commands/bot.hpp"
#include "commands/jammers.hpp"
#include "tools/time.hpp"
#include "commands/moderation.hpp"
#include "quit_to_exit.hpp"
#include "core/event_bus.hpp"

#include "join_request.hpp"

static std::unordered_map<int, ::pos> arrivals{}; // @note where an account should appear instead of the door (summon, pull)
void arrive_at(int user_id, ::pos at) { arrivals[user_id] = at; }

void action::join_request(ENetEvent& event, const std::string& header, const std::string_view world_name = "")
{
    try
    {
        ::peer *pPeer = static_cast<::peer*>(event.peer->data);
        ::hPipe hPipe{ header };

        std::string name =  hPipe["name"];
        if (name.empty() && !world_name.empty()) name = world_name;

        if (name.length() > 24) throw std::runtime_error(""); // @note impossible unless using a proxy since client caps at 24
        if (!alnum(name)) throw std::runtime_error("Sorry, spaces and special characters are not allowed in world or door names.  Try again.");

        for (char &c : name) c = std::toupper(c); // @note start -> START

        // @note /curse: a cursed player can only be in HELL
        if (pPeer->curse_until > std::time(nullptr) && name != "HELL")
        {
            on::ConsoleMessage(event.peer, std::format("`4You are cursed!`` You can't leave `wHELL`` for another `w{}``.", curse_time_left(*pPeer)));
            name = "HELL";
        }
        if (name == "HELL" && pPeer->curse_until <= std::time(nullptr) && pPeer->role < MODERATOR)
            throw std::runtime_error("`4Only the cursed may enter HELL.``");

        if (is_world_banned(name, pPeer->user_id))
            throw std::runtime_error("You are banned from this world.");

        // @note already inside a world (join without quit_to_exit)? leave it properly first,
        //       otherwise the old world keeps a ghost visitor and the netid is reused.
        if (pPeer->netid != 0) action::quit_to_exit(event, "", true);

        auto it = std::ranges::find(worlds, name, &::world::name);
        const bool created = it == worlds.end();
        if (created)
            it = worlds.emplace(worlds.end(), name);

        ::world &world = *it;
        if (world.owner != pPeer->user_id && pPeer->role == PLAYER &&
            std::ranges::find(world.access, pPeer->user_id) == world.access.end() &&
            pPeer->level[0] < world.minimum_entry_level)
        {
            const int min_level = world.minimum_entry_level;
            if (created) worlds.erase(it); // @note don't leave an empty world behind
            throw std::runtime_error(std::format(
                "You need to be level {} to enter this world.", min_level));
        }

        if (world.nuked && pPeer->role < MODERATOR && world.name != "HELL")
            throw std::runtime_error("`4That world has been nuked and is closed.``");

        {
            ::blob blob = compress_state(::gamePacket{ .type = 0x04, /*PACKET_SEND_MAP_DATA*/ .state = state::S_EXTENDED });
            blob.push_back(world.serialize());

            ENetPacket *packet = enet_packet_create(blob.data().data(), blob.size(), ENET_PACKET_FLAG_RELIABLE);
            if (enet_peer_send(event.peer, 0, packet)) enet_packet_destroy(packet);
        } // @note delete blob
        {
            std::string *this_world = std::ranges::find(pPeer->recent_worlds, world.name);
            std::string *end = pPeer->recent_worlds.end();
            std::string *first = this_world != end ? this_world : pPeer->recent_worlds.begin();

            std::rotate(first, first + 1, end);
            pPeer->recent_worlds.back() = world.name;
        } // @note delete name, first
        on::EmoticonDataChanged(event);

        pPeer->display_growid = display_name_for(*pPeer, world); // @note /nick and titles (Dr., of Legend) included

        pPeer->rest_pos = world.spawn;
        if (auto at = arrivals.find(pPeer->user_id); at != arrivals.end()) { pPeer->rest_pos = at->second; arrivals.erase(at); } // @note summoned / pulled: appear where they were sent
        pPeer->pos = pPeer->rest_pos;
        leave_flush(world.name, pPeer->user_id); // @note if this account left this world a moment ago, finish that first

        // @note snapshot the world roster FIRST (joiner has netid 0 so is not in it yet)
        pPeer->netid = 0;
        std::vector<ENetPeer*> roster = peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD);
        const int others_here = static_cast<int>(roster.size());

        // @note netid must be unique inside the world. world.netid_counter restarts when the world is
        //       re-created (everyone left, /reset, ...) while players may still hold old netids,
        //       and two players with the same netid are drawn as ONE player.
        const auto netid_taken = [&roster](int id)
        {
            for (ENetPeer *p : roster)
                if (static_cast<::peer*>(p->data)->netid == id) return true;
            return false;
        };
        do
        {
            if (++world.netid_counter <= 0) world.netid_counter = 1; // @note never hand out netid 0 (peers() treats it as "not in world")
        } while (netid_taken(world.netid_counter));
        pPeer->netid = world.netid_counter;
        roster.push_back(event.peer); // @note joiner is skipped by user_id below, kept so the loop sees the full list

        const bool me_hidden = (pPeer->state & S_INVISIBLE) == S_INVISIBLE; // @note /invis: nobody sees you arrive
        for (ENetPeer *other_p : roster)
        {
            ENetPeer &peer = *other_p;
            ::peer *pOthers = static_cast<::peer*>(peer.data); // @note everyone in world's peer.data
            if (pOthers->user_id == pPeer->user_id) continue;

            const bool other_hidden = (pOthers->state & S_INVISIBLE) == S_INVISIBLE; // @note hidden players still see everyone; nobody sees them
            if (!other_hidden)
            {
                on::Spawn(*event.peer, pOthers->netid, pOthers->user_id, pOthers->pos, pOthers->nametag(), pOthers->country, pOthers->role, pOthers->role >= DEVELOPER, false);
                on::SetClothing(*event.peer, *pOthers);
            }
            if (me_hidden) continue;

            on::Spawn(peer, pPeer->netid, pPeer->user_id, pPeer->rest_pos, pPeer->nametag(), pPeer->country, pPeer->role, pPeer->role >= DEVELOPER, false);
            on::SetClothing(peer, *pPeer);
            if (!g_silent_move)
            {
                const std::string entered = std::format("`5<{} entered, `w{}`` others here>``", pPeer->display_growid, others_here);
                on::ConsoleMessage(&peer, entered);
                // @note the reason this is here is cause we need the peer's OnSpawn to happen before OnTalkBubble
                send_varlist(&peer, { "OnTalkBubble", pPeer->netid, entered, 1u });
            }
        } // @note roster loop: joiner learns every existing peer, each existing peer learns the joiner
        spawn_bots_for(event, world.name);
        on::Spawn(*event.peer, pPeer->netid, pPeer->user_id, pPeer->rest_pos, pPeer->nametag(), pPeer->country, pPeer->role, pPeer->role >= DEVELOPER, true);

        if (pPeer->billboard.id != 0) on::BillboardChange(event); // @note don't waste memory if billboard is empty.

        send_varlist(event.peer, {
            "OnSetPos",
            CL_Vec2f{pPeer->rest_pos.x, pPeer->rest_pos.y}
        }, pPeer->netid);

        if (!g_silent_move) on::ConsoleMessage(event.peer,
            std::format(
                "World `w{}`` entered.  There are `w{}`` other people here, `w{}`` online.",
                world.name, others_here, peers().size()
            )
        );
        world.visitors = others_here + 1; // @note real head count, not a drifting counter

        // @note like the real game: tell visitors who owns a locked world
        if (world.owner != 0)
        {
            const std::string owner_name = username_for_uid(world.owner);
            on::ConsoleMessage(event.peer, std::format("`5[```w{}`` `$World Locked`` by {}`5]``", world.name, owner_name.empty() ? "unknown" : owner_name) + jam_suffix(world.name));
        }
        on::SetClothing(*event.peer);
        on::CountryState(event);

        // @note restore the world's weather for the joiner (saved with the weather machine's toggle state)
        if (const int weather_id = world.weather_id(); weather_id != 0 || world.weather != ::pos{})
            send_varlist(event.peer, { "OnSetCurrentWeather", weather_id });

        event_bus::emit({ event_bus::type::player_entered_world, event.peer, world.name }); // @note quests & achievements
    }
    catch (const std::exception& exc)
    {
        send_varlist(event.peer, { "OnFailedToEnterWorld" });
        if (const std::string_view msg{ exc.what() }; !msg.empty()) on::ConsoleMessage(event.peer, std::string{ msg }); // @note tell the player why
        return;
    }
}
