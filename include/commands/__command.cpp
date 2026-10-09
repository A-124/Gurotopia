#include "pch.hpp"
#include "onVariant/Action.hpp"
#include "admin.hpp"
#include "maint.hpp"
#include "resetworld.hpp"
#include "resetallworld.hpp"
#include "ready.hpp"
#include "setrole.hpp"
#include "time.hpp"
#include "find.hpp"
#include "warp.hpp"
#include "punch.hpp"
#include "skin.hpp"
#include "sb.hpp"
#include "who.hpp"
#include "me.hpp"
#include "news.hpp"
#include "weather.hpp"
#include "ghost.hpp"
#include "__command.hpp"
#include "stats.hpp"
#include "event_manager.hpp"
#include "reload.hpp"
#include "gameplay/content_commands.hpp"
#include "gameplay/craft.hpp"
#include "moderation.hpp"

/* emote commands all dispatch to on::Action. listed once here so the
 * cmd_pool registration and the /help text stay in sync automatically. */
static constexpr std::string_view emotes[24]{
    "wave", "dance", "love", "sleep", "facepalm", "fp",
    "smh", "yes", "no", "omg", "idk", "shrug",
    "furious", "rolleyes", "foldarms", "fa", "stubborn", "fold",
    "dab", "sassy", "dance2", "march", "grumpy", "shy"
};

/* Commands that require an argument are rejected before dispatch when omitted. */
std::array<std::string_view, 13> cmd_requires_arg{
    "sb", "warp", "punch", "skin", "me", "weather", "setrole", "resetallworld",
    "setlevel", "kick", "ban", "unban", "pull"
};

/* /help is assembled from the caller's actual role and current world access. */
auto help_return = [](ENetEvent& event, const std::string_view) 
{
    if (!event.peer) return;
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    std::string list =
        "/help /? /time /sb <message> /find /warp <world> /punch <id> /skin <id> /who /me <message> "
        "/news /weather <id> /ghost /online /on /event /content /craft <item_id> [amount]";

    if (pPeer->role == DEVELOPER)
    {
        list += " /admin /maint [on|off] /maintenance [on|off] /resetworld /resetallworld confirm /ready "
                "/setrole <UID> <role> /setlevel <player|UID> <level> /kick <player|UID> "
                "/ban <player|UID> /unban <player|UID> /pull <player|UID> "
                "/reload <items|content|store|holiday|all> /startmultiplier <gem> <xp> <seconds> /stopmultiplier";
    }
    else if (pPeer->netid != 0 && !pPeer->recent_worlds.back().empty())
    {
        auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
        if (world != worlds.end() && (world->owner == pPeer->user_id ||
            std::ranges::find(world->access, pPeer->user_id) != world->access.end()))
            list += " /kick <player|UID> /ban <player|UID> /unban <player|UID> /pull <player|UID>";
    }

    for (std::string_view emote : emotes)
        list += std::format(" /{}", emote);
    send_action(*event.peer, "log", std::format("msg|>> Commands: {} \0", list));
};

std::unordered_map<std::string_view, std::function<void(ENetEvent&, const std::string_view)>> cmd_pool = []
{
    std::unordered_map<std::string_view, std::function<void(ENetEvent&, const std::string_view)>> pool
    {
        {"help", help_return },
        {"?", help_return },
        {"admin", &admin},
        {"maint", &maint},
        {"maintenance", &maint},
        {"resetworld", &resetworld},
        {"resetallworld", &resetallworld},
        {"ready", &ready},
        {"setrole", &setrole},
        {"setlevel", &command_setlevel},
        {"kick", &command_kick},
        {"ban", &command_ban},
        {"unban", &command_unban},
        {"pull", &command_pull},
        {"time", &command::time}, // @note namespace is to prevent mismatching C time
        {"find", &find},
        {"warp", &warp},
        {"punch", &punch},
        {"skin", &skin},
        {"sb", &sb},
        {"who", &who},
        {"me", &me},
        {"news", &news},
        {"weather", &weather},
        {"ghost", &ghost},
        {"online", &stats_command},
        {"on", &stats_command},
        {"startmultiplier", &event_start_command},
        {"stopmultiplier", &event_stop_command},
        {"event", &event_show_command},
        {"reload", &reload},
        {"content", &content_status},
        {"craft", &craft}
    };

    for (std::string_view emote : emotes)
        pool.emplace(emote, &on::Action);

    return pool;
}();
