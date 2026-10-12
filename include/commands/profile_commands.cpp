#include "pch.hpp"
#include "gameplay/profile_system.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "profile_commands.hpp"

void command_notebook(ENetEvent &event, std::string_view)
{
    ::hPipe pipe{ "buttonClicked|notebook_edit|" };
    profile_system::handle_popup(event, pipe);
}

void command_playtime(ENetEvent &event, std::string_view)
{
    auto *p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    on::ConsoleMessage(event.peer, std::format("`oPlay time: `w{}``   Account age: `w{}``",
        profile_system::playtime_text(*p), profile_system::account_age_text(*p)));
}
