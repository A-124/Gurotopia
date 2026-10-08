#pragma once

enum holiday : u_char
{
    H_NONE,
    H_VALENTINES,
    H_PATRICKS,
    H_SUMMERFEST = 6,
    H_G4G = 18
};
extern u_char holiday;

extern std::tm localtime();

extern void check_for_holiday();

// Seasonal item recipes tagged CAT_HOLIDAY are only available during these events.
extern bool is_holiday_item_creation_season();

extern std::string game_theme_string();

extern std::pair<std::string, std::string> holiday_greeting();

extern std::string holiday_banner();
