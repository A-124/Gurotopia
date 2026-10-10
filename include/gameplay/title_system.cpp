#include "pch.hpp"
#include <fstream>
#include <ctime>
#include <algorithm>
#include "gameplay/title_system.hpp"
#include "gameplay/achievement_system.hpp"
#include "gameplay/quest_system.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/AddNotification.hpp"
#include "tools/ui.hpp"

namespace title_system
{
namespace
{
    std::vector<title> catalog;

    kind parse_kind(std::string_view name)
    {
        if (name == "none")         return kind::none;
        if (name == "level")        return kind::level;
        if (name == "achievement")  return kind::achievement;
        if (name == "achievements") return kind::achievements;
        if (name == "quest")        return kind::quest;
        if (name == "quests")       return kind::quests;
        if (name == "playtime")     return kind::playtime;
        if (name == "age")          return kind::age;
        if (name == "streak")       return kind::streak;
        if (name == "fires")        return kind::fires;
        if (name == "role")         return kind::role;
        if (name == "granted")      return kind::granted;
        return kind::granted; // @note unknown requirement: never unlock by accident
    }

    long long account_days(const ::peer &p)
    {
        return p.created_at > 0 ? std::max<long long>(0, (std::time(nullptr) - p.created_at) / 86400) : 0;
    }

    ::peer *peer_of(ENetPeer *peer) { return (peer && peer->data) ? static_cast<::peer*>(peer->data) : nullptr; }

    std::string colored_name(const title &t) { return std::format("`{}{}``", t.color, t.name); }
}

bool reload()
{
    std::ifstream file("resources/titles.txt");
    std::vector<title> next;
    if (file)
    {
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;

            const std::vector<std::string> f = readch(line, '|');
            if (f.size() < 4) continue;

            title t;
            t.id = std::atoi(f[0].c_str());
            if (t.id <= 0 || std::ranges::any_of(next, [&](const title &o) { return o.id == t.id; })) continue;
            t.name = ui::sanitize(f[1], 24);
            t.color = f[2].empty() ? "2" : f[2].substr(0, 1);
            if (t.name.empty()) continue;

            const std::size_t colon = f[3].find(':');
            t.need = parse_kind(f[3].substr(0, colon));
            if (colon != std::string::npos) t.amount = std::atoi(f[3].c_str() + colon + 1);
            if (f.size() > 4) t.description = ui::sanitize(f[4], 120);
            next.push_back(std::move(t));
        }
    }
    if (next.empty()) next.push_back(title{ 1, "Newbie", "2", kind::none, 0, "Everyone starts somewhere. Welcome!" });

    std::ranges::sort(next, {}, &title::id);
    catalog.swap(next);
    return static_cast<bool>(file);
}

const std::vector<title>& all() noexcept { return catalog; }

const title* find(int id) noexcept
{
    const auto it = std::ranges::find(catalog, id, &title::id);
    return it == catalog.end() ? nullptr : &*it;
}

std::pair<long long, long long> progress(const ::peer &p, const title &t)
{
    switch (t.need)
    {
        case kind::level:        return { p.level.front(), t.amount };
        case kind::achievements: return { static_cast<long long>(p.achievements_done.size()), t.amount };
        case kind::quests:       return { static_cast<long long>(p.quests_done.size()), t.amount };
        case kind::playtime:     return { p.total_playtime() / 3600, t.amount };
        case kind::age:          return { account_days(p), t.amount };
        case kind::streak:       return { p.daily_streak, t.amount };
        case kind::fires:        return { p.fires_removed, t.amount };
        default:                 return { 0, 0 };
    }
}

bool meets(const ::peer &p, const title &t)
{
    switch (t.need)
    {
        case kind::none:        return true;
        case kind::achievement: return p.achievements_done.contains(t.amount);
        case kind::quest:       return p.quests_done.contains(t.amount);
        case kind::role:        return p.role >= t.amount;
        case kind::granted:     return false;
        default:
        {
            const auto [current, total] = progress(p, t);
            return current >= total;
        }
    }
}

std::string requirement_text(const ::peer &p, const title &t)
{
    switch (t.need)
    {
        case kind::none:        return "Available to everyone";
        case kind::achievement:
        {
            const auto *a = achievement_system::find(t.amount);
            return std::format("Achievement: {}", a ? a->name : std::format("#{}", t.amount));
        }
        case kind::quest:
        {
            const auto *q = quest_system::find(t.amount);
            return std::format("Quest: {}", q ? q->name : std::format("#{}", t.amount));
        }
        case kind::role:        return t.amount >= DEVELOPER ? "Server developers only" : "Server staff only";
        case kind::granted:     return "Awarded by the staff";
        default: break;
    }
    const auto [current, total] = progress(p, t);
    static constexpr std::string_view unit[] = { "", "level", "", "achievements", "", "quests", "hours played", "days old", "day streak", "fires put out" };
    const auto idx = static_cast<std::size_t>(t.need);
    return std::format("{}/{} {}", std::min(current, total), total, idx < std::size(unit) ? unit[idx] : "");
}

