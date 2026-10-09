#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "core/event_bus.hpp"

/* @brief shared engine behind quests and achievements: parsing, progress tracking and rewards. */
namespace goals {

struct reward
{
    int gems{};
    int xp{};
    std::vector<std::pair<int, int>> items{}; // @note {item id, amount}
    bool empty() const noexcept { return gems <= 0 && xp <= 0 && items.empty(); }
};

/*
* @note one line of resources/quests.txt or resources/achievements.txt
*   id|name|trigger|target|required|reward|description
*   trigger : item_changed | block_changed | block_placed | player_entered_world
*   target  : item id the event must involve, 0 = anything
*   required: how many times (or how much, for item_changed) to reach the goal. default 1
*   reward  : comma list of gems:N, xp:N, item:ID:COUNT. e.g. gems:100,xp:50,item:3402:1
*/
struct goal
{
    int id{};
    std::string name{};
    std::string description{};
    event_bus::type trigger{};
    int target{};
    int required{1};
    reward prize{};
};

bool parse_line(const std::string &line, goal &out);
reward parse_reward(std::string_view text);
std::string describe(const reward &r); // @note e.g. "100 gems, 50 XP, 1x item"

/* @return true while a reward is being handed out, events raised by the reward itself must not count as progress */
bool busy() noexcept;

/* @brief give a reward to a peer. never throws. @return what was actually given, for display. */
std::string grant(ENetPeer *peer, const reward &r);

struct track_result
{
    bool changed{};
    std::vector<const goal*> completed{};
};
/* @brief apply one event to a player's progress. */
track_result track(const std::unordered_map<int, goal> &defs, std::unordered_map<int, int> &progress,
                   std::unordered_set<int> &done, const event_bus::event &event);

/* @brief tell the player a goal was completed and give the reward. */
void announce(ENetPeer *peer, const goal &g, std::string_view label);

/* @return one display line per goal, ordered by id. */
std::vector<std::string> report(const std::unordered_map<int, goal> &defs, const std::unordered_map<int, int> &progress,
                                const std::unordered_set<int> &done);
}
