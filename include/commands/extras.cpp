#include "pch.hpp"
#include <chrono>
#include <unordered_map>
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/NameChanged.hpp"
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

void command_rainbow(ENetEvent &event, std::string_view)
{
    ::peer *self = self_of(event);
    if (!self) return;
    if (!self->nick.empty())
    {
        on::ConsoleMessage(event.peer, "Clear your custom staff nickname before using /rainbow.");
        return;
    }
    self->rainbow_name = !self->rainbow_name;
    on::NameChanged(event);
    on::ConsoleMessage(event.peer, self->rainbow_name
        ? "Rainbow nickname enabled for this session."
        : "Rainbow nickname disabled.");
}

namespace
{
    std::unordered_map<int, std::chrono::steady_clock::time_point> legendary_broadcast_cooldowns;
}
void command_lsb(ENetEvent &event, std::string_view text)
{
    ::peer *self = self_of(event);
    if (!self) return;
    const auto split = text.find_first_of(" \t");
    std::string_view message = split == std::string_view::npos ? std::string_view{} : text.substr(split + 1);
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.front()))) message.remove_prefix(1);
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.back()))) message.remove_suffix(1);
    if (message.empty())
    {
        on::ConsoleMessage(event.peer, "Usage: /lsb <message> (requires Legendary Mask, item 25550).");
        return;
    }
    const auto mask = std::ranges::find(self->slots, static_cast<short>(25550), &::slot::id);
    if (mask == self->slots.end() || mask->count <= 0)
    {
        on::ConsoleMessage(event.peer, "You need a Legendary Mask (item 25550) to use /lsb.");
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (const auto it = legendary_broadcast_cooldowns.find(self->user_id); it != legendary_broadcast_cooldowns.end())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
        if (elapsed < 300)
        {
            const auto remaining = 300 - elapsed;
            on::ConsoleMessage(event.peer, std::format("Legendary broadcast cooldown: {}m {}s.", remaining / 60, remaining % 60));
            return;
        }
    }
    const std::string world = self->recent_worlds.back().empty() ? "EXIT" : self->recent_worlds.back();
    const std::string announcement = std::format("`5[LEGEND-SB] from ({} `5) in [`o{}`5]: `o{}", self->nametag(), world, message);
    legendary_broadcast_cooldowns[self->user_id] = now;
    for (ENetPeer *candidate : peers())
    {
        auto *target = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
        if (target && !target->growid.empty()) on::ConsoleMessage(candidate, announcement);
    }
}
