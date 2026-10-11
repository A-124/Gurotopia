#include "pch.hpp"
#include <fstream>
#include <sstream>
#include <charconv>
#include <ctime>
#include "database/peer.hpp"
#include "database/item_registry.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "gameplay/item_exchange_system.hpp"

namespace item_exchange_system {
namespace {
struct exchange { int id{}, required_item{}, required_amount{}, reward_item{}, reward_amount{}; };
std::vector<exchange> configs;
bool loaded = false;
int next_id = 1;
int count_item(const ::peer& p, int id) {
    int count = 0;
    for (const auto& s : p.slots) if (s.id == id && s.count > 0) count += s.count;
    return count;
}
std::string item_name(int id) {
    if (id > 0 && id < static_cast<int>(items.size())) return id_to_item(static_cast<u_short>(id)).raw_name;
    return std::format("Item {}", id);
}
void load() {
    if (loaded) return;
    loaded = true;
    std::ifstream file("resources/item_exchange.txt");
    exchange e;
    while (file >> e.id >> e.required_item >> e.required_amount >> e.reward_item >> e.reward_amount) {
        if (e.id > 0 && e.required_item > 0 && e.required_amount > 0 &&
            e.reward_item > 0 && e.reward_amount > 0 &&
            e.required_item <= 32767 && e.reward_item <= 32767 &&
            e.required_amount <= 200 && e.reward_amount <= 200) {
            configs.push_back(e);
            next_id = std::max(next_id, e.id + 1);
        }
    }
}
bool save() {
    std::ofstream file("resources/item_exchange.txt", std::ios::trunc);
    if (!file) return false;
    for (const auto& e : configs)
        file << e.id << ' ' << e.required_item << ' ' << e.required_amount << ' ' << e.reward_item << ' ' << e.reward_amount << '\n';
    return static_cast<bool>(file);
}
void show(ENetEvent& event, std::string notice = {}) {
    auto* p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    load();
    std::string d = "add_label_with_icon|big|Item Exchange|left|1366|\n";
    d += "add_textbox|Trade the required items for the listed reward.|left|\n";
    if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);
    d += "add_spacer|small|\n";
    if (configs.empty()) d += "add_textbox|No exchanges are configured yet.|left|\n";
    for (const auto& e : configs) {
        d += std::format("add_label_with_icon|small|Give {} x{}  ->  Receive {} x{}|left|{}|\n",
            item_name(e.required_item), e.required_amount, item_name(e.reward_item), e.reward_amount, e.reward_item);
        d += std::format("add_button|exchange_{}|Exchange|noflags|0|0|\n", e.id);
    }
    d += "add_quick_exit|\nend_dialog|gurotopia_exchange||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
}
void execute(ENetEvent& event, int id) {
    auto* p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    load();
    auto it = std::ranges::find(configs, id, &exchange::id);
    if (it == configs.end()) { show(event, "That exchange is no longer available."); return; }
    const exchange e = *it;
    if (count_item(*p, e.required_item) < e.required_amount) {
        show(event, std::format("You need {} x{}.", item_name(e.required_item), e.required_amount)); return;
    }
    if (count_item(*p, e.reward_item) + e.reward_amount > 200) {
        show(event, "Your reward stack would exceed the inventory stack limit."); return;
    }
    const bool has_stack = std::ranges::any_of(p->slots, [&](const ::slot& s) { return s.id == e.reward_item && s.count > 0; });
    if (!has_stack && p->slots.size() >= static_cast<std::size_t>(std::max(p->slot_size, 0))) {
        show(event, "Your backpack is full."); return;
    }
    modify_item_inventory(event, {static_cast<short>(e.required_item), static_cast<short>(-e.required_amount)});
    modify_item_inventory(event, {static_cast<short>(e.reward_item), static_cast<short>(e.reward_amount)});
    send_inventory_state(event);
    std::ofstream log("resources/item_exchange.log", std::ios::app);
    if (log) log << static_cast<long long>(std::time(nullptr)) << ' ' << p->user_id << ' ' << e.id << ' ' << e.required_item << ' ' << e.required_amount << ' ' << e.reward_item << ' ' << e.reward_amount << '\n';
    show(event, std::format("Exchange complete: {} x{} received.", item_name(e.reward_item), e.reward_amount));
}
}
void command(ENetEvent& event, std::string_view) { show(event); }
void show_admin(ENetEvent& event, std::string notice = {}) {
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p || p->role != DEVELOPER) return;
    load();
    std::string d = "add_label_with_icon|big|Exchange Configuration|left|1366|\n";
    d += "add_textbox|Developer-only editor. Configure required item quantities and rewards.|left|\n";
    if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);
    for (const auto& e : configs) {
        d += std::format("add_textbox|#{}: {} x{} -> {} x{}|left|\n", e.id, item_name(e.required_item), e.required_amount, item_name(e.reward_item), e.reward_amount);
        d += std::format("add_button|manageexchange_remove_{}|Remove #{}|noflags|0|0|\n", e.id, e.id);
    }
    d += "add_spacer|small|\nadd_label|small|Add an exchange|left|\n";
    d += "add_text_input|exchange_required_item|Required item ID:||8|\n";
    d += "add_text_input|exchange_required_amount|Required amount:|1|4|\n";
    d += "add_text_input|exchange_reward_item|Reward item ID:||8|\n";
    d += "add_text_input|exchange_reward_amount|Reward amount:|1|4|\n";
    d += "add_button|manageexchange_add|Save exchange|noflags|0|0|\n";
    d += "add_quick_exit|\nend_dialog|gurotopia_exchange_admin||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
}
void manage_command(ENetEvent& event, std::string_view text) {
    auto* p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    if (p->role != DEVELOPER) { on::ConsoleMessage(event.peer, "Only developers can manage exchanges."); return; }
    load();
    std::istringstream in{std::string(text)};
    std::string cmd, action;
    in >> cmd >> action;
    if (action.empty()) { show_admin(event); return; }
    if (action == "list") {
        for (const auto& e : configs)
            on::ConsoleMessage(event.peer, std::format("#{}: {} x{} -> {} x{}", e.id, e.required_item, e.required_amount, e.reward_item, e.reward_amount));
        return;
    }
    if (action == "remove") {
        int id{}; in >> id;
        auto it = std::ranges::find(configs, id, &exchange::id);
        if (!in || it == configs.end()) { on::ConsoleMessage(event.peer, "Usage: /manageexchange remove <exchange_id>"); return; }
        exchange old = *it;
        configs.erase(it);
        if (!save()) { configs.push_back(old); on::ConsoleMessage(event.peer, "Could not save exchange config."); return; }
        on::ConsoleMessage(event.peer, "Exchange removed.");
        return;
    }
    if (action == "add") {
        exchange e{}; e.id = next_id++;
        in >> e.required_item >> e.required_amount >> e.reward_item >> e.reward_amount;
        if (!in || e.required_item <= 0 || e.required_item > 32767 || e.reward_item <= 0 || e.reward_item > 32767 ||
            e.required_amount < 1 || e.required_amount > 200 || e.reward_amount < 1 || e.reward_amount > 200 ||
            !item_registry::exists(static_cast<u_short>(e.required_item)) || !item_registry::exists(static_cast<u_short>(e.reward_item))) {
            on::ConsoleMessage(event.peer, "Usage: /manageexchange add <required_item> <required_amount> <reward_item> <reward_amount>");
            return;
        }
        configs.push_back(e);
        if (!save()) { configs.pop_back(); on::ConsoleMessage(event.peer, "Could not save exchange config."); return; }
        on::ConsoleMessage(event.peer, std::format("Added exchange #{}.", e.id));
        return;
    }
    on::ConsoleMessage(event.peer, "Usage: /manageexchange add <required_item> <required_amount> <reward_item> <reward_amount> | remove <id> | list");
}
void handle_dialog_return(ENetEvent& event, const ::hPipe& pipe) {
    const std::string button = pipe["buttonClicked"];
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (button == "manageexchange_add" || button.starts_with("manageexchange_remove_")) {
        if (!p || p->role != DEVELOPER) return;
        if (button == "manageexchange_add") {
            const std::string a = pipe["exchange_required_item"], b = pipe["exchange_required_amount"];
            const std::string c = pipe["exchange_reward_item"], d = pipe["exchange_reward_amount"];
            int req{}, req_amount{}, reward{}, reward_amount{};
            auto parse = [](const std::string& value, int& out) {
                const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
                return !value.empty() && ec == std::errc{} && end == value.data() + value.size();
            };
            if (!parse(a, req) || !parse(b, req_amount) || !parse(c, reward) || !parse(d, reward_amount) ||
                !item_registry::exists(static_cast<u_short>(req)) || !item_registry::exists(static_cast<u_short>(reward)) ||
                req_amount < 1 || req_amount > 200 || reward_amount < 1 || reward_amount > 200) {
                show_admin(event, "Enter valid item IDs and quantities from 1 to 200."); return;
            }
            const int id = next_id++;
            configs.push_back({id, req, req_amount, reward, reward_amount});
            if (!save()) { configs.pop_back(); show_admin(event, "Could not save exchange configuration."); return; }
            show_admin(event, "Exchange added."); return;
        }
        constexpr std::string_view remove_prefix = "manageexchange_remove_";
        const std::string_view value(button.data() + remove_prefix.size(), button.size() - remove_prefix.size());
        int id{};
        const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), id);
        auto it = std::ranges::find(configs, id, &exchange::id);
        if (ec != std::errc{} || end != value.data() + value.size() || it == configs.end()) { show_admin(event, "Exchange no longer exists."); return; }
        const auto previous = *it;
        configs.erase(it);
        if (!save()) { configs.push_back(previous); show_admin(event, "Could not save exchange configuration."); return; }
        show_admin(event, "Exchange removed."); return;
    }
    constexpr std::string_view prefix = "exchange_";
    if (!button.starts_with(prefix)) return;
    int id{};
    const auto value = std::string_view(button).substr(prefix.size());
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), id);
    if (ec == std::errc{} && end == value.data() + value.size()) execute(event, id);
}
}
