#include "pch.hpp"
#include <map>
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/CountryState.hpp"
#include "database/database.hpp"
#include "database/world.hpp"
#include "flag.hpp"

namespace
{
std::map<int, std::string> flags{}; // @note uid -> chosen flag, "" = default

void ensure_table()
{
    static bool done = false;
    if (done) return;
    done = true;
    mysql_query(db, "CREATE TABLE IF NOT EXISTS flags (uid INT PRIMARY KEY, country VARCHAR(16) NOT NULL)");
}

const std::string &chosen(int uid)
{
    if (auto it = flags.find(uid); it != flags.end()) return it->second;
    ensure_table();
    std::string c{};
    if (mysql_query(db, std::format("SELECT country FROM flags WHERE uid = {}", uid).c_str()) == 0)
        if (MYSQL_RES *res = mysql_store_result(db))
        {
            if (MYSQL_ROW r = mysql_fetch_row(res)) c = r[0] ? r[0] : "";
            mysql_free_result(res);
        }
    return flags[uid] = c;
}

/* badges and title icons: devs only (titles give the icons themselves); the icon sheets would look broken */
bool staff_flag(const std::string &code)
{
    return code == "ccbadge" || code == "ttbadge" || code == "g4g" || code == "master_icon" || code == "game_icons" || code == "faction_icons";
}
}

std::string flag_of(const ::peer &p)
{
    if (p.user_id != 0)
        if (const std::string &c = chosen(static_cast<int>(p.user_id)); !c.empty()) return c;
    if (p.role >= DEVELOPER) return "ccBadge"; // @note devs get the blue checkmark
    return p.country;
}
