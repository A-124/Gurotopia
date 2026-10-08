#include "pch.hpp"

#include <cctype>
#include <ctime>
#include <format>
#include <ranges>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>
#include <utility>

#include "database/peer.hpp"
#include "database/world.hpp"
#include "tools/create_dialog.hpp"
#include "proton/Variant.hpp"
#include "__command.hpp"
#include "stats.hpp"

/* ==================== COUNTRY NAME MAP ==================== */
static const std::unordered_map<std::string, std::string> COUNTRY_NAME_MAP = {
    {"af", "Afghanistan"}, {"al", "Albania"}, {"dz", "Algeria"}, {"as", "American Samoa"}, {"ad", "Andorra"},
    {"ao", "Angola"}, {"ai", "Anguilla"}, {"aq", "Antarctica"}, {"ag", "Antigua and Barbuda"}, {"ar", "Argentina"},
    {"am", "Armenia"}, {"aw", "Aruba"}, {"au", "Australia"}, {"at", "Austria"}, {"az", "Azerbaijan"},
    {"bs", "Bahamas"}, {"bh", "Bahrain"}, {"bd", "Bangladesh"}, {"bb", "Barbados"}, {"by", "Belarus"},
    {"be", "Belgium"}, {"bz", "Belize"}, {"bj", "Benin"}, {"bm", "Bermuda"}, {"bt", "Bhutan"},
    {"bo", "Bolivia"}, {"ba", "Bosnia and Herzegovina"}, {"bw", "Botswana"}, {"br", "Brazil"}, {"bn", "Brunei"},
    {"bg", "Bulgaria"}, {"bf", "Burkina Faso"}, {"bi", "Burundi"}, {"cv", "Cabo Verde"}, {"kh", "Cambodia"},
    {"cm", "Cameroon"}, {"ca", "Canada"}, {"ky", "Cayman Islands"}, {"cf", "Central African Republic"}, {"td", "Chad"},
    {"cl", "Chile"}, {"cn", "China"}, {"co", "Colombia"}, {"km", "Comoros"}, {"cg", "Congo"},
    {"cd", "Congo, DR"}, {"ck", "Cook Islands"}, {"cr", "Costa Rica"}, {"ci", "Cote d'Ivoire"}, {"hr", "Croatia"},
    {"cu", "Cuba"}, {"cy", "Cyprus"}, {"cz", "Czech Republic"}, {"dk", "Denmark"}, {"dj", "Djibouti"},
    {"dm", "Dominica"}, {"do", "Dominican Republic"}, {"ec", "Ecuador"}, {"eg", "Egypt"}, {"sv", "El Salvador"},
    {"gq", "Equatorial Guinea"}, {"er", "Eritrea"}, {"ee", "Estonia"}, {"sz", "Eswatini"}, {"et", "Ethiopia"},
    {"fj", "Fiji"}, {"fi", "Finland"}, {"fr", "France"}, {"ga", "Gabon"}, {"gm", "Gambia"},
    {"ge", "Georgia"}, {"de", "Germany"}, {"gh", "Ghana"}, {"gi", "Gibraltar"}, {"gr", "Greece"},
    {"gl", "Greenland"}, {"gd", "Grenada"}, {"gu", "Guam"}, {"gt", "Guatemala"}, {"gn", "Guinea"},
    {"gw", "Guinea-Bissau"}, {"gy", "Guyana"}, {"ht", "Haiti"}, {"hn", "Honduras"}, {"hk", "Hong Kong"},
    {"hu", "Hungary"}, {"is", "Iceland"}, {"in", "India"}, {"id", "Indonesia"}, {"ir", "Iran"},
    {"iq", "Iraq"}, {"ie", "Ireland"}, {"il", "Israel"}, {"it", "Italy"}, {"jm", "Jamaica"},
    {"jp", "Japan"}, {"jo", "Jordan"}, {"kz", "Kazakhstan"}, {"ke", "Kenya"}, {"ki", "Kiribati"},
    {"kp", "North Korea"}, {"kr", "South Korea"}, {"kw", "Kuwait"}, {"kg", "Kyrgyzstan"}, {"la", "Laos"},
    {"lv", "Latvia"}, {"lb", "Lebanon"}, {"ls", "Lesotho"}, {"lr", "Liberia"}, {"ly", "Libya"},
    {"li", "Liechtenstein"}, {"lt", "Lithuania"}, {"lu", "Luxembourg"}, {"mo", "Macao"}, {"mg", "Madagascar"},
    {"mw", "Malawi"}, {"my", "Malaysia"}, {"mv", "Maldives"}, {"ml", "Mali"}, {"mt", "Malta"},
    {"mh", "Marshall Islands"}, {"mr", "Mauritania"}, {"mu", "Mauritius"}, {"mx", "Mexico"}, {"fm", "Micronesia"},
    {"md", "Moldova"}, {"mc", "Monaco"}, {"mn", "Mongolia"}, {"me", "Montenegro"}, {"ma", "Morocco"},
    {"mz", "Mozambique"}, {"mm", "Myanmar"}, {"na", "Namibia"}, {"nr", "Nauru"}, {"np", "Nepal"},
    {"nl", "Netherlands"}, {"nz", "New Zealand"}, {"ni", "Nicaragua"}, {"ne", "Niger"}, {"ng", "Nigeria"},
    {"mk", "North Macedonia"}, {"no", "Norway"}, {"om", "Oman"}, {"pk", "Pakistan"}, {"pw", "Palau"},
    {"ps", "Palestine"}, {"pa", "Panama"}, {"pg", "Papua New Guinea"}, {"py", "Paraguay"}, {"pe", "Peru"},
    {"ph", "Philippines"}, {"pl", "Poland"}, {"pt", "Portugal"}, {"qa", "Qatar"}, {"ro", "Romania"},
    {"ru", "Russian Federation"}, {"rw", "Rwanda"}, {"kn", "Saint Kitts"}, {"lc", "Saint Lucia"}, {"vc", "Saint Vincent"},
    {"ws", "Samoa"}, {"sm", "San Marino"}, {"st", "Sao Tome"}, {"sa", "Saudi Arabia"}, {"sn", "Senegal"},
    {"rs", "Serbia"}, {"sc", "Seychelles"}, {"sl", "Sierra Leone"}, {"sg", "Singapore"}, {"sk", "Slovakia"},
    {"si", "Slovenia"}, {"sb", "Solomon Islands"}, {"so", "Somalia"}, {"za", "South Africa"}, {"ss", "South Sudan"},
    {"es", "Spain"}, {"lk", "Sri Lanka"}, {"sd", "Sudan"}, {"sr", "Suriname"}, {"se", "Sweden"},
    {"ch", "Switzerland"}, {"sy", "Syria"}, {"tw", "Taiwan"}, {"tj", "Tajikistan"}, {"tz", "Tanzania"},
    {"th", "Thailand"}, {"tl", "Timor-Leste"}, {"tg", "Togo"}, {"to", "Tonga"}, {"tt", "Trinidad and Tobago"},
    {"tn", "Tunisia"}, {"tr", "Turkey"}, {"tm", "Turkmenistan"}, {"tv", "Tuvalu"}, {"ug", "Uganda"},
    {"ua", "Ukraine"}, {"ae", "UAE"}, {"gb", "United Kingdom"}, {"us", "United States"}, {"uy", "Uruguay"},
    {"uz", "Uzbekistan"}, {"vu", "Vanuatu"}, {"ve", "Venezuela"}, {"vn", "Vietnam"}, {"ye", "Yemen"},
    {"zm", "Zambia"}, {"zw", "Zimbabwe"}
};