std::string tag(const ::peer &p)
{
    if (p.title_active <= 0 || !p.titles_unlocked.contains(p.title_active)) return {};
    const title *t = find(p.title_active);
    return t ? std::format("`{}[{}] ", t->color, t->name) : std::string{};
}

std::size_t owned_count(const ::peer &p)
{
    return static_cast<std::size_t>(std::ranges::count_if(catalog, [&](const title &t) { return p.titles_unlocked.contains(t.id); }));
}

void broadcast_name(ENetPeer *peer)
{
    ::peer *self = peer_of(peer);
    if (!self || self->netid == 0 || self->recent_worlds.back().empty()) return;

    const std::string name = self->nametag();
    const int netid = self->netid;
    peers(self->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &other)
    {
        send_varlist(&other, { "OnNameChanged", name }, netid);
    });
}

bool grant(ENetPeer *peer, int id)
{
    ::peer *p = peer_of(peer);
    if (!p || !find(id)) return false;
    if (p->titles_unlocked.insert(id).second)
    {
        p->save_titles();
        on::ConsoleMessage(peer, std::format("`2New title unlocked: ``{}``! Open your wrench menu and tap `wTitle`` to wear it.``", colored_name(*find(id))));
    }
    return true;
}

int refresh(ENetPeer *peer, bool announce)
{
    ::peer *p = peer_of(peer);
    if (!p || p->growid.empty()) return 0;

    // @note the worn title was removed from titles.txt: take it off instead of leaving a dead id behind
    if (p->title_active > 0 && !find(p->title_active))
    {
        p->title_active = 0;
        p->save_titles();
        if (p->netid != 0) broadcast_name(peer);
    }

    std::vector<const title*> fresh;
    for (const title &t : catalog)
        if (!p->titles_unlocked.contains(t.id) && meets(*p, t))
        {
            p->titles_unlocked.insert(t.id);
            fresh.push_back(&t);
        }
    if (fresh.empty()) return 0;

    // @note the first ever title is worn automatically so new players see the feature working
    if (p->title_active == 0 && p->titles_unlocked.size() == fresh.size())
    {
        p->title_active = fresh.front()->id;
        if (p->netid != 0) broadcast_name(peer);
    }
    p->save_titles();

    if (announce)
    {
        for (const title *t : fresh)
            on::ConsoleMessage(peer, std::format("`2New title unlocked:`` {} `o- {}``", colored_name(*t), t->description));
        on::AddNotification(peer, std::format("`2New title unlocked: ``{}", colored_name(*fresh.back())), "audio/piano_nice.wav");
        if (p->netid != 0)
            send_varlist(peer, { "OnTalkBubble", p->netid, std::format("`2New title: ``{}", colored_name(*fresh.back())), 0u, 1u });
        on::ConsoleMessage(peer, "`oOpen your wrench menu and tap `wTitle`` to wear it.``");
    }
    return static_cast<int>(fresh.size());
}

namespace
{
    /* @brief one catalog entry. Owned titles are a button (tap = wear), locked ones show how close you are. */
    std::string title_row(const ::peer &p, const title &t, bool unlocked)
    {
        std::string row;
        if (unlocked)
        {
            const bool worn = p.title_active == t.id;
            row += std::format("add_button|title_pick_{}|{}`{}[{}]``|noflags|0|0|\n", t.id, worn ? "`2WORN  `` " : "", t.color, t.name);
            row += std::format("add_smalltext|{}|left|\n", t.description);
        }
        else
        {
            const auto [current, total] = progress(p, t);
            row += std::format("add_label|small|`{}[{}]`` `4locked``|left|\n", t.color, t.name);
            row += std::format("add_smalltext|{}  `9{}``|left|\n", t.description, requirement_text(p, t));
            if (total > 0) row += std::format("add_smalltext|{}|left|\n", ui::bar(current, total, 12));
        }
        return row;
    }

    /* @return 0..1000 how close a locked title is, used to list the nearest goals first */
    long long closeness(const ::peer &p, const title &t)
    {
        const auto [current, total] = progress(p, t);
        return total > 0 ? std::min<long long>(current, total) * 1000 / total : 0;
    }
}

