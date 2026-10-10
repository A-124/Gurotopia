#include "pch.hpp"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/AddNotification.hpp"
#include "daily_system.hpp"
#include "gameplay/title_system.hpp"

namespace daily_system {
namespace {

config active = []
{
    // @note built-in defaults, identical to the original hard-coded /daily
    config c{};
    for (int day = 1; day <= 7; ++day)
    {
        day_reward d{};
        d.prize.gems = 1000 * day;
        d.prize.xp = 250 * day;
        if (day == 7)
        {
            d.prize.items.emplace_back(3402/*Golden Booty Chest*/, 1);
            d.note = "Weekly bonus!";
        }
        c.days.push_back(std::move(d));
    }
    return c;
}();

constexpr std::size_t max_days = 31;

std::vector<std::string> split(const std::string &text, char delimiter)
{
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string part;
    while (std::getline(ss, part, delimiter)) out.push_back(part);
    return out;
}

bool to_int(const std::string &text, int &value)
{
    try { std::size_t used{}; value = std::stoi(text, &used); return used == text.size(); }
    catch (...) { return false; }
}

std::string duration(unsigned seconds)
{
    const unsigned h = seconds / 3600u, m = (seconds % 3600u) / 60u;
    if (h > 0) return std::format("{}h {}m", h, m);
    if (m > 0) return std::format("{}m {}s", m, seconds % 60u);
    return std::format("{}s", seconds);
}

/* what the player's streak looks like right now */
struct state
{
    bool claimed_today{};   // @note still on cooldown
    unsigned seconds_left{}; // @note until the next claim (when claimed_today)
    bool streak_alive{};    // @note the streak would continue if claimed now / was continued today
    int streak{};           // @note current streak length (0 if it was lost or never started)
    int next_day{ 1 };      // @note 1-based day of the cycle that the next claim gives
};

state evaluate(const ::peer &p, unsigned now)
{
    const config &c = active;
    const int cycle = static_cast<int>(c.days.size());
    state s{};

    const bool never = p.last_daily == 0;
    const bool clock_ok = !never && now >= p.last_daily;
    const unsigned since = clock_ok ? now - p.last_daily : 0u;

    s.claimed_today = clock_ok && since < c.cooldown_seconds;
    if (s.claimed_today) s.seconds_left = c.cooldown_seconds - since;
    s.streak_alive = clock_ok && since <= c.streak_window_seconds;
    s.streak = s.streak_alive ? std::max(p.daily_streak, 0) : 0;
    s.next_day = (s.streak % cycle) + 1; // @note streak 0 -> day 1; after the last day the cycle starts over
    return s;
}

std::string day_line(int number, const day_reward &d, std::string_view status)
{
    std::string text = std::format("`wDay {}`` {}  {}", number, status, goals::describe(d.prize));
    if (!d.note.empty()) text += std::format("  `o({})``", d.note);
    return text;
}

int icon_for(const goals::reward &r)
{
    if (!r.items.empty()) return r.items.front().first;
    return 112; // @note gems
}
}

bool reload()
{
    std::ifstream file("resources/daily.txt");
    if (!file) return true; // @note keep whatever is active (defaults on first run)

    config next{};
    next.cooldown_seconds = active.cooldown_seconds;
    next.streak_window_seconds = active.streak_window_seconds;
    bool cooldown_set = false, window_set = false;
    std::map<int, day_reward> by_day;

    std::string line;
    while (std::getline(file, line))
    {
        while (!line.empty() && (line.back() == '\r' || std::isspace(static_cast<unsigned char>(line.back())))) line.pop_back();
        if (line.empty() || line.front() == '#') continue;

        const std::vector<std::string> f = split(line, '|');
        int n{};
        if (f.size() == 3 && f[0] == "setting" && to_int(f[2], n))
        {
            if (f[1] == "cooldown_hours" && n >= 1 && n <= 720) { next.cooldown_seconds = static_cast<unsigned>(n) * 3600u; cooldown_set = true; }
            else if (f[1] == "streak_window_hours" && n >= 1 && n <= 8760) { next.streak_window_seconds = static_cast<unsigned>(n) * 3600u; window_set = true; }
            else std::fprintf(stderr, "[daily] ignored setting: %s\n", line.c_str());
        }
        else if (f.size() >= 3 && f[0] == "day" && to_int(f[1], n) && n > 0)
        {
            day_reward d{};
            d.prize = goals::parse_reward(f[2]);
            if (f.size() > 3) d.note = f[3];
            if (d.prize.empty()) { std::fprintf(stderr, "[daily] day %d has no valid reward, skipped\n", n); continue; }
            if (by_day.size() >= max_days) { std::fprintf(stderr, "[daily] more than %zu days, extra ignored\n", max_days); continue; }
            by_day[n] = std::move(d); // @note a repeated day number replaces the earlier one
        }
        else std::fprintf(stderr, "[daily] ignored line: %s\n", line.c_str());
    }
    (void)cooldown_set; (void)window_set;

    if (next.streak_window_seconds < next.cooldown_seconds) next.streak_window_seconds = next.cooldown_seconds;
    for (auto &[number, d] : by_day) next.days.push_back(std::move(d));
    if (next.days.empty())
    {
        std::fprintf(stderr, "[daily] resources/daily.txt has no valid 'day' lines, using the built-in rewards\n");
        next.days = active.days;
    }
    active = std::move(next);
    return true;
}

const config& current() noexcept { return active; }

bool claim(ENetEvent &event)
{
    ::peer *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer || pPeer->growid.empty()) return false;

