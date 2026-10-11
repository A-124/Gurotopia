#include "pch.hpp"
#include <chrono>
#include <unordered_map>
#include <fstream>
#include <charconv>
#include <filesystem>
#include <unordered_set>
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/AddNotification.hpp"
#include "database/item_registry.hpp"
#include "tools/ui.hpp"
#include "tools/random.hpp"
#include "gameplay/gacha_system.hpp"
#include "extras.hpp"

namespace
{
    const auto started = std::chrono::steady_clock::now(); // @note set when the server process loads

    ::peer *self_of(ENetEvent &event) { return (event.peer && event.peer->data) ? static_cast<::peer*>(event.peer->data) : nullptr; }
}

void command_flip(ENetEvent &event, std::string_view)
{
    ::peer *self = self_of(event);
    if (!self) return;
    if (self->netid == 0 || self->recent_worlds.back().empty())
    {
        on::ConsoleMessage(event.peer, "`4Enter a world before flipping a coin.``");
        return;
    }
    const bool heads = RandomRange(0, 2) == 0;
    const std::string message = std::format("`w{}`` flipped a coin: `${}``", self->display_growid, heads ? "HEADS" : "TAILS");
    peers(self->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p)
    {
        send_varlist(&p, { "OnTalkBubble", self->netid, message });
        on::ConsoleMessage(&p, message);
    });
}

void command_ping(ENetEvent &event, std::string_view)
{
    if (!self_of(event)) return;
    const unsigned rtt = event.peer->roundTripTime;
    const char *colour = rtt < 80 ? "`2" : rtt < 200 ? "`6" : "`4";
    on::ConsoleMessage(event.peer, std::format("`oYour ping: {}{} ms``", colour, rtt));
}

void command_uptime(ENetEvent &event, std::string_view)
{
    if (!self_of(event)) return;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
    on::ConsoleMessage(event.peer, std::format("`oServer uptime: `w{}``", ui::duration(static_cast<unsigned long long>(seconds))));
}

void command_count(ENetEvent &event, std::string_view)
{
    ::peer *self = self_of(event);
    if (!self) return;
    std::size_t online = 0, here = 0;
    for (ENetPeer *candidate : peers())
    {
        auto *p = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
        if (!p || p->growid.empty()) continue;
        ++online;
        if (self->netid != 0 && !self->recent_worlds.back().empty() && p->recent_worlds.back() == self->recent_worlds.back()) ++here;
    }
    on::ConsoleMessage(event.peer, std::format("`oPlayers online: `w{}``   In your world: `w{}``", online, here));
}

namespace
{
    std::unordered_map<int, std::chrono::steady_clock::time_point> legendary_broadcast_cooldowns;
}
void command_lsb(ENetEvent &event, std::string_view text)
{
    ::peer *self = self_of(event);
    if (!self) return;
    const auto split = text.find_first_of(" \t");
    std::string_view message = split == std::string_view::npos ? std::string_view{} : text.substr(split + 1);
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.front()))) message.remove_prefix(1);
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.back()))) message.remove_suffix(1);
    if (message.empty())
    {
        on::ConsoleMessage(event.peer, "Usage: /lsb <message> (requires Legendary Mask, item 25550).");
        return;
    }
    const auto mask = std::ranges::find(self->slots, static_cast<short>(25550), &::slot::id);
    if (mask == self->slots.end() || mask->count <= 0)
    {
        on::ConsoleMessage(event.peer, "You need a Legendary Mask (item 25550) to use /lsb.");
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (const auto it = legendary_broadcast_cooldowns.find(self->user_id); it != legendary_broadcast_cooldowns.end())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
        if (elapsed < 300)
        {
            const auto remaining = 300 - elapsed;
            on::ConsoleMessage(event.peer, std::format("Legendary broadcast cooldown: {}m {}s.", remaining / 60, remaining % 60));
            return;
        }
    }
    const std::string world = self->recent_worlds.back().empty() ? "EXIT" : self->recent_worlds.back();
    const std::string announcement = std::format("`5[LEGEND-SB] from ({} `5) in [`o{}`5]: `o{}", self->nametag(), world, message);
    legendary_broadcast_cooldowns[self->user_id] = now;
    for (ENetPeer *candidate : peers())
    {
        auto *target = candidate ? static_cast<::peer*>(candidate->data) : nullptr;
        if (target && !target->growid.empty()) {
            on::ConsoleMessage(candidate, announcement);
            on::AddNotification(candidate, std::format("[LEGEND-SB] {} in {}: {}", self->growid, world, message), "audio/hub_open.wav");
        }
    }
}

