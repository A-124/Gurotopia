#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

class shouhin
{
public:
    std::string btn{};
    std::string name{};
    std::string rttx{};
    std::string description{};
    char tex1{};
    char tex2{}; // @todo may have to change variable type···
    int cost{};

    std::vector<std::pair<short, short>> items{}; // @note {id, amount}
};

/* Live store catalog. Replaced atomically by parse_store() so purchases never see a half-loaded list. */
extern std::vector<std::pair<short, shouhin>> shouhin_tachi; // @note {tab, shouhin}

/* @brief load (or reload) resources/store.txt. Builds a full catalog then swaps it in.
*  @return false if the file is missing or no valid products were parsed (previous catalog is kept). */
bool parse_store();

/* @return number of products currently in the live catalog */
std::size_t store_product_count() noexcept;

/* @return unix time of the last successful parse_store(), or 0 if never loaded */
std::time_t store_last_reload() noexcept;

/* @brief if resources/store.txt mtime is newer than the last successful load, re-parse it.
*  Safe to call from the main loop. Logs only when a reload actually happens. */
void store_auto_reload_tick();
