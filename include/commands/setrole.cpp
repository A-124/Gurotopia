#include "pch.hpp"

#include <charconv>
#include <cctype>

#include "onVariant/ConsoleMessage.hpp"
#include "setrole.hpp"

namespace
{
    bool parse_uid(std::string_view text, int &uid)
    {
        if (text.empty()) return false;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), uid);
        return error == std::errc{} && end == text.data() + text.size() && uid > 0;
    }

    bool parse_role(std::string_view text, u_char &role)
    {
        std::string value(text);
        for (char &character : value)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

        if (value == "0" || value == "player") role = PLAYER;
        else if (value == "1" || value == "mod" || value == "moderator") role = MODERATOR;
        else if (value == "2" || value == "dev" || value == "developer") role = DEVELOPER;
        else return false;
        return true;
    }
}

void setrole(ENetEvent &event, std::string_view text)
{
    auto *actor = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!actor || actor->role != DEVELOPER)
    {
        on::ConsoleMessage(event.peer, "`4You do not have permission to change player roles.``");
        return;
    }

    constexpr std::string_view prefix = "setrole ";
    if (!text.starts_with(prefix))
    {
        on::ConsoleMessage(event.peer, "`oUsage: /setrole <UID> <player|moderator|developer>``");
        return;
    }
    text.remove_prefix(prefix.size());

    const std::size_t separator = text.find_first_of(" \t");
    if (separator == std::string_view::npos)
    {
        on::ConsoleMessage(event.peer, "`oUsage: /setrole <UID> <player|moderator|developer>``");
        return;
    }

    const std::string_view uid_text = text.substr(0, separator);
    text.remove_prefix(separator);
    const std::size_t role_start = text.find_first_not_of(" \t");
    if (role_start == std::string_view::npos)
    {
        on::ConsoleMessage(event.peer, "`oUsage: /setrole <UID> <player|moderator|developer>``");
        return;
    }

    int uid{};
    u_char new_role{};
    if (!parse_uid(uid_text, uid) || !parse_role(text.substr(role_start), new_role))
    {
        on::ConsoleMessage(event.peer, "`4Invalid arguments. Use a valid UID and player, moderator, or developer.``");
        return;
    }

    for (ENetPeer *connection : peers())
    {
        if (!connection || !connection->data) continue;
        auto *target = static_cast<::peer*>(connection->data);
        if (target->user_id != uid) continue;
        if (target == actor)
        {
            on::ConsoleMessage(event.peer, "`4You cannot change your own role with this command.``");
            return;
        }

        target->role = new_role;
        target->mysql_update<signed>("role", static_cast<signed>(new_role));
        target->update_display_growid();

        constexpr std::array<std::string_view, 3> role_names{"Player", "Moderator", "Developer"};
        on::ConsoleMessage(connection, std::format("`2Your role is now {}. Re-enter the world to refresh your name tag.``", role_names[new_role]));
        on::ConsoleMessage(event.peer, std::format("`2Changed {} (UID {}) to {}.``", target->growid, uid, role_names[new_role]));
        return;
    }

    on::ConsoleMessage(event.peer, std::format("`4UID {} is not online.``", uid));
}
