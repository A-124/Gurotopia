#include "pch.hpp"
#include <cstdlib>
#include "tools/ui.hpp"
#include "leaderboard.hpp"

namespace
{
    constexpr std::string_view medal[] = { "`61st``", "`72nd``", "`83rd``" };

    /* @brief run a read-only query made only of server-built text/integers. Rows are copied out as strings. */
    std::vector<std::vector<std::string>> rows(const std::string &query, int columns)
    {
        std::vector<std::vector<std::string>> out;
        if (!db || mysql_query(db, query.c_str())) return out;
        MYSQL_RES *result = mysql_store_result(db);
        if (!result) return out;
        while (MYSQL_ROW row = mysql_fetch_row(result))
        {
            std::vector<std::string> line;
            for (int i = 0; i < columns; ++i) line.emplace_back(row[i] ? row[i] : "");
            out.push_back(std::move(line));
        }
        mysql_free_result(result);
        return out;
    }

    long long scalar(const std::string &query)
    {
        const auto r = rows(query, 1);
        return r.empty() ? 0 : std::atoll(r[0][0].c_str());
    }

    void show(ENetEvent &event, bool by_playtime)
    {
        ::peer *p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
        if (!p) return;

        std::string d = ui::header("Leaderboard", 1796, by_playtime ? "Most dedicated players (play time)" : "Highest level players");
        d += std::format("add_button|lb_level|{}Top Levels|noflags|0|0|\n", by_playtime ? "`w" : "`2> ");
        d += std::format("add_button|lb_playtime|{}Top Play Time|noflags|0|0|\n", by_playtime ? "`2> " : "`w");
        d += ui::section("Top 10");

        const auto top = by_playtime
            ? rows("SELECT growid, playtime, role FROM peer WHERE growid IS NOT NULL ORDER BY playtime DESC, uid ASC LIMIT 10", 3)
            : rows("SELECT growid, level, xp FROM peer WHERE growid IS NOT NULL ORDER BY level DESC, xp DESC, uid ASC LIMIT 10", 3);
        if (top.empty()) d += "add_textbox|`oNo players yet.``|left|\n";
        for (std::size_t i = 0; i < top.size(); ++i)
        {
            const std::string name = ui::sanitize(top[i][0], 18);
            const std::string rank = i < 3 ? std::string{medal[i]} : std::format("`o{}.``", i + 1);
            const bool me = name == p->growid;
            const std::string value = by_playtime
                ? ui::duration(static_cast<unsigned long long>(std::atoll(top[i][1].c_str())))
                : std::format("Level {}", top[i][1]);
            d += std::format("add_textbox|{} {}{}`` - `2{}``|left|\n", rank, me ? "`2" : "`w", name, value);
        }

        // @note your own rank
        long long rank = 0;
        if (by_playtime) rank = scalar(std::format("SELECT COUNT(*) + 1 FROM peer WHERE playtime > {}", p->total_playtime()));
        else rank = scalar(std::format("SELECT COUNT(*) + 1 FROM peer WHERE level > {0} OR (level = {0} AND xp > {1})", p->level.front(), p->level.back()));
        d += ui::section("You");
        d += std::format("add_textbox|`wRank #{}`` - {}|left|\n", rank,
            by_playtime ? ui::duration(p->total_playtime()) : std::format("Level {}", p->level.front()));
        d += ui::footer("gurotopia_leaderboard", "Close", "");
        send_varlist(event.peer, { "OnDialogRequest", std::move(d) });
    }
}

void command_leaderboard(ENetEvent &event, std::string_view text)
{
    show(event, text.find("time") != std::string_view::npos || text.find("play") != std::string_view::npos);
}

void leaderboard_dialog_return(ENetEvent &event, const ::hPipe &pipe)
{
    const std::string button = pipe["buttonClicked"];
    if (button == "lb_level") show(event, false);
    else if (button == "lb_playtime") show(event, true);
}
