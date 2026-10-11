#include "pch.hpp"
#include <chrono>
#include <unordered_map>
#include <fstream>
#include <charconv>
#include <filesystem>
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
        d += "add_textbox|Register an item and scan online player inventories. Offline/world economy totals are not available in this native server yet.|left|\n";
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
        d += "add_quick_exit|\nend_dialog|scanitem_main||Close|\n";
        send_varlist(event.peer, {"OnDialogRequest", std::move(d)});
    }
    void show_scanitem_results(ENetEvent& event, int id)
    {
        auto* self = self_of(event);
        if (!self) return;
        std::vector<std::pair<std::string, int>> found;
        long long total = 0;
        for (ENetPeer* candidate : peers()) {
            auto* target = candidate && candidate->data ? static_cast<::peer*>(candidate->data) : nullptr;
            if (!target || target->growid.empty()) continue;
            const int amount = item_count(*target, id);
            if (amount <= 0) continue;
            found.emplace_back(target->growid, amount);
            total += amount;
        }
        std::ranges::sort(found, [](const auto& a, const auto& b) { return a.second > b.second; });
        std::string d = std::format("add_label_with_icon|big|Scan Results: {}|left|{}|\n", scan_item_name(id), id);
        d += std::format("add_textbox|{} online holders; {} items total.|left|\n", found.size(), total);
        if (found.empty()) d += "add_textbox|No online players currently hold this item.|left|\n";
        for (std::size_t i = 0; i < std::min<std::size_t>(found.size(), 50); ++i)
            d += std::format("add_textbox|{}: {}|left|\n", ui::sanitize(found[i].first, 48), found[i].second);
        if (found.size() > 50) d += std::format("add_smalltext|{} more holder(s) omitted.|left|\n", found.size() - 50);
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
    if (button == "scanitem_save") {
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
