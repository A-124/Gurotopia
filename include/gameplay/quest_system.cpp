#include "pch.hpp"
#include <fstream>
#include "gameplay/quest_system.hpp"
#include "gameplay/title_system.hpp"

namespace quest_system {
namespace { std::unordered_map<int, quest> quests; }

bool reload()
{
    std::ifstream file("resources/quests.txt");
    if (!file) return false;

    std::unordered_map<int, quest> next;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        quest parsed;
        if (goals::parse_line(line, parsed)) next[parsed.id] = std::move(parsed);
    }
    quests.swap(next);
    return true;
}

const quest* find(int id) noexcept
{
    const auto it = quests.find(id);
    return it == quests.end() ? nullptr : &it->second;
}

void on_event(const event_bus::event& event)
{
    if (goals::busy() || !event.context || quests.empty()) return;

    ENetPeer *peer = static_cast<ENetPeer*>(event.context);
    ::peer *pPeer = peer->data ? static_cast<::peer*>(peer->data) : nullptr;
    if (!pPeer || pPeer->growid.empty()) return;

    const goals::track_result result = goals::track(quests, pPeer->quest_progress, pPeer->quests_done, event);
    if (!result.changed) return;

    for (const quest *done : result.completed) goals::announce(peer, *done, "Quest");
    if (!result.completed.empty() || ++pPeer->goals_unsaved >= 25) pPeer->save_goals();
    if (!result.completed.empty()) title_system::refresh(peer); // @note quests unlock titles
}

const std::unordered_map<int, quest>& all() noexcept { return quests; }
}
