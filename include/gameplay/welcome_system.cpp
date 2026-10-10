#include "pch.hpp"
#include <ctime>
#include <fstream>
#include "gameplay/welcome_system.hpp"
#include "gameplay/daily_system.hpp"
#include "gameplay/title_system.hpp"
#include "commands/help.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "tools/ui.hpp"

namespace welcome_system
{
namespace
{
    ::peer *peer_of(ENetEvent &event) { return (event.peer && event.peer->data) ? static_cast<::peer*>(event.peer->data) : nullptr; }

    void send_motd(ENetEvent &event)
    {
        std::ifstream file("resources/motd.txt");
        if (!file) return;
        int sent = 0;
        for (std::string line; sent < 5 && std::getline(file, line); )
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            on::ConsoleMessage(event.peer, ui::sanitize(line, 200));
            ++sent;
        }
    }

    void show(ENetEvent &event)
    {
        ::peer *p = peer_of(event);
        if (!p) return;

        std::string d = ui::header("Welcome to Gurotopia!", 18, std::format("Nice to meet you, `w{}``", p->display_growid));
        d += "add_textbox|Break blocks, build worlds, level up and unlock titles. Here is where to start:|left|\n";
        d += "add_spacer|small|\n";
        d += "add_button|welcome_help|`wCommand guide``|noflags|0|0|\n";
        d += "add_smalltext|Everything you can type in chat.|left|\n";
        d += "add_button|welcome_daily|`wDaily reward``|noflags|0|0|\n";
        d += "add_smalltext|Log in every day to build a streak and earn prizes.|left|\n";
        d += "add_button|welcome_titles|`wTitles``|noflags|0|0|\n";
        d += "add_smalltext|Unlock titles and wear them above your name.|left|\n";
        d += ui::footer("welcome_menu", "Start playing!", "");
        send_varlist(event.peer, { "OnDialogRequest", std::move(d) });
    }
}

void on_login(ENetEvent &event)
{
    ::peer *p = peer_of(event);
    if (!p || p->growid.empty()) return;

    send_motd(event);

    // @note only a really new account: never played before and created within the last day
    const std::time_t now = std::time(nullptr);
    if (p->playtime == 0 && p->created_at > 0 && now - p->created_at < 86400) show(event);
}

void command(ENetEvent &event, std::string_view) { show(event); }

void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe)
{
    const std::string button = pipe["buttonClicked"];
    if (button == "welcome_help") command_help(event, "");
    else if (button == "welcome_daily") daily_system::show(event);
    else if (button == "welcome_titles") title_system::show(event);
}
}
