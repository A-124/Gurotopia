#pragma once

#include <algorithm>
#include <format>
#include <string>
#include <string_view>

/*
* @brief shared look & feel for every Gurotopia dialog.
*   One palette + a few helpers so all panels (titles, hub, quests, profile, help...) look the same
*   instead of each file hand-writing its own colours.
*/
namespace ui
{
    /* palette: deep ocean background with a cyan border (same family as the Growtopia wrench menu) */
    inline constexpr std::string_view BG     = "set_bg_color|15,50,75,235|\n";
    inline constexpr std::string_view BORDER = "set_border_color|75,205,230,255|\n";

    /* @brief make user text safe to place inside a dialog field: no field separators, no line breaks. */
    inline std::string sanitize(std::string_view text, std::size_t max_len = 120)
    {
        std::string out;
        out.reserve(std::min(text.size(), max_len));
        for (char c : text)
        {
            if (out.size() >= max_len) break;
            if (c == '|' || c == '\n' || c == '\r' || c == '\t') continue;
            if (static_cast<unsigned char>(c) < 0x20) continue;
            out += c;
        }
        // @note trim
        while (!out.empty() && out.back() == ' ') out.pop_back();
        const std::size_t first = out.find_first_not_of(' ');
        return first == std::string::npos ? std::string{} : out.substr(first);
    }

    /* @brief themed dialog start: colours + big icon title + small grey subtitle. */
    inline std::string header(std::string_view title, int icon, std::string_view subtitle = {})
    {
        std::string d{ BG };
        d += BORDER;
        d += "set_default_color|`o\n";
        d += std::format("add_label_with_icon|big|`w{}``|left|{}|\n", title, icon);
        if (!subtitle.empty()) d += std::format("add_smalltext|{}|left|\n", subtitle);
        d += "add_spacer|small|\n";
        return d;
    }

    /* @brief a divider line with a section name, e.g. "---- Unlocked ----" */
    inline std::string section(std::string_view name)
    {
        return std::format("add_spacer|small|\nadd_label|small|`9~ {} ~``|left|\nadd_spacer|small|\n", name);
    }

    /* @brief text progress bar: [#######-------] 50% */
    inline std::string bar(long long current, long long total, int width = 14)
    {
        if (total <= 0) total = 1;
        current = std::clamp<long long>(current, 0, total);
        const int filled = static_cast<int>(current * width / total);
        return std::format("`2[{}`4{}`2]`` `w{}%``", std::string(filled, '#'), std::string(width - filled, '-'),
            static_cast<int>(current * 100 / total));
    }

    /* @brief "1h 05m" / "3d 4h" style duration */
    inline std::string duration(unsigned long long seconds)
    {
        const unsigned long long d = seconds / 86400, h = (seconds % 86400) / 3600, m = (seconds % 3600) / 60;
        if (d) return std::format("{}d {}h", d, h);
        if (h) return std::format("{}h {:02}m", h, m);
        return std::format("{}m", m);
    }

    /* @brief themed dialog end. */
    inline std::string footer(std::string_view dialog_name, std::string_view close = "Close", std::string_view ok = "")
    {
        return std::format("add_spacer|small|\nend_dialog|{}|{}|{}|\nadd_quick_exit|\n", dialog_name, close, ok);
    }
}