namespace
{
    std::optional<int> load_scanned_item()
    {
        std::ifstream file{"resources/scanitem.txt"};
        int id{};
        if (!(file >> id) || id <= 0 || id > 32767 || !item_registry::exists(static_cast<u_short>(id))) return std::nullopt;
        return id;
    }
    bool save_scanned_item(int id)
    {
        if (id <= 0 || id > 32767 || !item_registry::exists(static_cast<u_short>(id))) return false;
        std::ofstream file{"resources/scanitem.txt", std::ios::trunc};
        return file && static_cast<bool>(file << id << '\n');
    }
    struct scan_session { std::string query; int page{}; };
    std::unordered_map<int, scan_session> scan_sessions;
    void show_item_browser(ENetEvent& event)
    {
        auto* self = self_of(event);
        if (!self) return;
        auto& state = scan_sessions[self->netid];
        std::vector<const ::item*> filtered;
        std::string query = state.query;
        std::ranges::transform(query, query.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        for (const auto& item : items) {
            if (item.id <= 0 || !item_registry::exists(static_cast<u_short>(item.id))) continue;
            std::string name = item.raw_name;
            std::ranges::transform(name, name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (query.empty() || name.find(query) != std::string::npos) filtered.push_back(&item);
        }
        constexpr int page_size = 35;
        const int pages = std::max(1, static_cast<int>((filtered.size() + page_size - 1) / page_size));
        state.page = std::clamp(state.page, 0, pages - 1);
        std::string d = "add_label_with_icon|big|Browse Items|left|6016|\n";
        d += "add_text_input|scan_search_query|Search item name:|" + ui::sanitize(state.query, 48) + "|48|\n";
        d += "add_button|scan_search|Search|noflags|0|0|\n";
        d += "add_button|scan_clear_search|Clear search|noflags|0|0|\n";
        d += std::format("add_textbox|Page {} of {} - {} matching items|left|\n", state.page + 1, pages, filtered.size());
        if (state.page > 0) d += "add_button|scan_prev_page|Previous|noflags|0|0|\n";
        if (state.page + 1 < pages) d += "add_button|scan_next_page|Next|noflags|0|0|\n";
        const std::size_t first = static_cast<std::size_t>(state.page * page_size);
        const std::size_t last = std::min(filtered.size(), first + page_size);
        for (std::size_t i = first; i < last; ++i) {
            const auto* item = filtered[i];
            d += std::format("add_button_with_icon|scan_pick_{}|{}|staticBlueFrame|{}|\n", item->id, ui::sanitize(item->raw_name, 64), item->id);
        }
        d += "add_button|scan_browser_back|Back|noflags|0|0|\nend_dialog|scanitem_browser||Close|\n";
        send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
    }
    int item_count(const ::peer& p, int id)
    {
        int total = 0;
        for (const auto& slot : p.slots) if (slot.id == id && slot.count > 0) total += slot.count;
        return total;
    }
    std::string scan_item_name(int id)
    {
        if (id > 0 && id < static_cast<int>(items.size())) return id_to_item(static_cast<u_short>(id)).raw_name;
        return std::format("Item {}", id);
    }
    void show_scanitem_panel(ENetEvent& event, std::string notice = {})
    {
        const auto id = load_scanned_item();
        std::string d = "add_label_with_icon|big|Item Scanner|left|32|\n";
        d += "add_textbox|Register an item and scan online player inventories. The scanner reads online and saved offline player inventories plus loaded world objects, display blocks, and vending stock.|left|\n";
        if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", ui::sanitize(notice, 180));
        if (id) {
            d += std::format("add_label_with_icon|small|Tracked: {} (ID {})|left|{}|\n", scan_item_name(*id), *id, *id);
            d += "add_button|scanitem_scan|Scan online players|noflags|0|0|\n";
            d += "add_button|scanitem_clear|Remove tracked item|noflags|0|0|\n";
        } else {
            d += "add_textbox|No item is currently registered.|left|\n";
        }
        d += "add_spacer|small|\n";
        d += "add_text_input|scanitem_item_id|Item ID to track:||8|\n";
        d += "add_button|scanitem_save|Register item ID|noflags|0|0|\n";
        d += "add_button|scanitem_browse|Browse/search items|noflags|0|0|\n";
        d += "add_quick_exit|\nend_dialog|scanitem_main||Close|\n";
        send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
    }
    void show_scanitem_results(ENetEvent& event, int id)
    {
        auto* self = self_of(event);
        if (!self) return;
        std::vector<std::pair<std::string, int>> found;
        std::unordered_set<std::string> online_names;
        long long online_total = 0, offline_total = 0;
        std::size_t online_holders = 0, offline_holders = 0;
        for (ENetPeer* candidate : peers()) {
            auto* target = candidate && candidate->data ? static_cast<::peer*>(candidate->data) : nullptr;
            if (!target || target->growid.empty()) continue;
            std::string key = target->growid;
            std::ranges::transform(key, key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            online_names.insert(key);
            const int amount = item_count(*target, id);
            if (amount <= 0) continue;
            found.emplace_back(target->growid, amount);
            online_total += amount;
            ++online_holders;
        }
        // The inventory blob is: int slot_size, short slot_count, then (short item_id, short count) pairs.
        // Scan saved inventories too, excluding online GrowIDs so they are not double-counted.
        if (db && mysql_query(db, "SELECT growid, inventory FROM peer") == 0) {
            if (MYSQL_RES* result = mysql_store_result(db)) {
                while (MYSQL_ROW row = mysql_fetch_row(result)) {
                    if (!row[0] || !row[1]) continue;
                    std::string name = row[0];
                    std::string key = name;
                    std::ranges::transform(key, key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    if (online_names.contains(key)) continue;
                    unsigned long* lengths = mysql_fetch_lengths(result);
                    if (!lengths || lengths[1] < sizeof(int) + sizeof(short)) continue;
                    const auto* bytes = reinterpret_cast<const unsigned char*>(row[1]);
                    short slots{};
                    std::memcpy(&slots, bytes + sizeof(int), sizeof(short));
                    const std::size_t max_slots = (lengths[1] - sizeof(int) - sizeof(short)) / (sizeof(short) * 2);
                    const std::size_t count = std::min<std::size_t>(static_cast<std::size_t>(std::max<short>(slots, 0)), max_slots);
                    int amount{};
                    std::size_t offset = sizeof(int) + sizeof(short);
                    for (std::size_t i = 0; i < count; ++i) {
                        short item_id{}, item_amount{};
                        std::memcpy(&item_id, bytes + offset, sizeof(short)); offset += sizeof(short);
                        std::memcpy(&item_amount, bytes + offset, sizeof(short)); offset += sizeof(short);
                        if (item_id == id && item_amount > 0) amount += item_amount;
                    }
                    if (amount > 0) {
                        found.emplace_back("[Offline] " + name, amount);
                        offline_total += amount;
                        ++offline_holders;
                    }
                }
                mysql_free_result(result);
            }
        }
        std::vector<std::pair<std::string, long long>> world_holders;
        long long world_total = 0;
        for (const auto& world : worlds) {
            long long amount = 0;
            for (const auto& object : world.objects)
                if (object.id == id && object.count > 0) amount += object.count;
            for (const auto& display : world.displays)
                if (display.id == id) ++amount;
            for (const auto& machine : world.vending_machines)
                if (machine.item_id == id && machine.stock > 0) amount += machine.stock;
            if (amount > 0) { world_holders.emplace_back(world.name, amount); world_total += amount; }
        }
        std::ranges::sort(found, [](const auto& a, const auto& b) { return a.second > b.second; });
        std::ranges::sort(world_holders, [](const auto& a, const auto& b) { return a.second > b.second; });
        const long long total = online_total + offline_total + world_total;
        std::string d = std::format("add_label_with_icon|big|Scan Results: {}|left|{}|\n", scan_item_name(id), id);
        d += std::format("add_textbox|Online players: {} holders / {} items. Offline players: {} holders / {} items.|left|\n", online_holders, online_total, offline_holders, offline_total);
        d += std::format("add_textbox|Loaded world objects, displays and vending stock: {} items across {} worlds.|left|\n", world_total, world_holders.size());
        d += std::format("add_textbox|Combined scanned quantity: {}|left|\n", total);
        if (found.empty()) d += "add_textbox|No online or offline player inventories currently hold this item.|left|\n";
        for (std::size_t i = 0; i < std::min<std::size_t>(found.size(), 50); ++i)
            d += std::format("add_textbox|{}: {}|left|\n", ui::sanitize(found[i].first, 48), found[i].second);
        if (found.size() > 50) d += std::format("add_smalltext|{} more player holder(s) omitted.|left|\n", found.size() - 50);
        if (!world_holders.empty()) {
            d += "add_spacer|small|\nadd_label|small|World holders|left|\n";
            for (std::size_t i = 0; i < std::min<std::size_t>(world_holders.size(), 20); ++i)
                d += std::format("add_textbox|{}: {}|left|\n", ui::sanitize(world_holders[i].first, 48), world_holders[i].second);
            if (world_holders.size() > 20) d += std::format("add_smalltext|{} more world(s) omitted.|left|\n", world_holders.size() - 20);
        }
        d += "add_button|scanitem_back|Back to scanner|noflags|0|0|\nend_dialog|scanitem_results||Close|\n";
        send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
    }
}
void command_scanitem(ENetEvent &event, std::string_view text)
{
    ::peer *self = self_of(event);
    if (!self) return;
    if (self->role < MODERATOR) {
        on::ConsoleMessage(event.peer, "Only staff can use /scanitem.");
        return;
    }
    const auto split = text.find_first_of(" \t");
    std::string_view arg = split == std::string_view::npos ? std::string_view{} : text.substr(split + 1);
    while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.front()))) arg.remove_prefix(1);
    while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.back()))) arg.remove_suffix(1);
    if (!arg.empty()) {
        int id{};
        const auto [end, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), id);
        if (ec != std::errc{} || end != arg.data() + arg.size() || !save_scanned_item(id)) {
            on::ConsoleMessage(event.peer, "Usage: /scanitem [valid_item_id]");
            return;
        }
        show_scanitem_panel(event, std::format("Now tracking {} (ID {}).", scan_item_name(id), id));
        return;
    }
    show_scanitem_panel(event);
}
void handle_scanitem_dialog_return(ENetEvent& event, const ::hPipe& pipe)
{
    auto* self = self_of(event);
    if (!self || self->role < MODERATOR) return;
    const std::string button = pipe["buttonClicked"];
    if (button == "scanitem_browse") {
        scan_sessions[self->netid] = {};
        show_item_browser(event);
    } else if (button == "scan_search") {
        auto& state = scan_sessions[self->netid];
        state.query = pipe["scan_search_query"];
        state.page = 0;
        show_item_browser(event);
    } else if (button == "scan_clear_search") {
        auto& state = scan_sessions[self->netid];
        state.query.clear(); state.page = 0;
        show_item_browser(event);
    } else if (button == "scan_prev_page" || button == "scan_next_page") {
        auto& state = scan_sessions[self->netid];
        state.page = std::max(0, state.page + (button == "scan_next_page" ? 1 : -1));
        show_item_browser(event);
    } else if (button == "scan_browser_back") {
        show_scanitem_panel(event);
    } else if (button.starts_with("scan_pick_")) {
        const std::string_view raw(button.data() + 10, button.size() - 10);
        int id{};
        const auto [end, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), id);
        if (ec != std::errc{} || end != raw.data() + raw.size() || !save_scanned_item(id)) {
            show_item_browser(event); return;
        }
        scan_sessions.erase(self->netid);
        show_scanitem_panel(event, std::format("Registered {} (ID {}).", scan_item_name(id), id));
    } else if (button == "scanitem_save") {
        const std::string raw = pipe["scanitem_item_id"];
        int id{};
        const auto [end, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), id);
        if (ec != std::errc{} || end != raw.data() + raw.size() || !save_scanned_item(id)) {
            show_scanitem_panel(event, "Enter a valid item ID that exists in the item database.");
            return;
        }
        show_scanitem_panel(event, std::format("Registered {} (ID {}).", scan_item_name(id), id));
    } else if (button == "scanitem_clear") {
        std::error_code ec;
        std::filesystem::remove("resources/scanitem.txt", ec);
        show_scanitem_panel(event, ec ? "Could not remove the tracked item file." : "Tracked item removed.");
    } else if (button == "scanitem_scan") {
        const auto id = load_scanned_item();
        if (!id) { show_scanitem_panel(event, "Register an item first."); return; }
        show_scanitem_results(event, *id);
    } else if (button == "scanitem_back") show_scanitem_panel(event);
}
