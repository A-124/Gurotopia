#include "pch.hpp"
#include <charconv>
#include <fstream>
#include "onVariant/ConsoleMessage.hpp"
#include "tools/create_dialog.hpp"
#include "tools/random.hpp"
#include "commands/moderation.hpp" // @note username_for_uid()
#include "social.hpp"

namespace
{
    void say(ENetEvent &event, std::string message)
    {
        if (event.peer) on::ConsoleMessage(event.peer, std::move(message));
    }

    /* "msg bob hello there" -> args: "bob hello there" */
    std::string_view after_command(std::string_view text)
    {
        const std::size_t space = text.find_first_of(" \t");
        if (space == std::string_view::npos) return {};
        text.remove_prefix(space);
        const std::size_t start = text.find_first_not_of(" \t");
        return (start == std::string_view::npos) ? std::string_view{} : text.substr(start);
    }

    bool same_text_nocase(std::string_view a, std::string_view b)
    {
        return std::ranges::equal(a, b, [](unsigned char l, unsigned char r) { return std::tolower(l) == std::tolower(r); });
    }

    bool starts_with_nocase(std::string_view text, std::string_view prefix)
    {
        return text.size() >= prefix.size() && same_text_nocase(text.substr(0, prefix.size()), prefix);
    }

    /* online players only. exact (case-insensitive) match wins, otherwise a UNIQUE prefix match. */
    enum class lookup { found, none, ambiguous };
    lookup find_online(std::string_view query, ENetPeer *&result)
    {
        result = nullptr;
        ENetPeer *prefix_hit = nullptr;
        int prefix_hits = 0;
        for (ENetPeer *candidate : peers())
        {
            auto *p = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
            if (!p || p->growid.empty()) continue; // @note not logged in yet
            if (same_text_nocase(p->growid, query)) { result = candidate; return lookup::found; }
            if (starts_with_nocase(p->growid, query)) { prefix_hit = candidate; ++prefix_hits; }
        }
        if (prefix_hits == 1) { result = prefix_hit; return lookup::found; }
        return prefix_hits > 1 ? lookup::ambiguous : lookup::none;
    }

    std::string role_color(u_char role) { return role == DEVELOPER ? "`b" : "`#"; }
}

void command_msg(ENetEvent &event, std::string_view text)
{
    auto *self = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!self) return;

    const std::string_view rest = after_command(text);
    const std::size_t split = rest.find_first_of(" \t");
    if (rest.empty() || split == std::string_view::npos)
    {
        say(event, "`oUsage: /msg <player> <message>``");
        return;
    }
    const std::string_view who = rest.substr(0, split);
    std::string_view message = rest.substr(split);
    message.remove_prefix(message.find_first_not_of(" \t"));
    if (message.empty()) { say(event, "`oUsage: /msg <player> <message>``"); return; }

    ENetPeer *target_peer{};
    switch (find_online(who, target_peer))
    {
        case lookup::none: say(event, std::format("`4Player `w{}`` is not online.``", who)); return;
        case lookup::ambiguous: say(event, std::format("`4More than one online player starts with `w{}``. Type more of the name.``", who)); return;
        case lookup::found: break;
    }
    auto *target = static_cast<::peer*>(target_peer->data);
    if (target == self) { say(event, "`4You can't message yourself.``"); return; }

    const std::string where = (self->netid != 0 && !self->recent_worlds.back().empty()) ? self->recent_worlds.back() : "EXIT";
    target->reply_uid = self->user_id;
    on::ConsoleMessage(target_peer, std::format("`c>> from (`w{}```c in `${}```c) > `o{}``", self->display_growid, where, message));
    say(event, std::format("`6>> (Sent to `w{}```6)``", target->display_growid));
}

void command_reply(ENetEvent &event, std::string_view text)
{
    auto *self = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!self) return;

    const std::string_view message = after_command(text);
    if (message.empty()) { say(event, "`oUsage: /r <message>``"); return; }
    if (self->reply_uid == 0) { say(event, "`4Nobody has messaged you yet.`` Use `$/msg <player> <message>`` first."); return; }

    ENetPeer *target_peer{};
    for (ENetPeer *candidate : peers())
    {
        auto *p = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
        if (p && p->user_id == self->reply_uid) { target_peer = candidate; break; }
    }
    if (!target_peer) { say(event, "`4That player is no longer online.``"); return; }

    auto *target = static_cast<::peer*>(target_peer->data);
    const std::string where = (self->netid != 0 && !self->recent_worlds.back().empty()) ? self->recent_worlds.back() : "EXIT";
    target->reply_uid = self->user_id;
    on::ConsoleMessage(target_peer, std::format("`c>> from (`w{}```c in `${}```c) > `o{}``", self->display_growid, where, message));
    say(event, std::format("`6>> (Sent to `w{}```6)``", target->display_growid));
}

