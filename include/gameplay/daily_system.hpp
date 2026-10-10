#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "gameplay/goals.hpp"

/* @brief daily login reward: configuration (resources/daily.txt), claiming and the /daily window. */
namespace daily_system {

struct day_reward
{
    goals::reward prize{};
    std::string note{};
};

struct config
{
    unsigned cooldown_seconds{ 20u * 3600u };
    unsigned streak_window_seconds{ 48u * 3600u };
    std::vector<day_reward> days{}; // @note one entry per day of the cycle, in order
};

/* @brief (re)load resources/daily.txt. A missing file or a file with no valid days falls back to the built-in defaults.
*  @return false only if the file exists but could not be read. */
bool reload();
const config& current() noexcept;

/* @brief open the reward window for a peer */
void show(ENetEvent &event, std::string notice = {});

/* @brief try to claim today's reward. always tells the player what happened. @return true if a reward was given */
bool claim(ENetEvent &event);

/* @brief /daily  ->  window.   /daily claim  ->  claim immediately */
void command(ENetEvent &event, std::string_view text);

/* @brief buttons of the "gurotopia_daily" dialog */
void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe);

}
