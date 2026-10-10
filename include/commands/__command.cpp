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
#include "social.hpp"
#include "help.hpp"
#include "leaderboard.hpp"
#include "title_commands.hpp"
#include "extras.hpp"
#include "gameplay/welcome_system.hpp"
#include "staff_tools.hpp"

/* emote commands all dispatch to on::Action. listed once here so the
 * cmd_pool registration and the /help text stay in sync automatically. */
static constexpr std::string_view emotes[24]{
    "wave", "dance", "love", "sleep", "facepalm", "fp",
    "smh", "yes", "no", "omg", "idk", "shrug",
    "furious", "rolleyes", "foldarms", "fa", "stubborn", "fold",
    "dab", "sassy", "dance2", "march", "grumpy", "shy"
};

/* Commands that require an argument are rejected before dispatch when omitted. */
std::array<std::string_view, 16> cmd_requires_arg{
    "sb", "warp", "punch", "skin", "me", "weather", "setrole", "resetallworld",
    "setlevel", "kick", "ban", "unban", "pull", "mute", "unmute", "pinfo"
};

/* Keep permission checks at dispatch time so hidden commands cannot be invoked directly. */
static auto developer_only(std::function<void(ENetEvent&, const std::string_view)> command)
{
    return [command = std::move(command)](ENetEvent& event, const std::string_view text)
    {
        auto *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
        if (!pPeer || pPeer->role != DEVELOPER)
        {
            if (event.peer)
                send_action(*event.peer, "log", "msg|Only developers can use this command.");
            return;
        }
        command(event, text);
    };
}

static void command_onehit(ENetEvent& event, const std::string_view)
{
    auto *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer || pPeer->role != DEVELOPER)
    {
        if (event.peer)
            send_action(*event.peer, "log", "msg|Only developers can use /1hit.");
        return;
    }

    pPeer->one_hit = !pPeer->one_hit;
    send_action(*event.peer, "log", std::format("msg|One-hit block breaking {}.",
        pPeer->one_hit ? "enabled" : "disabled"));
}

/* /help and /? show a role-aware command guide in clearly separated sections. */
auto help_return = [](ENetEvent& event, const std::string_view)
{
    if (!event.peer) return;
    auto *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    // Avoid embedded newlines: the game's log packet parser truncates at line breaks.
    std::string player_commands = "/help /? /time /sb <message> /find /warp <world> /who "
        "/me <message> /news /event /skin <id> /craft <item_id> [amount] /craftui /features /quests /achievements /daily [claim] /titles /leaderboard /playtime /notebook "
        "/msg <player> <message> /r <message> /mods /rules /worldinfo /roll [sides] /flip /ping /uptime /count /status /welcome";

    for (std::string_view emote : emotes)
        player_commands += std::format(" /{}", emote);

    send_action(*event.peer, "log", "msg|>> Player Cmd:");
    send_action(*event.peer, "log", std::format("msg|{}", player_commands));

    if (pPeer->role >= MODERATOR)
    {
        send_action(*event.peer, "log", "msg|>> `cModerator Cmd:");
        send_action(*event.peer, "log",
            "msg|/kick <player|UID> /ban <player|UID> /unban <player|UID> /pull <player|UID> /mute <player> [minutes] /unmute <player> /pinfo <player> /nick <name> /default");
    }

    if (pPeer->role == DEVELOPER)
    {
        send_action(*event.peer, "log", "msg|>> `bDeveloper Cmd:");
        send_action(*event.peer, "log",
            "msg|/admin /maint [on|off] /maintenance [on|off] /resetworld /resetallworld confirm /ready "
            "/setrole <UID> <role> /setlevel <player|UID> <level> /givetitle <player|me> <id> "
            "/on /online /weather <id> /ghost /punch <id> /content [/validate] /contentui /1hit "
            "/reload <items|content|store|holiday|all> "
            "/startmultiplier <gem> <xp> <seconds> /stopmultiplier");
    }
};

std::unordered_map<std::string_view, std::function<void(ENetEvent&, const std::string_view)>> cmd_pool = []
{
    std::unordered_map<std::string_view, std::function<void(ENetEvent&, const std::string_view)>> pool
    {
        {"help", &command_help },
        {"?", help_return },
        {"admin", &admin},
        {"maint", &maint},
        {"maintenance", &maint},
        {"resetworld", &resetworld},
        {"resetallworld", &resetallworld},
        {"ready", &ready},
        {"setrole", &setrole},
        {"setlevel", developer_only(&command_setlevel)},
        {"kick", &command_kick},
        {"ban", &command_ban},
        {"unban", &command_unban},
        {"pull", &command_pull},
        {"time", &command::time}, // @note namespace is to prevent mismatching C time
        {"find", &find},
        {"warp", &warp},
        {"punch", developer_only(&punch)},
        {"skin", &skin},
        {"sb", &sb},
        {"who", &who},
        {"me", &me},
        {"news", &news},
        {"weather", developer_only(&weather)},
        {"ghost", developer_only(&ghost)},
        {"online", developer_only(&stats_command)},
        {"on", developer_only(&stats_command)},
        {"startmultiplier", &event_start_command},
        {"stopmultiplier", &event_stop_command},
        {"event", &event_show_command},
        {"reload", &reload},
        {"content", developer_only(&content_status)},
        {"features", &features_command},
        {"craftui", &craft_dialog_command},
        {"contentui", developer_only(&content_dialog_command)},
        {"craft", &craft},
        {"quests", &quests_command},
        {"quest", &quests_command},
        {"achievements", &achievements_command},
        {"daily", &daily_command},
        {"titles", &command_titles},
        {"title", &command_titles},
        {"givetitle", developer_only(&command_givetitle)},
        {"leaderboard", &command_leaderboard},
        {"lb", &command_leaderboard},
        {"top", &command_leaderboard},
        {"playtime", &command_playtime},
        {"notebook", &command_notebook},
        {"msg", &command_msg},
        {"w", &command_msg},
        {"r", &command_reply},
        {"mods", &command_mods},
        {"rules", &command_rules},
        {"worldinfo", &command_worldinfo},
        {"roll", &command_roll},
        {"flip", &command_flip},
        {"ping", &command_ping},
        {"uptime", &command_uptime},
        {"count", &command_count},
        {"mute", &command_mute},
        {"unmute", &command_unmute},
        {"pinfo", &command_pinfo},
        {"nick", &command_nick},
        {"default", &command_default},
        {"status", &command_status},
        {"welcome", &welcome_system::command},
        {"1hit", developer_only(&command_onehit)}
    };

    for (std::string_view emote : emotes)
        pool.emplace(emote, &on::Action);

    return pool;
}();
