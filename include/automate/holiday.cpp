#include "pch.hpp"

#include "holiday.hpp"

u_char holiday = H_NONE;

std::tm localtime()
{
    std::time_t now  = std::time(nullptr);

    return *std::localtime(&now);
}

void check_for_holiday()
{
    std::tm time = localtime();

    if (time.tm_mon == 1/*feb*/ && (time.tm_mday >= 13 && time.tm_mday <= 13+7)) holiday = H_VALENTINES;
    if (time.tm_mon == 2/*march*/ && (time.tm_mday >= 14 && time.tm_mday <= 14+7)) holiday = H_PATRICKS;
    if (time.tm_mon == 6/*july*/ && (time.tm_mday >= 10 && time.tm_mday <= 10+7)) holiday = H_SUMMERFEST;
}

bool is_holiday_item_creation_season()
{
    const std::tm time = localtime();
    const int month = time.tm_mon + 1;
    const int day = time.tm_mday;

    // The server has no live event schedule endpoint. Use the recurring event
    // windows (late October/early November and late December/early January).
    const bool halloween = (month == 10 && day >= 23) || (month == 11 && day <= 7);
    const bool winterfest = (month == 12 && day >= 18) || (month == 1 && day <= 1);
    return halloween || winterfest;
}

std::string game_theme_string()
{
    switch (holiday)
    {
        case H_VALENTINES: return "valentines-theme";
        case H_PATRICKS:   return "patricks-theme";
        case H_SUMMERFEST: return "summerfest-theme";
        case H_G4G:        return "g4g-theme";
    }
    return "";
}

std::pair<std::string, std::string> holiday_greeting()
{
    switch (holiday)
    {
        case H_VALENTINES: return {"`5Valentine's Week!``",   "`4Happy Valentine's Week!``"};
        case H_PATRICKS:   return {"`5St. Patrick's Week!``", "`2Happy St. Patrick's Day!``"};
        case H_SUMMERFEST: return {"`5Summerfest``",          "`3Party down, it's `4Summerfest!`` Collect Fireworks and celebrate!``"};
        case H_G4G:        return {"`5Grow4Good Week!``",     "It's `2Grow4Good!`` Complete tasks, earn rewards and contribute to a great cause!"};
    }
    return {"", ""};
}

std::string holiday_banner()
{
    switch (holiday)
    {
        case H_VALENTINES: return "interface/large/gui_valentine_banner.rttex";
    }
    return "interface/large/news_banner.rttex"; // @note default:
}