/* server start time (seconds since epoch) */
static const std::time_t SERVER_START_TIME = std::time(nullptr);
static constexpr int MAX_ONLINE = 3000;

/* ==================== HELPERS ==================== */

static std::string format_uptime(long seconds)
{
    long days = seconds / 86400; seconds %= 86400;
    long hours = seconds / 3600; seconds %= 3600;
    long minutes = seconds / 60;
    long secs = seconds % 60;

    if (days > 0)
        return std::format("{} days, {} hours, {} minutes, {} seconds", days, hours, minutes, secs);
    return std::format("{} hours, {} minutes, {} seconds", hours, minutes, secs);
}

/* ==================== MAIN COMMAND ==================== */

void stats_command(ENetEvent& event, const std::string_view)
{
    ::peer* pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    int online_count = 0;
    std::unordered_map<std::string, int> countries;
    std::vector<std::string> player_list;

    /* iterate lahat ng connected peers gamit ang peers() helper */
    peers("", peer_condition::PEER_ALL, [&](ENetPeer& p)
    {
        ::peer* pOther = static_cast<::peer*>(p.data);
        if (!pOther) return;
        if (pOther->growid.empty()) return; // hindi pa tapos mag-login

        online_count++;

        /* country */
        if (!pOther->country.empty())
        {
            std::string c = pOther->country;
            std::ranges::transform(c, c.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            countries[c]++;
        }

        /* player list (walang ping dahil wala sa ::peer) */
        player_list.push_back(std::format("`w{}``", pOther->display_growid));
    });

    /* sort countries by count desc */
    std::vector<std::pair<std::string, int>> sorted_countries(countries.begin(), countries.end());
    std::ranges::sort(sorted_countries, [](const auto& a, const auto& b) { return a.second > b.second; });

    /* ==================== BUILD DIALOG ==================== */
    const long uptime = std::time(nullptr) - SERVER_START_TIME;
    const int current_online = std::min(online_count, MAX_ONLINE);
    const std::string server_name = "Gurotopia";

    create_dialog dialog;
    dialog.add_label_with_icon("big", "`oServer Statistics", 3802);
    dialog.add_label_with_icon("medium", "`oInfo", 7190);
    dialog.add_player_info(
        std::format("  `oServer: {}``", server_name),
        std::format("  `oOnline:``|{}|{}|({}/{})", current_online, MAX_ONLINE, current_online, MAX_ONLINE),
        current_online, MAX_ONLINE
    );
    dialog.add_smalltext(std::format("`oUptime: `2{}", format_uptime(uptime)));
    dialog.add_smalltext("`o----------------------------");

    /* Countries */
    if (!sorted_countries.empty())
    {
        dialog.add_label_with_icon("small", "`oPlayer Country", 3394);
        const int max_display = 20;
        int displayed = 0;
        for (const auto& [code, count] : sorted_countries)
        {
            if (displayed >= max_display) break;

            std::string name;
            if (COUNTRY_NAME_MAP.contains(code))
            {
                name = COUNTRY_NAME_MAP.at(code);
            }
            else
            {
                name = code;
                std::ranges::transform(name, name.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            }

            dialog.add_custom_button("",
                std::format("image:interface/flags/{}.rttex;image_size:16,12;width:0.03;state:visibled;", code));
            dialog.add_smalltext(std::format("`w{} - `2{} `$User Online", name, count));
            displayed++;
        }
        if (static_cast<int>(sorted_countries.size()) > max_display)
            dialog.add_smalltext(std::format("`oAnd more... ({})", static_cast<int>(sorted_countries.size()) - max_display));
        dialog.add_smalltext("`o----------------------------");
    }

    /* Players */
    dialog.add_label_with_icon("small", "`oPlayers Username", 1280);
    dialog.add_custom_break();

    std::string players_str;
    for (std::size_t i = 0; i < player_list.size(); ++i)
    {
        players_str += player_list[i];
        if (i + 1 < player_list.size()) players_str += ", ";
    }
    dialog.add_label("small", players_str);
    dialog.add_quick_exit();

    const std::string dialog_str = dialog.end_dialog("ons_stats", "Cancel", "OK");

    /* send sa client gamit ang send_varlist */
    send_varlist(event.peer, VariantList{ Variant("OnDialogRequest"), Variant(dialog_str) }, -1, 0);
}