#include "pch.hpp"
#include <charconv>
#include <ctime>
#include "onVariant/ConsoleMessage.hpp"
#include "gameplay/title_system.hpp"
#include "gameplay/profile_system.hpp"
#include "tools/ui.hpp"
#include "staff_tools.hpp"

namespace
{
    ::peer *self_of(ENetEvent &event) { return (event.peer && event.peer->data) ? static_cast<::peer*>(event.peer->data) : nullptr; }
    void say(ENetEvent &event, std::string message) { if (event.peer) on::ConsoleMessage(event.peer, std::move(message)); }

    std::string_view first_arg(std::string_view text, std::string_view &rest)
    {
        const std::size_t space = text.find_first_of(" \t");
        if (space == std::string_view::npos) { rest = {}; return {}; }
        text.remove_prefix(space);
        text.remove_prefix(std::min(text.size(), text.find_first_not_of(" \t")));
        const std::size_t end = text.find_first_of(" \t");
        rest = end == std::string_view::npos ? std::string_view{} : text.substr(end);
        rest.remove_prefix(std::min(rest.size(), rest.find_first_not_of(" \t")));
        return text.substr(0, end);
    }

    bool same_nocase(std::string_view a, std::string_view b)
    {
        return std::ranges::equal(a, b, [](unsigned char l, unsigned char r) { return std::tolower(l) == std::tolower(r); });
    }

    ENetPeer *find_online(std::string_view name)
    {
        for (ENetPeer *candidate : peers())
        {
            auto *p = (candidate && candidate->data) ? static_cast<::peer*>(candidate->data) : nullptr;
            if (p && !p->growid.empty() && same_nocase(p->growid, name)) return candidate;
        }
        return nullptr;
    }

    bool staff_only(ENetEvent &event, ::peer *self)
    {
        if (self && self->role >= MODERATOR) return true;
        say(event, "`4Only staff can use this command.``");
        return false;
    }
}

void command_mute(ENetEvent &event, std::string_view text)
{
    ::peer *self = self_of(event);
    if (!self || !staff_only(event, self)) return;

    std::string_view rest;
    const std::string_view who = first_arg(text, rest);
    if (who.empty()) { say(event, "`oUsage: /mute <player> [minutes 1-10080]`` (default 10)"); return; }

    int minutes = 10;
    if (!rest.empty())
    {
        const auto [end, ec] = std::from_chars(rest.data(), rest.data() + rest.size(), minutes);
        if (ec != std::errc{} || minutes < 1 || minutes > 10080) { say(event, "`4Minutes must be from 1 to 10080 (one week).``"); return; }
    }
    ENetPeer *target = find_online(who);
    auto *p = target ? static_cast<::peer*>(target->data) : nullptr;
    if (!p) { say(event, std::format("`4Player `w{}`` is not online.``", who)); return; }
    if (p == self) { say(event, "`4You cannot mute yourself.``"); return; }
    if (p->role >= self->role) { say(event, "`4You cannot mute someone with the same or a higher role.``"); return; }

    p->muted_until = static_cast<u_int>(std::time(nullptr)) + static_cast<u_int>(minutes) * 60u;
    p->save_moderation();
    on::ConsoleMessage(target, std::format("`4You were muted for {} minute{} by a staff member.``", minutes, minutes == 1 ? "" : "s"));
    say(event, std::format("`2Muted `w{}`` for {} minute{}.``", p->growid, minutes, minutes == 1 ? "" : "s"));
}

void command_unmute(ENetEvent &event, std::string_view text)
{
    ::peer *self = self_of(event);
    if (!self || !staff_only(event, self)) return;

    std::string_view rest;
    const std::string_view who = first_arg(text, rest);
    if (who.empty()) { say(event, "`oUsage: /unmute <player>``"); return; }
    ENetPeer *target = find_online(who);
    auto *p = target ? static_cast<::peer*>(target->data) : nullptr;
    if (!p) { say(event, std::format("`4Player `w{}`` is not online.``", who)); return; }

    p->muted_until = 0;
    p->save_moderation();
    on::ConsoleMessage(target, "`2You can talk again.``");
    say(event, std::format("`2Unmuted `w{}``.``", p->growid));
}

void command_pinfo(ENetEvent &event, std::string_view text)
{
    ::peer *self = self_of(event);
    if (!self || !staff_only(event, self)) return;

    std::string_view rest;
    const std::string_view who = first_arg(text, rest);
    if (who.empty()) { say(event, "`oUsage: /pinfo <player>`` (online players)"); return; }
    ENetPeer *target = find_online(who);
    auto *p = target ? static_cast<::peer*>(target->data) : nullptr;
    if (!p) { say(event, std::format("`4Player `w{}`` is not online.``", who)); return; }

    const std::time_t now = std::time(nullptr);
    const bool muted = p->muted_until > static_cast<u_int>(now);
    static constexpr std::string_view roles[] = { "Player", "Moderator", "Developer" };
    say(event, std::format("`w{}`` `o(UID {}, {})``", p->growid, p->user_id, roles[std::min<std::size_t>(p->role, 2)]));
    say(event, std::format("`oLevel `w{}``  Gems `w{}``  Play time `w{}``  Account age `w{}``",
        p->level.front(), p->gems, profile_system::playtime_text(*p), profile_system::account_age_text(*p)));
    say(event, std::format("`oWorld `w{}``  Muted {}  Ping `w{} ms``",
        p->recent_worlds.back().empty() ? "none" : p->recent_worlds.back(),
        muted ? std::format("`4{}s left``", p->muted_until - static_cast<u_int>(now)) : std::string{"`2no``"},
        target->roundTripTime));
}

void command_nick(ENetEvent &event, std::string_view text)
{
    ::peer *self = self_of(event);
    if (!self || !staff_only(event, self)) return;

    std::string_view rest;
    first_arg(text, rest); // @note skip "nick"
    const std::size_t space = text.find_first_of(" \t");
    std::string wanted = space == std::string_view::npos ? std::string{} : std::string{ text.substr(space) };
    std::erase(wanted, '`'); // @note no colour codes: staff must not fake other roles
    wanted = ui::sanitize(wanted, 18);
    if (wanted.size() < 3) { say(event, "`oUsage: /nick <name>`` (3-18 characters, letters and numbers). Use /default to go back."); return; }

    // @note never allow copying a real account: impersonating another player is exactly what this must not become
    if (ENetPeer *taken = find_online(wanted); taken && taken != event.peer)
    { say(event, "`4That name belongs to another player.``"); return; }

    self->nick = wanted;
    title_system::broadcast_name(event.peer);
    say(event, std::format("`2Your name is now `w{}``. Use /default to restore it.``", wanted));
}

void command_default(ENetEvent &event, std::string_view)
{
    ::peer *self = self_of(event);
    if (!self) return;
    if (self->nick.empty()) { say(event, "`oYou are already using your real name.``"); return; }
    self->nick.clear();
    title_system::broadcast_name(event.peer);
    say(event, "`2Your real name is back.``");
}

void command_status(ENetEvent &event, std::string_view)
{
    ::peer *self = self_of(event);
    if (!self) return;
    say(event, std::format("`wLevel {}`` `o(XP {})``   Gems `w{}``   Play time `w{}``",
        self->level.front(), self->level.back(), self->gems, profile_system::playtime_text(*self)));
    say(event, std::format("`oWorld: `w{}``   Titles: `w{}/{}``   Daily streak: `w{}``",
        self->recent_worlds.back().empty() ? "none" : self->recent_worlds.back(),
        title_system::owned_count(*self), title_system::all().size(), self->daily_streak));
}
