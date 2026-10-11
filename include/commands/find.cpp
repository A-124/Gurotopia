#include "pch.hpp"
#include "find.hpp"
#include "database/item_registry.hpp"
#include "tools/ui.hpp"

namespace {
struct find_session { std::string query; int page{}; };
std::unordered_map<int, find_session> sessions;
void show_find(ENetEvent& event) {
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    auto& state = sessions[p->netid];
    std::string query = state.query;
    std::ranges::transform(query, query.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::vector<const ::item*> filtered;
    for (const auto& item : items) {
        if (item.id <= 0 || !item_registry::exists(static_cast<u_short>(item.id))) continue;
        std::string name = item.raw_name;
        std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (query.empty() || name.find(query) != std::string::npos) filtered.push_back(&item);
    }
    constexpr int page_size = 30;
    const int pages = std::max(1, static_cast<int>((filtered.size() + page_size - 1) / page_size));
    state.page = std::clamp(state.page, 0, pages - 1);
    std::string d = "set_default_color|\x60o\n";
    d += "add_label_with_icon|big|Browse Items|left|6016|\n";
    d += "add_textbox|Search the item database and select an item to add to your inventory.|left|\n";
    d += "add_text_input|find_search_query|Search item name:|" + ui::sanitize(state.query, 48) + "|48|\n";
    d += "add_button|find_search|Search|noflags|0|0|\n";
    d += "add_button|find_clear|Clear search|noflags|0|0|\n";
    d += std::format("add_textbox|Page {} of {} - {} matching items|left|\n", state.page + 1, pages, filtered.size());
    if (state.page > 0) d += "add_button|find_prev|Previous|noflags|0|0|\n";
    if (state.page + 1 < pages) d += "add_button|find_next|Next|noflags|0|0|\n";
    const std::size_t first = static_cast<std::size_t>(state.page * page_size);
    const std::size_t last = std::min(filtered.size(), first + page_size);
    for (std::size_t i = first; i < last; ++i) {
        const auto* item = filtered[i];
        d += std::format("add_button_with_icon|find_pick_{}|{}|staticBlueFrame|{}|\n",
            item->id, ui::sanitize(item->raw_name, 64), item->id);
    }
    d += "add_quick_exit|\nend_dialog|find_item||Close|\n";
    send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
}
}
void find(ENetEvent& event, const std::string_view) {
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    sessions[p->netid] = {};
    show_find(event);
}
void find_item(ENetEvent& event, const ::hPipe& pipe) {
    auto* p = event.peer && event.peer->data ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    const std::string button = pipe["buttonClicked"];
    auto& state = sessions[p->netid];
    if (button == "find_search") {
        state.query = pipe["find_search_query"]; state.page = 0; show_find(event);
    } else if (button == "find_clear") {
        state.query.clear(); state.page = 0; show_find(event);
    } else if (button == "find_prev" || button == "find_next") {
        state.page = std::max(0, state.page + (button == "find_next" ? 1 : -1)); show_find(event);
    } else if (button.starts_with("find_pick_")) {
        const std::string_view raw(button.data() + 10, button.size() - 10);
        int id{};
        const auto [end, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), id);
        if (ec != std::errc{} || end != raw.data() + raw.size() || id <= 0 ||
            !item_registry::exists(static_cast<u_short>(id))) { show_find(event); return; }
        modify_item_inventory(event, ::slot(id, 200));
        sessions.erase(p->netid);
    }
}
