#include "pch.hpp"
#include <fstream>
#include <sstream>
#include <charconv>
#include <map>
#include "database/peer.hpp"
#include "database/world.hpp"
#include "database/item_registry.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "tools/ui.hpp"
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
    if (!file) {
        rules[2] = {{242, 1, 10}, {500, 1, 5}};
        save();
        return;
    }
    int tile{}, item{}, amount{}, chance{};
    while (file >> tile >> item >> amount >> chance) {
        if (tile > 0 && tile <= 32767 && item > 0 && item <= 32767 && item_registry::exists(static_cast<u_short>(item)) &&
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
void show_main(ENetEvent& event, std::string notice = {}) {
    if (!get_peer(event)) return;
    load();
    std::string d = "add_label_with_icon|big|Bonus Drop Manager|left|32|\n";
    d += "add_textbox|Configure extra drops when a tile breaks. Each rule rolls independently.|left|\n";
    if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", ui::sanitize(notice, 160));
    for (const auto& [tile, entries] : rules) {
        d += std::format("add_label_with_icon|small|Tile {} ({})|left|{}|\n", tile, entries.size(), tile);
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            d += std::format("add_textbox|Drop {}: item {} x{} at {}%|left|\n", i + 1, e.item, e.amount, e.chance);
            d += std::format("add_button|bonus_remove_{}_{}|Remove drop|noflags|0|0|\n", tile, i + 1);
        }
        d += std::format("add_button|bonus_remove_tile_{}|Remove all for tile|noflags|0|0|\n", tile);
    }
    d += "add_label|small|Add a drop rule|left|\n";
    d += "add_text_input|bonus_tile_id|Tile foreground ID:||8|\n";
    d += "add_text_input|bonus_item_id|Drop item ID:||8|\n";
    d += "add_text_input|bonus_amount|Amount:|1|4|\n";
    d += "add_text_input|bonus_chance|Chance percent:|10|4|\n";
    d += "add_button|bonus_add_rule|Add rule|noflags|0|0|\n";
    d += "add_button|bonus_reload|Reload saved rules|noflags|0|0|\n";
    d += "add_button|bonus_reset_default|Reset to defaults|noflags|0|0|\n";
    d += "add_quick_exit|\nend_dialog|bonus_main_menu||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
}
}
void command(ENetEvent& event, std::string_view text) {
    auto* p = get_peer(event);
    if (!p) return;
    if (p->role != DEVELOPER) { on::ConsoleMessage(event.peer, "Only developers can manage bonus drops."); return; }
    std::istringstream in{std::string(text)};
    std::string command_name, operation;
    in >> command_name >> operation;
    load();
    if (operation.empty()) { show_main(event); return; }
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
    if (!in || tile <= 0 || tile > 32767 || !item_registry::exists(static_cast<u_short>(tile)) || item <= 0 || item > 32767 || !item_registry::exists(static_cast<u_short>(item)) || amount < 1 || amount > 200 || chance < 1 || chance > 100) {
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
        if (item_registry::exists(static_cast<u_short>(e.item)) && RandomRange(0, 100) < e.chance)
            add_drop(event, ::slot(static_cast<short>(e.item), static_cast<short>(e.amount)), position, world);
}

void handle_dialog_return(ENetEvent& event, const ::hPipe& pipe) {
    auto* p = get_peer(event);
    if (!p || p->role != DEVELOPER) return;
    const std::string button = pipe["buttonClicked"];
    load();
    if (button == "bonus_add_rule") {
        int tile{}, item{}, amount{}, chance{};
        const std::string a = pipe["bonus_tile_id"], b = pipe["bonus_item_id"];
        const std::string c = pipe["bonus_amount"], d = pipe["bonus_chance"];
        auto parse = [](const std::string& v, int& out) {
            const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
            return !v.empty() && ec == std::errc{} && end == v.data() + v.size();
        };
        if (!parse(a, tile) || !parse(b, item) || !parse(c, amount) || !parse(d, chance) ||
            !item_registry::exists(static_cast<u_short>(tile)) || !item_registry::exists(static_cast<u_short>(item)) ||
            amount < 1 || amount > 200 || chance < 1 || chance > 100) {
            show_main(event, "Enter valid item IDs, amount 1-200, and chance 1-100."); return;
        }
        rules[tile].push_back({item, amount, chance});
        if (!save()) { rules[tile].pop_back(); if (rules[tile].empty()) rules.erase(tile); show_main(event, "Could not save the rule."); return; }
        show_main(event, "Bonus drop rule added."); return;
    }
    if (button == "bonus_reload") { rules.clear(); loaded = false; load(); show_main(event, "Configuration reloaded."); return; }
    if (button == "bonus_reset_default") {
        auto previous = rules;
        rules.clear(); rules[2] = {{242, 1, 10}, {500, 1, 5}};
        if (!save()) { rules = std::move(previous); show_main(event, "Could not save defaults."); return; }
        show_main(event, "Default rules restored for tile 2."); return;
    }
    constexpr std::string_view tile_prefix = "bonus_remove_tile_";
    constexpr std::string_view prefix = "bonus_remove_";
    const bool remove_tile = button.starts_with(tile_prefix);
    if (!remove_tile && !button.starts_with(prefix)) return;
    const std::string_view suffix(button.data() + (remove_tile ? tile_prefix.size() : prefix.size()), button.size() - (remove_tile ? tile_prefix.size() : prefix.size()));
    const auto sep = remove_tile ? std::string_view::npos : suffix.find('_');
    const auto tile_part = suffix.substr(0, sep);
    int tile{};
    const auto [tile_end, tile_ec] = std::from_chars(tile_part.data(), tile_part.data() + tile_part.size(), tile);
    if (tile_ec != std::errc{} || tile_end != tile_part.data() + tile_part.size()) { show_main(event); return; }
    auto it = rules.find(tile);
    if (it == rules.end()) { show_main(event, "That rule no longer exists."); return; }
    auto previous = rules;
    if (remove_tile) rules.erase(it);
    else {
        const auto value = suffix.substr(sep + 1);
        int index{};
        const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), index);
        if (ec != std::errc{} || end != value.data() + value.size() || index < 1 || index > static_cast<int>(it->second.size())) { show_main(event); return; }
        it->second.erase(it->second.begin() + index - 1);
        if (it->second.empty()) rules.erase(it);
    }
    if (!save()) { rules = std::move(previous); show_main(event, "Could not save the change."); return; }
    show_main(event, "Bonus drop rule removed.");
}

}