    const unsigned now = static_cast<unsigned>(std::time(nullptr));
    const state s = evaluate(*pPeer, now);
    if (s.claimed_today)
    {
        on::ConsoleMessage(event.peer, std::format("`4Daily reward already claimed.`` Come back in `w{}``. Current streak: `w{}`` day(s).",
            duration(s.seconds_left), pPeer->daily_streak));
        return false;
    }

    const int cycle = static_cast<int>(active.days.size());
    pPeer->daily_streak = s.streak + 1;
    pPeer->last_daily = now;

    const day_reward &today = active.days[static_cast<std::size_t>(s.next_day - 1)];
    const std::string given = goals::grant(event.peer, today.prize);
    pPeer->save_goals();
    title_system::refresh(event.peer); // @note streak titles

    on::ConsoleMessage(event.peer, std::format("`2Daily reward claimed!`` Day `w{}`` of your streak ({}/{} this cycle): `2{}``",
        pPeer->daily_streak, s.next_day, cycle, given));
    on::AddNotification(event.peer, std::format("`2Daily reward claimed!`` Streak: `w{}``", pPeer->daily_streak), "audio/cash_register.wav");
    if (pPeer->netid != 0)
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, std::format("`2Daily reward! Streak: {}``", pPeer->daily_streak), 0u, 1u });
    return true;
}

void show(ENetEvent &event, std::string notice)
{
    ::peer *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer) return;

    const unsigned now = static_cast<unsigned>(std::time(nullptr));
    const state s = evaluate(*pPeer, now);
    const int cycle = static_cast<int>(active.days.size());

    // @note which days of the cycle are already collected in the current streak
    const int collected = s.claimed_today ? (std::max(pPeer->daily_streak, 1) - 1) % cycle + 1 : (s.streak % cycle);
    const int display_streak = s.streak_alive ? pPeer->daily_streak : 0;

    std::string d =
        "set_bg_color|15,50,75,235|\n"
        "set_border_color|75,205,230,255|\n"
        "add_label_with_icon|big|`wDaily Rewards``|left|112|\n";
    d += std::format("add_smalltext|Log in every day to keep your streak going. Reward cycle: {} days.|left|\n", cycle);
    d += "add_spacer|small|\n";
    if (!notice.empty()) d += std::format("add_textbox|{}|left|\nadd_spacer|small|\n", notice);

    d += std::format("add_textbox|Current streak: `2{}`` day(s)|left|\n", display_streak);
    if (!s.streak_alive && pPeer->last_daily != 0)
        d += "add_smalltext|`4Your streak was lost.`` Claim now to start again from Day 1.|left|\n";
    if (s.claimed_today)
        d += std::format("add_textbox|`oNext reward available in `w{}```o.``|left|\n", duration(s.seconds_left));
    else
        d += std::format("add_textbox|`2Today's reward (Day {}) is ready to claim!``|left|\n", s.next_day);
    d += "add_spacer|small|\n";

    for (int i = 1; i <= cycle; ++i)
    {
        const day_reward &day = active.days[static_cast<std::size_t>(i - 1)];
        std::string status;
        if (i <= collected && (s.claimed_today || s.streak_alive)) status = "`2[CLAIMED]``";
        else if (i == s.next_day) status = s.claimed_today ? "`e[NEXT]``" : "`2[READY]``";
        else status = "`o[LOCKED]``";
        d += std::format("add_label_with_icon|small|{}|left|{}|\n", day_line(i, day, status), icon_for(day.prize));
    }
    d += "add_spacer|small|\n";
    if (!s.claimed_today) d += "add_button|daily_claim|Claim Today's Reward|noflags|0|0|\n";
    d += "add_button|daily_refresh|Refresh|noflags|0|0|\n";
    d += "end_dialog|gurotopia_daily||Close|\nadd_quick_exit|\n";

    send_varlist(event.peer, { "OnDialogRequest", std::move(d) });
}

void command(ENetEvent &event, std::string_view text)
{
    if (text.find("claim") != std::string_view::npos) { claim(event); return; }
    show(event);
}

void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe)
{
    const std::string button = pipe["buttonClicked"];
    if (button == "daily_claim")
    {
        const bool ok = claim(event);
        show(event, ok ? "`2Reward claimed!``" : "`4You can't claim yet.``");
    }
    else if (button == "daily_refresh") show(event);
}

}
