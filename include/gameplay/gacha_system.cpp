#include "pch.hpp"
#include <fstream>
#include <map>
#include <ctime>
#include "database/peer.hpp"
#include "database/item_registry.hpp"
#include "gameplay/gacha_system.hpp"

namespace gacha_system {
namespace {
constexpr int currency_id = 7188;
constexpr int single_cost = 5;
constexpr int ten_cost = 45;
constexpr int max_stack = 200;
struct prize { int id; const char* rarity; int weight; };
constexpr prize pool[] = {
    {2400, "Common", 500}, {2401, "Common", 500},
    {3200, "Rare", 250}, {3201, "Rare", 250},
    {5100, "Epic", 60}, {5101, "Epic", 60},
    {9000, "Legendary", 8}, {9001, "Legendary", 8}
};
struct history_entry { int item{}; std::string rarity; std::time_t time{}; };
struct player_data { int total{}; std::vector<history_entry> history; };
std::map<int, player_data> saved;
bool loaded = false;
int count_item(const ::peer& p, int id) {
    int total = 0;
    for (const auto& s : p.slots) if (s.id == id && s.count > 0) total += s.count;
    return total;
}
void load() {
    if (loaded) return;
    loaded = true;
    std::ifstream f("resources/gacha_history.txt");
    int uid{}, total{}, item{};
    long long timestamp{};
    std::string rarity;
    while (f >> uid >> total >> item >> rarity >> timestamp) {
        if (uid <= 0) continue;
        auto& d = saved[uid];
        d.total = std::max(d.total, total);
        if (item > 0) {
            d.history.push_back({item, rarity, static_cast<std::time_t>(timestamp)});
            if (d.history.size() > 30) d.history.erase(d.history.begin());
        }
    }
}
bool save() {
    std::ofstream f("resources/gacha_history.txt", std::ios::trunc);
    if (!f) return false;
    for (const auto& [uid, d] : saved) {
        if (d.history.empty()) f << uid << ' ' << d.total << " 0 None 0\n";
        for (const auto& h : d.history) f << uid << ' ' << d.total << ' ' << h.item << ' ' << h.rarity << ' ' << static_cast<long long>(h.time) << '\n';
    }
    return static_cast<bool>(f);
}
const prize& roll() {
    int total = 0;
    for (const auto& p : pool) total += p.weight;
    int n = RandomRange(0, total);
    for (const auto& p : pool) { if (n < p.weight) return p; n -= p.weight; }
    return pool[0];
}
std::string item_name(int id) {
    if (id > 0 && id < static_cast<int>(items.size())) return id_to_item(static_cast<u_short>(id)).raw_name;
    return std::format("Item {}", id);
}
void show(ENetEvent& event, std::string notice = {}) {
    auto* p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    load();
    const auto& d = saved[p->user_id];
    std::string dialog = "set_bg_color|10,10,40,220|\nset_border_color|168,85,247,255|\n";
    dialog += "add_label_with_icon|big|Gacha Banner|left|7188|\n";
    if (!notice.empty()) dialog += std::format("add_textbox|{}|left|\n", notice);
    dialog += std::format("add_label_with_icon|small|Blue Gem Locks: {}|left|7188|\n", count_item(*p, currency_id));
    dialog += std::format("add_textbox|Total pulls: {}|left|\n", d.total);
    dialog += "add_spacer|small|\n";
    dialog += "add_button|gacha_pull_1|Pull 1x (5 BGL)|noflags|0|0|\n";
    dialog += "add_button|gacha_pull_10|Pull 10x (45 BGL)|noflags|0|0|\n";
    dialog += "add_button|gacha_history|View Recent Results|noflags|0|0|\n";
    dialog += "add_quick_exit|\nend_dialog|gurotopia_gacha||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(dialog)});
}
void show_history(ENetEvent& event) {
    auto* p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    load();
    const auto& h = saved[p->user_id].history;
    std::string dialog = "add_label_with_icon|big|Gacha History|left|7188|\n";
    int start = std::max(0, static_cast<int>(h.size()) - 20);
    if (start == static_cast<int>(h.size())) dialog += "add_textbox|No pulls recorded yet.|left|\n";
    for (int i = static_cast<int>(h.size()) - 1; i >= start; --i) {
        const auto& e = h[static_cast<std::size_t>(i)];
        dialog += std::format("add_label_with_icon|small|[{}] {}|left|{}|\n", e.rarity, item_name(e.item), e.item);
    }
    dialog += "add_button|gacha_back|Back to Banner|noflags|0|0|\nend_dialog|gurotopia_gacha_history||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(dialog)});
}
void execute(ENetEvent& event, int pulls) {
    auto* p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p || (pulls != 1 && pulls != 10)) return;
    load();
    const int cost = pulls == 1 ? single_cost : ten_cost;
    if (count_item(*p, currency_id) < cost) { show(event, std::format("Need {} Blue Gem Locks.", cost)); return; }
    std::vector<const prize*> results;
    std::map<int, int> additions;
    for (int i = 0; i < pulls; ++i) { const prize& r = roll(); results.push_back(&r); ++additions[r.id]; }
    for (const auto& [id, amount] : additions) {
        if (id <= 0 || !item_registry::exists(static_cast<u_short>(id)) || count_item(*p, id) + amount > max_stack) {
            show(event, "Inventory has no room for those results. No currency was spent.");
            return;
        }
        const bool stack = std::ranges::any_of(p->slots, [id](const ::slot& s) { return s.id == id && s.count > 0; });
        if (!stack && p->slots.size() + additions.size() > static_cast<std::size_t>(std::max(p->slot_size, 0))) {
            show(event, "Your backpack is full. No currency was spent.");
            return;
        }
    }
    modify_item_inventory(event, {currency_id, static_cast<short>(-cost)});
    auto& data = saved[p->user_id];
    std::string result_dialog = "add_label_with_icon|big|Gacha Results|left|7188|\n";
    for (const prize* r : results) {
        modify_item_inventory(event, {static_cast<short>(r->id), 1});
        ++data.total;
        data.history.push_back({r->id, r->rarity, std::time(nullptr)});
        result_dialog += std::format("add_label_with_icon|small|[{}] {}|left|{}|\n", r->rarity, item_name(r->id), r->id);
    }
    if (data.history.size() > 30) data.history.erase(data.history.begin(), data.history.begin() + (data.history.size() - 30));
    save();
    send_inventory_state(event);
    result_dialog += "add_button|gacha_back|Back to Banner|noflags|0|0|\nend_dialog|gurotopia_gacha_result||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(result_dialog)});
}
}
void command(ENetEvent& event, std::string_view) { show(event); }
void handle_dialog_return(ENetEvent& event, const ::hPipe& pipe) {
    const auto button = pipe["buttonClicked"];
    if (button == "gacha_pull_1") execute(event, 1);
    else if (button == "gacha_pull_10") execute(event, 10);
    else if (button == "gacha_history") show_history(event);
    else if (button == "gacha_back") show(event);
}
}