void show(ENetEvent &event, int page, std::string notice)
{
    ::peer *p = peer_of(event.peer);
    if (!p) return;

    refresh(event.peer); // @note make sure the list is up to date before showing it

    std::vector<const title*> owned, locked;
    for (const title &t : catalog) (p->titles_unlocked.contains(t.id) ? owned : locked).push_back(&t);
    // @note nearest goal first so the next unlock is always at the top
    std::ranges::stable_sort(locked, [&](const title *l, const title *r) { return closeness(*p, *l) > closeness(*p, *r); });

    const title *active = p->title_active > 0 ? find(p->title_active) : nullptr;
    if (active && !p->titles_unlocked.contains(active->id)) active = nullptr;

    std::string d = ui::header("Titles", 1436, std::format("`w{}`` of `w{}`` unlocked", owned.size(), catalog.size()));
    if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);

    // @note state card: what you wear, what everyone sees
    d += "add_label|small|`9~ Your name tag ~``|left|\n";
    d += std::format("add_textbox|{}|left|\n", p->nametag());
    d += std::format("add_smalltext|Wearing: {}   Display: {}|left|\n",
        active ? std::format("`{}[{}]``", active->color, active->name) : "`onothing``",
        p->title_enabled ? "`2shown``" : "`4hidden``");
    d += std::format("add_button|title_toggle|{}|noflags|0|0|\n", p->title_enabled ? "`4Hide my title``" : "`2Show my title``");
    if (active) d += "add_button|title_remove|`wTake title off``|noflags|0|0|\n";

    d += ui::section(page == 0 ? "Your titles - tap one to wear it" : "Locked - closest goals first");
    d += std::format("add_button|title_page_0|{}Unlocked ({})|noflags|0|0|\n", page == 0 ? "`2> " : "`w", owned.size());
    d += std::format("add_button|title_page_1|{}Locked ({})|noflags|0|0|\n", page == 1 ? "`2> " : "`w", locked.size());
    d += "add_spacer|small|\n";

    const auto &list = page == 0 ? owned : locked;
    if (list.empty())
        d += page == 0 ? "add_textbox|You have not unlocked any titles yet.|left|\n"
                       : "add_textbox|`2Amazing! You unlocked every title.``|left|\n";
    for (const title *t : list)
    {
        d += title_row(*p, *t, page == 0);
        d += "add_spacer|small|\n";
    }
    d += std::format("embed_data|title_page|{}\n", page);
    d += ui::footer("title_menu", "Close", "");
    send_varlist(event.peer, { "OnDialogRequest", std::move(d) });
}

void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe)
{
    ::peer *p = peer_of(event.peer);
    if (!p) return;

    const std::string button = pipe["buttonClicked"];
    const int page = std::atoi(pipe["title_page"].c_str()) == 1 ? 1 : 0;

    if (button.empty()) return; // @note Close
    if (button == "title_page_0") { show(event, 0); return; }
    if (button == "title_page_1") { show(event, 1); return; }

    if (button == "title_toggle")
    {
        p->title_enabled = !p->title_enabled;
        p->save_titles();
        broadcast_name(event.peer);
        show(event, page, p->title_enabled ? "`2Title display enabled.``" : "`4Title display disabled.`` Your choice is remembered.");
        return;
    }
    if (button == "title_remove")
    {
        p->title_active = 0;
        p->save_titles();
        broadcast_name(event.peer);
        show(event, page, "`oYou took your title off.``");
        return;
    }
    if (button.starts_with("title_pick_"))
    {
        const int id = std::atoi(button.c_str() + std::string_view("title_pick_").size());
        const title *t = find(id);
        if (!t || !p->titles_unlocked.contains(id))
        {
            show(event, page, "`4You have not unlocked that title yet.``");
            return;
        }
        p->title_active = id;
        p->title_enabled = true; // @note picking a title obviously means you want to see it
        p->save_titles();
        broadcast_name(event.peer);
        show(event, page, std::format("`2Now wearing`` {}`2.``", colored_name(*t)));
    }
}

void on_tick()
{
    static std::time_t next_run = 0;
    const std::time_t now = std::time(nullptr);
    if (now < next_run) return;
    next_run = now + 60;

    peers("", PEER_ALL, [](ENetPeer &peer)
    {
        ::peer *p = peer_of(&peer);
        if (!p || p->growid.empty()) return;
        p->save_profile(); // @note keep play time safe even if the server crashes
        refresh(&peer);    // @note play time / account age titles
    });
}
}
