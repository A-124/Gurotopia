#include "pch.hpp"
#include <cctype>
#include "gameplay/title_system.hpp"
#include "gameplay/profile_system.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "title_commands.hpp"

namespace
{
    bool same_nocase(std::string_view a, std::string_view b)
    {
        return std::ranges::equal(a, b, [](unsigned char l, unsigned char r) { return std::tolower(l) == std::tolower(r); });
    }
}

void command_titles(ENetEvent &event, std::string_view) { title_system::show(event); }

void command_notebook(ENetEvent &event, std::string_view)
{
    ::hPipe pipe{ "buttonClicked|notebook_edit|" };
    profile_system::handle_popup(event, pipe);
}

void command_playtime(ENetEvent &event, std::string_view)
{
    auto *p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    on::ConsoleMessage(event.peer, std::format("`oPlay time: `w{}``   Account age: `w{}``   Titles: `w{}/{}``",
        profile_system::playtime_text(*p), profile_system::account_age_text(*p), title_system::owned_count(*p), title_system::all().size()));
}

void command_givetitle(ENetEvent &event, std::string_view text)
{
    auto *self = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!self) return;
    const auto say = [&](std::string message) { on::ConsoleMessage(event.peer, message); };

    // "givetitle bob 12"
    std::vector<std::string> words = readch(std::string{text}, ' ');
    std::erase_if(words, [](const std::string &w) { return w.empty(); });
    if (words.size() < 3) { say("`oUsage: /givetitle <player|me> <title id>`` - see resources/titles.txt"); return; }

    const int id = std::atoi(words[2].c_str());
    if (!title_system::find(id)) { say(std::format("`4No title with id `w{}``.``", id)); return; }

    ENetPeer *target = nullptr;
    for (ENetPeer *candidate : peers())
    {
        auto *p = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
        if (!p || p->growid.empty()) continue;
        if (same_nocase(p->growid, words[1]) || (same_nocase(words[1], "me") && p == self)) { target = candidate; break; }
    }
    if (!target) { say(std::format("`4Player `w{}`` is not online.``", words[1])); return; }

    title_system::grant(target, id);
    say(std::format("`2Gave title `w{}`` to `w{}``.``", title_system::find(id)->name, static_cast<::peer*>(target->data)->growid));
}
