#include "pch.hpp"
#include <chrono>
#include "onVariant/ConsoleMessage.hpp"
#include "tools/ui.hpp"
#include "tools/random.hpp"
#include "extras.hpp"

namespace
{
    const auto started = std::chrono::steady_clock::now(); // @note set when the server process loads

    ::peer *self_of(ENetEvent &event) { return (event.peer && event.peer->data) ? static_cast<::peer*>(event.peer->data) : nullptr; }
}

void command_flip(ENetEvent &event, std::string_view)
{
    ::peer *self = self_of(event);
    if (!self) return;
    if (self->netid == 0 || self->recent_worlds.back().empty())
    {
        on::ConsoleMessage(event.peer, "`4Enter a world before flipping a coin.``");
        return;
    }
    const bool heads = RandomRange(0, 2) == 0;
    const std::string message = std::format("`w{}`` flipped a coin: `${}``", self->display_growid, heads ? "HEADS" : "TAILS");
    peers(self->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p)
    {
        send_varlist(&p, { "OnTalkBubble", self->netid, message });
        on::ConsoleMessage(&p, message);
    });
}

void command_ping(ENetEvent &event, std::string_view)
{
    if (!self_of(event)) return;
    const unsigned rtt = event.peer->roundTripTime;
    const char *colour = rtt < 80 ? "`2" : rtt < 200 ? "`6" : "`4";
    on::ConsoleMessage(event.peer, std::format("`oYour ping: {}{} ms``", colour, rtt));
}

void command_uptime(ENetEvent &event, std::string_view)
{
    if (!self_of(event)) return;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
    on::ConsoleMessage(event.peer, std::format("`oServer uptime: `w{}``", ui::duration(static_cast<unsigned long long>(seconds))));
}

void command_count(ENetEvent &event, std::string_view)
{
    ::peer *self = self_of(event);
    if (!self) return;
    std::size_t online = 0, here = 0;
    for (ENetPeer *candidate : peers())
    {
        auto *p = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
        if (!p || p->growid.empty()) continue;
        ++online;
        if (self->netid != 0 && !self->recent_worlds.back().empty() && p->recent_worlds.back() == self->recent_worlds.back()) ++here;
    }
    on::ConsoleMessage(event.peer, std::format("`oPlayers online: `w{}``   In your world: `w{}``", online, here));
}
