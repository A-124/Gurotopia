#include "pch.hpp"
#include <algorithm>
#include <limits>
#include <sstream>
#include "onVariant/SetBux.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "goals.hpp"

namespace goals {
namespace {
bool granting = false;

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
bool trigger_from(const std::string &text, event_bus::type &kind)
{
    if (text == "item_changed")         kind = event_bus::type::item_changed;
    else if (text == "block_changed")   kind = event_bus::type::block_changed;
    else if (text == "block_placed")    kind = event_bus::type::block_placed;
    else if (text == "player_entered_world") kind = event_bus::type::player_entered_world;
    else return false;
    return true;
}
}

bool busy() noexcept { return granting; }

reward parse_reward(std::string_view text)
{
    reward r{};
    for (const std::string &entry : split(std::string{text}, ','))
    {
        const std::vector<std::string> p = split(entry, ':');
        int a{}, b{};
        if (p.size() == 2 && p[0] == "gems" && to_int(p[1], a) && a > 0) r.gems += a;
        else if (p.size() == 2 && p[0] == "xp" && to_int(p[1], a) && a > 0) r.xp += a;
        else if (p.size() == 3 && p[0] == "item" && to_int(p[1], a) && to_int(p[2], b) && a > 0 && b > 0 && b <= 200)
            r.items.emplace_back(a, b);
    }
    return r;
}

std::string describe(const reward &r)
{
    std::vector<std::string> parts;
    if (r.gems > 0) parts.push_back(std::format("`2{}`` gems", r.gems));
    if (r.xp > 0) parts.push_back(std::format("`2{}`` XP", r.xp));
    for (const auto &[id, count] : r.items)
        parts.push_back(std::format("`2{}x {}``", count, id_to_item(static_cast<u_short>(id)).raw_name));
    if (parts.empty()) return "nothing";

    std::string text;
    for (std::size_t i = 0; i < parts.size(); ++i) text += (i ? ", " : "") + parts[i];
    return text;
}

bool parse_line(const std::string &line, goal &out)
{
    if (line.empty() || line[0] == '#') return false;
    const std::vector<std::string> f = split(line, '|');
    if (f.size() < 4) return false;

    goal g{};
    if (!to_int(f[0], g.id) || g.id <= 0) return false;
    g.name = f[1];
    if (g.name.empty() || !trigger_from(f[2], g.trigger)) return false;
    if (!to_int(f[3], g.target) || g.target < 0) return false;
    if (f.size() > 4 && !to_int(f[4], g.required)) return false;
    g.required = std::max(1, g.required);
    if (f.size() > 5) g.prize = parse_reward(f[5]);
    if (f.size() > 6) g.description = f[6];

    out = std::move(g);
    return true;
}

std::string grant(ENetPeer *peer, const reward &r)
{
    if (!peer || !peer->data) return "";
    ::peer *pPeer = static_cast<::peer*>(peer->data);

    struct guard { guard() { granting = true; } ~guard() { granting = false; } } busy_guard; // @note see busy()
    ENetEvent fake{};
    fake.peer = peer;

    std::vector<std::string> notes;
    if (r.gems > 0)
    {
        const long long room = static_cast<long long>(std::numeric_limits<signed>::max()) - std::max(pPeer->gems, 0);
        pPeer->gems += static_cast<int>(std::min<long long>(r.gems, std::max(room, 0ll)));
        on::SetBux(fake);
        notes.push_back(std::format("{} gems", r.gems));
    }
    if (r.xp > 0)
    {
        pPeer->add_xp(fake, static_cast<u_short>(std::min(r.xp, 60000)));
        notes.push_back(std::format("{} XP", r.xp));
    }
    for (const auto &[id, count] : r.items)
    {
        if (id <= 0 || id >= static_cast<int>(items.size())) continue;

        const short item_id = static_cast<short>(id);
        const auto held = std::ranges::find(pPeer->slots, item_id, &::slot::id);
        const bool has_room = held != pPeer->slots.end() ? held->count + count <= 200
            : pPeer->slots.size() < static_cast<std::size_t>(std::max(pPeer->slot_size, 0));
        if (!has_room)
        {
            on::ConsoleMessage(peer, std::format("`4Your backpack is full, so you did not get {}x {}.``", count, id_to_item(item_id).raw_name));
            continue;
        }
        modify_item_inventory(fake, ::slot(item_id, static_cast<short>(count)));
        notes.push_back(std::format("{}x {}", count, id_to_item(item_id).raw_name));
    }
    std::string text;
    for (std::size_t i = 0; i < notes.size(); ++i) text += (i ? ", " : "") + notes[i];
    return text;
}

track_result track(const std::unordered_map<int, goal> &defs, std::unordered_map<int, int> &progress,
                   std::unordered_set<int> &done, const event_bus::event &event)
{
    track_result result{};
    for (const auto &[id, g] : defs)
    {
        if (g.trigger != event.kind || done.contains(id)) continue;
        if (g.target != 0 && g.target != event.item_id) continue;

        const int gain = std::max(1, event.amount);
        int &value = progress[id];
        value = static_cast<int>(std::min<long long>(static_cast<long long>(value) + gain, g.required));
        result.changed = true;
        if (value >= g.required)
        {
            done.insert(id);
            result.completed.push_back(&g);
        }
    }
    std::ranges::sort(result.completed, {}, &goal::id);
    return result;
}

void announce(ENetPeer *peer, const goal &g, std::string_view label)
{
    if (!peer || !peer->data) return;
    const ::peer *pPeer = static_cast<::peer*>(peer->data);

    const std::string given = grant(peer, g.prize);
    const std::string text = std::format("`2{} complete: ``w{}``{}", label, g.name,
        given.empty() ? "" : std::format(" `o- reward: `2{}``", given));
    on::ConsoleMessage(peer, text);
    if (pPeer->netid != 0) send_varlist(peer, { "OnTalkBubble", pPeer->netid, std::format("`2{} complete!`` `w{}``", label, g.name), 0u, 1u });
}

std::vector<std::string> report(const std::unordered_map<int, goal> &defs, const std::unordered_map<int, int> &progress,
                                const std::unordered_set<int> &done)
{
    std::vector<const goal*> ordered;
    for (const auto &[id, g] : defs) ordered.push_back(&g);
    std::ranges::sort(ordered, {}, &goal::id);

    std::vector<std::string> lines;
    for (const goal *g : ordered)
    {
        const bool finished = done.contains(g->id);
        const auto it = progress.find(g->id);
        const int value = finished ? g->required : (it == progress.end() ? 0 : it->second);

        std::string line = std::format("{}`w{}`` `o{}``", finished ? "`2[Done]`` " : "", g->name,
            finished ? "" : std::format("({}/{})", value, g->required));
        if (!g->description.empty()) line += std::format(" `o- {}``", g->description);
        if (!finished && !g->prize.empty()) line += std::format(" `o[Reward: {}`o]``", describe(g->prize));
        lines.push_back(std::move(line));
    }
    return lines;
}
}
