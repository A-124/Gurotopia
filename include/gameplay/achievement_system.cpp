#include "pch.hpp"
#include <fstream>
#include "gameplay/achievement_system.hpp"
#include "gameplay/title_system.hpp"

namespace achievement_system {
namespace { std::unordered_map<int, achievement> achievements; }

bool reload()
{
    std::ifstream file("resources/achievements.txt");
    if (!file) return false;

    std::unordered_map<int, achievement> next;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        achievement parsed;
        if (goals::parse_line(line, parsed)) next[parsed.id] = std::move(parsed);
    }
    achievements.swap(next);
    return true;
}

const achievement* find(int id) noexcept
{
    const auto it = achievements.find(id);
    return it == achievements.end() ? nullptr : &it->second;
}

void on_event(const event_bus::event& event)
{
    if (goals::busy() || !event.context || achievements.empty()) return;

    ENetPeer *peer = static_cast<ENetPeer*>(event.context);
    ::peer *pPeer = peer->data ? static_cast<::peer*>(peer->data) : nullptr;
    if (!pPeer || pPeer->growid.empty()) return;

    const goals::track_result result = goals::track(achievements, pPeer->achievement_progress, pPeer->achievements_done, event);
    if (!result.changed) return;

    for (const achievement *done : result.completed) goals::announce(peer, *done, "Achievement");
    if (!result.completed.empty() || ++pPeer->goals_unsaved >= 25) pPeer->save_goals();
    if (!result.completed.empty()) title_system::refresh(peer); // @note achievements unlock titles
}

const std::unordered_map<int, achievement>& all() noexcept { return achievements; }
}
