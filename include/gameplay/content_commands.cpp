#include "pch.hpp"
#include <ctime>
#include "content_commands.hpp"
#include "database/custom_content.hpp"
#include "tools/create_dialog.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include "gameplay/quest_system.hpp"
#include "gameplay/achievement_system.hpp"

void content_status(ENetEvent& event,const std::string_view){
 send_varlist(event.peer,{"OnConsoleMessage",std::format("[CONTENT] custom_items={} recipes={} quests={} achievements={}",custom_content::items().size(),custom_content::recipe_count(),quest_system::all().size(),achievement_system::all().size())});
}

namespace
{
    void show_goal_dialog(ENetEvent& event, const char *title, int icon, const std::vector<std::string> &lines, std::size_t done, const char *dialog_name)
    {
        create_dialog dialog = create_dialog()
            .set_default_color("`o")
            .add_label_with_icon("big", std::format("`w{}``", title), icon)
            .add_smalltext(std::format("`2{}`` of `w{}`` completed", done, lines.size()))
            .add_spacer("small");

        if (lines.empty()) dialog.add_textbox("Nothing here yet.");
        for (const std::string &line : lines) dialog.add_textbox(line);

        send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog(dialog_name, "", "OK") });
    }
}

void quests_command(ENetEvent& event, const std::string_view)
{
    ::peer *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer) return;

    show_goal_dialog(event, "Quests", 1436,
        goals::report(quest_system::all(), pPeer->quest_progress, pPeer->quests_done),
        pPeer->quests_done.size(), "quests");
}

void achievements_command(ENetEvent& event, const std::string_view)
{
    ::peer *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer) return;

    show_goal_dialog(event, "Achievements", 1436,
        goals::report(achievement_system::all(), pPeer->achievement_progress, pPeer->achievements_done),
        pPeer->achievements_done.size(), "achievements");
}

void daily_command(ENetEvent& event, const std::string_view)
{
    ::peer *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer) return;

    constexpr u_int cooldown = 20u * 3600u;   // @note claimable roughly once a day
    constexpr u_int streak_window = 48u * 3600u; // @note miss two days and the streak restarts
    const u_int now = static_cast<u_int>(std::time(nullptr));

    if (pPeer->last_daily != 0 && now >= pPeer->last_daily && now - pPeer->last_daily < cooldown)
    {
        const u_int left = cooldown - (now - pPeer->last_daily);
        on::ConsoleMessage(event.peer, std::format("`4Daily reward already claimed.`` Come back in `w{}h {}m``. Current streak: `w{}`` day(s).",
            left / 3600u, (left % 3600u) / 60u, pPeer->daily_streak));
        return;
    }

    if (pPeer->last_daily == 0 || now < pPeer->last_daily || now - pPeer->last_daily > streak_window) pPeer->daily_streak = 1;
    else ++pPeer->daily_streak;
    pPeer->last_daily = now;

    const int day = (pPeer->daily_streak - 1) % 7 + 1; // @note 1..7, then the cycle starts again
    goals::reward prize{};
    prize.gems = 100 * day;
    prize.xp = 25 * day;
    if (day == 7) prize.items.emplace_back(3402/*Golden Booty Chest*/, 1);

    const std::string given = goals::grant(event.peer, prize);
    pPeer->save_goals();

    on::ConsoleMessage(event.peer, std::format("`2Daily reward claimed!`` Day `w{}`` of your streak ({}/7 this week): `2{}``",
        pPeer->daily_streak, day, given));
    if (pPeer->netid != 0)
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, std::format("`2Daily reward! Streak: {}``", pPeer->daily_streak), 0u, 1u });
}
