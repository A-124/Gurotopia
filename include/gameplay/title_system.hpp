#pragma once
#include <string>
#include <string_view>
#include <vector>

/*
* @brief player titles: a catalog (resources/titles.txt), unlock rules, the wrench "Title" dialog
*        and the name tag decoration. Unlocked titles are saved per player, the equipped title and
*        the on/off switch too.
*/
namespace title_system
{
    enum class kind : unsigned char
    {
        none, level, achievement, achievements, quest, quests, playtime, age, streak, fires, role, granted
    };

    struct title
    {
        int id{};
        std::string name{};
        std::string color{ "2" }; // @note colour code letter, used as "`" + color
        kind need{ kind::none };
        int amount{};
        std::string description{};
    };

    /* @brief (re)load resources/titles.txt. A missing file falls back to a single "Newbie" title. */
    bool reload();
    const std::vector<title>& all() noexcept;
    const title* find(int id) noexcept;

    /* @return true if the peer currently satisfies the title's requirement */
    bool meets(const ::peer &p, const title &t);
    /* @return "37/50 levels" style progress, or a short sentence for yes/no requirements */
    std::string requirement_text(const ::peer &p, const title &t);
    /* @return progress current / total as a pair, total 0 for yes/no requirements */
    std::pair<long long, long long> progress(const ::peer &p, const title &t);

    /* @return "`2[Builder] " for the equipped title, empty if none. Does not look at the on/off switch.
    *  @note no "``" reset after the title on purpose: a reset makes the client fall back to its own base name
    *        colour (e.g. the level 125 colour). The caller must continue with an explicit colour code. */
    std::string tag(const ::peer &p);

    /* @return how many unlocked titles still exist in the catalog (removed titles are ignored) */
    std::size_t owned_count(const ::peer &p);

    /* @brief unlock every title the peer qualifies for. Tells the peer about new ones.
    *  @return how many titles were newly unlocked */
    int refresh(ENetPeer *peer, bool announce = true);

    /* @brief resend the name tag so the title shows up above the head, for the owner and everyone in the world */
    void broadcast_name(ENetPeer *peer);

    /* @brief the wrench > Title window. page 0 = unlocked, 1 = locked */
    void show(ENetEvent &event, int page = 0, std::string notice = {});
    void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe);

    /* @brief give a title regardless of requirements (developer command). @return false if unknown title */
    bool grant(ENetPeer *peer, int id);

    /* @brief once a minute: bank play time and unlock time based titles. subscribe to event_bus::type::tick */
    void on_tick();
}