void command_mods(ENetEvent &event, std::string_view)
{
    std::vector<std::string> names;
    for (ENetPeer *candidate : peers())
    {
        auto *p = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
        if (p && p->role >= MODERATOR && !p->growid.empty())
            names.emplace_back(std::format("{}{}``", role_color(p->role), p->growid));
    }
    if (names.empty()) say(event, "`oNo moderators are online right now.``");
    else say(event, std::format("`wStaff online ({}):`` {}", names.size(), join(names, ", ")));
}

void command_rules(ENetEvent &event, std::string_view)
{
    if (!event.peer) return;

    // @note servers can customize the rules without recompiling: one rule per line in resources/rules.txt
    std::vector<std::string> rules;
    if (std::ifstream file{"resources/rules.txt"})
        for (std::string line; std::getline(file, line) && rules.size() < 20; )
        {
            while (!line.empty() && (line.back() == '\r' || std::isspace(static_cast<unsigned char>(line.back())))) line.pop_back();
            std::ranges::replace(line, '|', '/'); // @note '|' is the dialog field separator
            if (!line.empty() && line.front() != '#') rules.emplace_back(std::move(line));
        }
    if (rules.empty())
        rules = {
            "Be respectful. No harassment, hate speech or spamming.",
            "No scamming other players in trades or vending machines.",
            "No cheating, exploiting bugs, or using bots/macros. Report bugs to the staff instead.",
            "Do not share your password with anyone, staff will never ask for it.",
            "Staff decisions are final. Use /mods to see who is online."
        };

    ::create_dialog dialog = ::create_dialog()
        .set_default_color("`o")
        .add_label_with_icon("big", "`wServer Rules``", 32)
        .add_spacer("small");
    int number = 1;
    for (const std::string &rule : rules)
        dialog.add_textbox(std::format("`w{}.`` {}", number++, rule));

    send_varlist(event.peer, { "OnDialogRequest", dialog.add_spacer("small").end_dialog("rules", "", "I Agree") });
}

void command_worldinfo(ENetEvent &event, std::string_view)
{
    auto *self = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!self) return;
    if (self->netid == 0 || self->recent_worlds.back().empty()) { say(event, "`4Enter a world first.``"); return; }

    const auto found = std::ranges::find(worlds, self->recent_worlds.back(), &::world::name);
    if (found == worlds.end()) return;
    const ::world &world = *found;

    std::string owner = username_for_uid(world.owner);
    if (world.owner == 0) owner = "`oNobody (unlocked)``";
    else if (owner.empty()) owner = "`ounknown``";
    else owner = std::format("`w{}``", owner);

    const int access_count = static_cast<int>(std::ranges::count_if(world.access, [](int uid) { return uid != 0; }));

    send_varlist(event.peer, {
        "OnDialogRequest",
        ::create_dialog()
            .set_default_color("`o")
            .add_label_with_icon("big", std::format("`w{}``", world.name), 242/*World Lock*/)
            .add_spacer("small")
            .add_textbox(std::format("Owner: {}", owner))
            .add_textbox(std::format("Players here: `w{}``", world.visitors))
            .add_textbox(std::format("Build access: `w{}``", world.owner == 0 ? "everyone" : (world.is_public ? "public (anyone can build)" : "owner and access list only")))
            .add_textbox(std::format("Players on access list: `w{}``", access_count))
            .add_textbox(std::format("Minimum entry level: `w{}``", world.minimum_entry_level))
            .add_textbox(std::format("Weather: `w{}``", world.weather_id() == 0 ? "none" : std::format("#{}", world.weather_id())))
            .add_spacer("small")
            .end_dialog("worldinfo", "", "Close")
    });
}

void command_roll(ENetEvent &event, std::string_view text)
{
    auto *self = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!self) return;
    if (self->netid == 0 || self->recent_worlds.back().empty()) { say(event, "`4Enter a world before rolling.``"); return; }

    int sides = 6;
    if (const std::string_view arg = after_command(text); !arg.empty())
    {
        int parsed{};
        const auto [end, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), parsed);
        if (ec != std::errc{} || end != arg.data() + arg.size() || parsed < 2 || parsed > 1000)
        {
            say(event, "`oUsage: /roll [sides 2-1000]`` (default 6)");
            return;
        }
        sides = parsed;
    }
    const int result = RandomRange(1, sides + 1); // @note upper bound is exclusive
    const std::string message = std::format("`w{}`` rolled a `${}`` `o(1-{})``", self->display_growid, result, sides);
    peers(self->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p)
    {
        send_varlist(&p, { "OnTalkBubble", self->netid, message });
        on::ConsoleMessage(&p, message);
    });
}
