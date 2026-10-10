#include "pch.hpp"
#include "tools/ui.hpp"
#include "help.hpp"

namespace
{
    struct entry { std::string_view command, text; };

    constexpr entry general[] = {
        { "/help", "Open this guide" }, { "/time", "Show the server time" }, { "/news", "Read the latest news" },
        { "/rules", "Server rules" }, { "/event", "Show the active gem/XP event" }, { "/find", "Search for an item" },
        { "/warp <world>", "Travel to a world" }, { "/worldinfo", "Details about the current world" },
        { "/who", "Players in your world" }, { "/welcome", "Reopen the welcome window" }, { "/ping", "Your latency" }, { "/uptime", "Server uptime" }, { "/skin <id>", "Change your skin colour" },
    };
    constexpr entry social[] = {
        { "/sb <message>", "Broadcast to everyone" }, { "/me <message>", "Emote text over your head" },
        { "/msg <player> <text>", "Private message (also /w)" }, { "/r <text>", "Reply to your last message" },
        { "/mods", "Online staff" }, { "/roll [sides]", "Roll a die" },
        { "/flip", "Flip a coin" }, { "/count", "Players online" }, { "/status", "Your level, gems and play time" },
    };
    constexpr entry progress[] = {
        { "/titles", "Choose, enable or disable your title" }, { "/quests", "Your quests" }, { "/achievements", "Your achievements" },
        { "/daily [claim]", "Daily reward and streak" }, { "/leaderboard", "Top levels and play time" },
        { "/features", "Gurotopia hub" }, { "/craft <id> [n]", "Craft custom items" }, { "/craftui", "Crafting workshop window" },
        { "/notebook", "Open your notebook" }, { "/playtime", "How long you have played" },
    };
    constexpr entry staff[] = {
        { "/kick <player>", "Remove a player from the world" }, { "/ban <player>", "Ban an account" },
        { "/unban <player>", "Lift a ban" }, { "/pull <player>", "Pull a player to you" },
        { "/mute <player> [min]", "Mute a player" }, { "/unmute <player>", "Lift a mute" }, { "/pinfo <player>", "Player details" },
        { "/nick <name>", "Temporary name (/default resets)" },
    };
    constexpr entry developer[] = {
        { "/admin", "Admin panel" }, { "/maint [on|off]", "Maintenance mode" }, { "/setrole <uid> <role>", "Change a role" },
        { "/setlevel <player> <lvl>", "Set a level" }, { "/givetitle <player> <id>", "Give any title" },
        { "/reload <target>", "Reload items, content, store, holiday or all" }, { "/content [validate]", "Content tools" },
        { "/startmultiplier <gem> <xp> <sec>", "Start an event" }, { "/stopmultiplier", "End the event" },
        { "/weather <id>", "Change weather" }, { "/ghost", "Ghost mode" }, { "/punch <id>", "Punch effect" }, { "/1hit", "One-hit break" },
    };
    constexpr std::string_view emotes = "/wave /dance /love /sleep /facepalm /smh /yes /no /omg /idk /shrug /furious /rolleyes /foldarms /stubborn /dab /sassy /dance2 /march /grumpy /shy";

    template<std::size_t N>
    std::string list(const entry (&items)[N])
    {
        std::string d;
        for (const entry &e : items) d += std::format("add_label|small|`w{}``|left|\nadd_smalltext|{}|left|\n", e.command, e.text);
        return d;
    }

    void show(ENetEvent &event, std::string_view page)
    {
        ::peer *p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
        if (!p) return;

        std::string d = ui::header("Command Guide", 32, "Pick a category. Commands are typed in chat.");
        const auto tab = [&](std::string_view id, std::string_view name) { return std::format("add_button|help_{}|{}{}|noflags|0|0|\n", id, page == id ? "`2> " : "`w", name); };
        d += tab("general", "General");
        d += tab("social", "Social");
        d += tab("progress", "Progress & Titles");
        d += tab("emotes", "Emotes");
        if (p->role >= MODERATOR) d += tab("staff", "`cModerator``");
        if (p->role == DEVELOPER) d += tab("developer", "`bDeveloper``");

        d += ui::section(page == "social" ? "Social" : page == "progress" ? "Progress & Titles" : page == "emotes" ? "Emotes" :
                         page == "staff" ? "Moderator" : page == "developer" ? "Developer" : "General");
        if (page == "social") d += list(social);
        else if (page == "progress") d += list(progress);
        else if (page == "emotes") d += std::format("add_smalltext|{}|left|\n", emotes);
        else if (page == "staff" && p->role >= MODERATOR) d += list(staff);
        else if (page == "developer" && p->role == DEVELOPER) d += list(developer);
        else d += list(general);
        d += ui::footer("gurotopia_help", "Close", "");
        send_varlist(event.peer, { "OnDialogRequest", std::move(d) });
    }
}

void command_help(ENetEvent &event, std::string_view) { show(event, "general"); }

void help_dialog_return(ENetEvent &event, const ::hPipe &pipe)
{
    const std::string button = pipe["buttonClicked"];
    if (button.starts_with("help_")) show(event, std::string_view{button}.substr(5));
}
