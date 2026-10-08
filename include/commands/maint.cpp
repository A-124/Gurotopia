#include "pch.hpp"

#include "https/server_data.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include "maint.hpp"

/* /maint / /maintenance — fallback when the admin dialog itself is broken.
   /maint            -> show current state
   /maint on|off    -> set state (persists to server_data.php) */
void maint(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer || pPeer->role != DEVELOPER)
    {
        on::ConsoleMessage(event.peer, "`4You do not have permission to change maintenance mode.``");
        return;
    }

    std::string_view arg = text;
    constexpr std::string_view p1 = "maint ";
    constexpr std::string_view p2 = "maintenance ";
    if (arg.starts_with(p2)) arg.remove_prefix(p2.size());
    else if (arg.starts_with(p1)) arg.remove_prefix(p1.size());
    else arg = {};
    while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.remove_prefix(1);
    while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t')) arg.remove_suffix(1);
    std::string lower(arg);
    for (char &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower.empty())
    {
        on::ConsoleMessage(event.peer, gServer_data.maintenance ?
            "`4● Maintenance is ON (players blocked).`` Use `/maint off` to reopen." :
            "`2● Maintenance is OFF.`` Use `/maint on` to block players.");
        return;
    }
    if (lower == "on" || lower == "1" || lower == "enable")
    {
        gServer_data.maintenance = true;
        gServer_data.save_maintenance();
        on::ConsoleMessage(event.peer, "`4Maintenance mode ON — regular players can no longer log in.``");
    }
    else if (lower == "off" || lower == "0" || lower == "disable")
    {
        gServer_data.maintenance = false;
        gServer_data.save_maintenance();
        on::ConsoleMessage(event.peer, "`2Maintenance mode OFF — players can log in again.``");
    }
    else on::ConsoleMessage(event.peer, "`4Usage: /maint [on|off]``");
}
