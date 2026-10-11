#include "pch.hpp"
#include <fstream>
#include <map>
#include <charconv>
#include <sstream>
#include "database/peer.hpp"
#include "database/items.hpp"
#include "database/item_registry.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "gameplay/donation_system.hpp"

namespace donation_system {
namespace {
struct donor { long long rarity{}; unsigned rewards{}; };
std::map<int, donor> donors;
long long total_rarity{};
int total_toys{};
bool loaded{};
struct reward { long long threshold; int item; const char* name; };
constexpr reward rewards[] = {
    {100000, 4292, "Stuff-4-Toys Tote Bag"},
    {250000, 4286, "Stuff-4-Toys Shirt"},
    {750000, 25972, "Frozen Da Vinci Wings"},
    {2000000, 4288, "Present Goblin Home Edition"},
    {3500000, 25964, "Crystallized Silk Scarf"},
    {5000000, 10500, "Snow Aura"}
};
int count_item(const ::peer& p, int id) {
    int count = 0;
    for (const auto& s : p.slots) if (s.id == id && s.count > 0) count += s.count;
    return count;
}
void load() {
    if (loaded) return;
    loaded = true;
    std::ifstream f("resources/stuff4toys.txt");
    std::string kind;
    while (f >> kind) {
        if (kind == "GLOBAL") f >> total_rarity >> total_toys;
        else if (kind == "PLAYER") {
            int uid{}; donor d{}; f >> uid >> d.rarity >> d.rewards;
            if (uid > 0) donors[uid] = d;
        }
    }
}
bool save() {
    std::ofstream f("resources/stuff4toys.txt", std::ios::trunc);
    if (!f) return false;
    f << "GLOBAL " << total_rarity << ' ' << total_toys << '\n';
    for (const auto& [uid, d] : donors) f << "PLAYER " << uid << ' ' << d.rarity << ' ' << d.rewards << '\n';
    return static_cast<bool>(f);
}
}
void command(ENetEvent& event, std::string_view text) {
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    load();
    std::istringstream in{std::string(text)};
    std::string command_name, action;
    int item_id{}, quantity{};
    in >> command_name >> action;
    if (action == "status" || action.empty()) {
        const auto d = donors[p->user_id];
        on::ConsoleMessage(event.peer, std::format("Stuff-4-Toys: your donated rarity {}, global rarity {}, rewards issued {}. Use /donate <item_id> <quantity>.", d.rarity, total_rarity, total_toys));
        return;
    }
    try { std::size_t used{}; item_id = std::stoi(action, &used); if (used != action.size()) throw std::invalid_argument("item"); }
    catch (...) { on::ConsoleMessage(event.peer, "Usage: /donate <item_id> <quantity> | /donate status"); return; }
    in >> quantity;
    if (!in || quantity < 1 || quantity > 200 || item_id <= 0 || item_id >= static_cast<int>(items.size())) {
        on::ConsoleMessage(event.peer, "Usage: /donate <item_id> <quantity> (quantity 1-200).");
        return;
    }
    const ::item& item = id_to_item(static_cast<u_short>(item_id));
    if (item.rarity <= 0 || item.rarity == 999) {
        on::ConsoleMessage(event.peer, "That item has no valid donation rarity or is excluded from donations.");
        return;
    }
    if (count_item(*p, item_id) < quantity) { on::ConsoleMessage(event.peer, "You do not have enough of that item."); return; }
    const long long donation = static_cast<long long>(item.rarity) * quantity;
    donor& d = donors[p->user_id];
    const long long updated = d.rarity + donation;
    std::vector<const reward*> unlocked;
    for (std::size_t i = 0; i < std::size(rewards); ++i)
        if (updated >= rewards[i].threshold && !(d.rewards & (1u << i))) unlocked.push_back(&rewards[i]);
    // Validate all milestone rewards before consuming any donated items.
    std::size_t new_reward_slots = 0;
    for (const reward* r : unlocked) {
        if (!item_registry::exists(static_cast<u_short>(r->item)) || count_item(*p, r->item) >= 200) {
            on::ConsoleMessage(event.peer, "A milestone reward is unavailable or your reward stack is full.");
            return;
        }
        const bool stack = std::ranges::any_of(p->slots, [r](const ::slot& s) { return s.id == r->item && s.count > 0; });
        if (!stack) ++new_reward_slots;
    }
    if (p->slots.size() + new_reward_slots > static_cast<std::size_t>(std::max(p->slot_size, 0))) {
        on::ConsoleMessage(event.peer, "Make backpack room for your milestone rewards before donating.");
        return;
    }
    modify_item_inventory(event, {static_cast<short>(item_id), static_cast<short>(-quantity)});
    d.rarity = updated;
    total_rarity += donation;
    for (const reward* r : unlocked) {
        modify_item_inventory(event, {static_cast<short>(r->item), 1});
        const auto index = static_cast<unsigned>(r - rewards);
        d.rewards |= (1u << index);
        ++total_toys;
        on::ConsoleMessage(event.peer, std::format("Milestone unlocked: {}!", r->name));
    }
    if (!save()) on::ConsoleMessage(event.peer, "Warning: donation applied, but persistent donation stats could not be saved.");
    send_inventory_state(event);
    on::ConsoleMessage(event.peer, std::format("Thank you! Donated {} x{} ({} rarity). Personal total: {}; global total: {}.", item.raw_name, quantity, donation, d.rarity, total_rarity));
}

void open_box(ENetEvent& event) {
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    load();
    const auto own = donors[p->user_id].rarity;
    std::string d = "set_default_color|\x60o\n";
    d += "add_label_with_icon|big|Stuff-4-Toys Donation Box|left|4264|\n";
    d += "add_textbox|Donate items with rarity to earn milestone rewards. Items with rarity 999 cannot be donated.|left|\n";
    d += std::format("add_textbox|Your donated rarity: {}|left|\n", own);
    d += std::format("add_textbox|Global donated rarity: {}|left|\n", total_rarity);
    d += std::format("add_textbox|Total milestone rewards issued: {}|left|\n", total_toys);
    d += "add_spacer|small|\nadd_label|small|Reward milestones|left|\n";
    for (const auto& r : rewards)
        d += std::format("add_label_with_icon|small|{} rarity - {}|left|{}|\n", r.threshold, r.name, r.item);
    d += "add_spacer|small|\n";
    d += "add_text_input|donate_item_id|Item ID to donate:||8|\n";
    d += "add_text_input|donate_quantity|Quantity:|1|4|\n";
    d += "add_button|donation_confirm|Confirm Donation|noflags|0|0|\n";
    d += "add_quick_exit|\nend_dialog|gurotopia_donation_box||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
}
void handle_dialog_return(ENetEvent& event, const ::hPipe& pipe) {
    if (pipe["buttonClicked"] != "donation_confirm") return;
    const std::string item = pipe["donate_item_id"];
    const std::string amount = pipe["donate_quantity"];
    int item_id{}, quantity{};
    const auto [item_end, item_ec] = std::from_chars(item.data(), item.data() + item.size(), item_id);
    const auto [amount_end, amount_ec] = std::from_chars(amount.data(), amount.data() + amount.size(), quantity);
    if (item_ec != std::errc{} || item_end != item.data() + item.size() ||
        amount_ec != std::errc{} || amount_end != amount.data() + amount.size()) {
        on::ConsoleMessage(event.peer, "Enter a valid item ID and quantity.");
        open_box(event);
        return;
    }
    command(event, std::format("donate {} {}", item_id, quantity));
    open_box(event);
}

}
