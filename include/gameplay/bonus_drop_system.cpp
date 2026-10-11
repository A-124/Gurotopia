#include "pch.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include "database/peer.hpp"
#include "database/world.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "tools/random.hpp"
#include "gameplay/bonus_drop_system.hpp"

namespace bonus_drop_system {
namespace {
struct drop_rule { int item{}; int amount{1}; int chance{100}; };
using rule_map = std::map<int, std::vector<drop_rule>>;
rule_map rules;
bool loaded = false;
void load() {
    if (loaded) return;
    loaded = true;
    std::ifstream file("resources/bonus_drops.txt");
    int tile{}, item{}, amount{}, chance{};
    while (file >> tile >> item >> amount >> chance) {
        if (tile > 0 && tile <= 32767 && item > 0 && item <= 32767 &&
            amount > 0 && amount <= 200 && chance >= 1 && chance <= 100)
            rules[tile].push_back({item, amount, chance});
    }
}
bool save() {
    std::ofstream file("resources/bonus_drops.txt", std::ios::trunc);
    if (!file) return false;
    for (const auto& [tile, entries] : rules)
        for (const auto& e : entries) file << tile << ' ' << e.item << ' ' << e.amount << ' ' << e.chance << '\n';
    return static_cast<bool>(file);
}
::peer* get_peer(ENetEvent& event) { return event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr; }
}
void command(ENetEvent& event, std::string_view text) {
    auto* p = get_peer(event);
    if (!p) return;
    if (p->role != DEVELOPER) { on::ConsoleMessage(event.peer, "Only developers can manage bonus drops."); return; }
    std::istringstream in{std::string(text)};
    std::string command_name, operation;
    in >> command_name >> operation;
    load();
    if (operation == "list") {
        int tile{}; in >> tile;
        auto it = rules.find(tile);
        if (!in || it == rules.end() || it->second.empty()) { on::ConsoleMessage(event.peer, "No bonus drops configured for that tile."); return; }
        for (std::size_t i = 0; i < it->second.size(); ++i) {
            const auto& e = it->second[i];
            on::ConsoleMessage(event.peer, std::format("{}: item {} x{} at {}% chance", i + 1, e.item, e.amount, e.chance));
        }
        return;
    }
    if (operation == "remove") {
        int tile{}, index{}; in >> tile >> index;
        auto it = rules.find(tile);
        if (!in || it == rules.end() || index < 1 || index > static_cast<int>(it->second.size())) {
            on::ConsoleMessage(event.peer, "Usage: /bonusdrop remove <tile_id> <index>");
            return;
        }
        it->second.erase(it->second.begin() + index - 1);
        if (it->second.empty()) rules.erase(it);
        on::ConsoleMessage(event.peer, save() ? "Bonus drop rule removed." : "Could not save bonus drop configuration.");
        return;
    }
    int tile{}, item{}, amount{1}, chance{100};
    try {
        std::size_t used{};
        tile = std::stoi(operation, &used);
        if (used != operation.size()) throw std::invalid_argument("tile");
    } catch (...) {
        on::ConsoleMessage(event.peer, "Usage: /bonusdrop <tile_id> <drop_item_id> <amount> <chance_percent> | /bonusdrop list <tile_id> | /bonusdrop remove <tile_id> <index>");
        return;
    }
    in >> item >> amount >> chance;
    if (!in || tile <= 0 || tile > 32767 || item <= 0 || item > 32767 || amount < 1 || amount > 200 || chance < 1 || chance > 100) {
        on::ConsoleMessage(event.peer, "Usage: /bonusdrop <tile_id> <drop_item_id> <amount> <chance_percent> (chance 1-100).");
        return;
    }
    rules[tile].push_back({item, amount, chance});
    if (!save()) { rules[tile].pop_back(); on::ConsoleMessage(event.peer, "Could not save resources/bonus_drops.txt."); return; }
    on::ConsoleMessage(event.peer, std::format("Bonus drop added: tile {}, item {} x{}, chance {}%.", tile, item, amount, chance));
}
void on_block_broken(ENetEvent& event, int tile_id, const ::pos& position, ::world& world) {
    load();
    auto it = rules.find(tile_id);
    if (it == rules.end()) return;
    for (const auto& e : it->second)
        if (RandomRange(0, 100) < e.chance)
            add_drop(event, ::slot(static_cast<short>(e.item), static_cast<short>(e.amount)), position, world);
}
}
