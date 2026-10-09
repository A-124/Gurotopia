#include "pch.hpp"
#include <ctime>
#include <algorithm>
#include "content_commands.hpp"
#include "database/custom_content.hpp"
#include "tools/create_dialog.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "core/runtime_reload.hpp"
#include "gameplay/quest_system.hpp"
#include "gameplay/achievement_system.hpp"
#include "gameplay/goals.hpp"
#include "database/peer.hpp"
#include "gameplay/craft.hpp"

namespace {
void open_dialog(ENetEvent& event, std::string body) {
    if (event.peer) send_varlist(event.peer, {"OnDialogRequest", std::move(body)});
}
std::string header(std::string title, int icon, std::string subtitle) {
    return std::format("set_bg_color|15,50,75,235|\nset_border_color|75,205,230,255|\nadd_label_with_icon|big|{}|left|{}|\nadd_smalltext|{}|left|\nadd_spacer|small|\n", title, icon, subtitle);
}
void hub(ENetEvent& event) {
    auto *p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    std::string d = header("Gurotopia Hub", 5814, "Quests, achievements, crafting and content tools");
    d += "add_textbox|Choose a section. Each feature has a dedicated panel with clear progress and actions.|left|\nadd_spacer|small|\n";
    d += "add_button|hub_quests|Quests - track objectives|noflags|0|0|\n";
    d += "add_button|hub_achievements|Achievements - long-term milestones|noflags|0|0|\n";
    d += "add_button|hub_craft|Crafting Workshop - recipes and quantities|noflags|0|0|\n";
    if (p->role == DEVELOPER) d += "add_button|hub_content|Content Manager - validate and reload|noflags|0|0|\n";
    d += std::format("add_spacer|small|\nadd_smalltext|Quests: {}   Achievements: {}   Recipes: {}|left|\nend_dialog|gurotopia_hub|Close|Open|\nadd_quick_exit|\n", quest_system::all().size(), achievement_system::all().size(), custom_content::recipe_count());
    open_dialog(event, std::move(d));
}
void goals_dialog(ENetEvent& event, bool achievement) {
    auto *p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p) return;
    const auto &defs = achievement ? achievement_system::all() : quest_system::all();
    const auto &progress = achievement ? p->achievement_progress : p->quest_progress;
    const auto &done = achievement ? p->achievements_done : p->quests_done;
    std::vector<const goals::goal*> sorted;
    for (const auto &[id, g] : defs) sorted.push_back(&g);
    std::ranges::sort(sorted, {}, &goals::goal::id);
    const std::string title = achievement ? "Achievements" : "Quests";
    std::string d = header(title, achievement ? 1436 : 6016, std::format("{} complete out of {}", done.size(), defs.size()));
    d += "add_button|hub_back|Back to Gurotopia Hub|noflags|0|0|\nadd_spacer|small|\n";
    if (sorted.empty()) d += "add_textbox|No entries configured yet.|left|\n";
    for (const auto *g : sorted) {
        const int current = done.contains(g->id) ? g->required : (progress.contains(g->id) ? progress.at(g->id) : 0);
        d += std::format("add_label|small|{}  {}|left|\n", done.contains(g->id) ? "[COMPLETE]" : "[IN PROGRESS]", g->name);
        if (!g->description.empty()) d += std::format("add_smalltext|{}|left|\n", g->description);
        d += std::format("add_smalltext|Progress: {}/{}     Reward: {}|left|\nadd_spacer|small|\n", current, g->required, goals::describe(g->prize));
    }
    d += std::format("end_dialog|{}||Close|\nadd_quick_exit|\n", achievement ? "gurotopia_achievements" : "gurotopia_quests");
    open_dialog(event, std::move(d));
}
void craft_dialog(ENetEvent& event, int item_id = 0, int amount = 1) {
    std::string d = header("Crafting Workshop", 32, "Enter a recipe output ID and quantity. Ingredients and backpack space are checked before crafting.");
    d += std::format("add_text_input|craft_item|Recipe output item ID:|{}|6|\nadd_text_input|craft_amount|Quantity (1-100):|{}|3|\nadd_spacer|small|\n", item_id, amount);
    std::vector<int> ids;
    for (const auto &[id, item] : custom_content::items()) if (custom_content::find_recipe(id)) ids.push_back(id);
    std::ranges::sort(ids);
    for (int id : ids) {
        const auto *recipe = custom_content::find_recipe(id);
        const auto *item = custom_content::find_item(id);
        d += std::format("add_label_with_icon|small|{} (ID {})|left|{}|\n", item ? item->name : std::format("Item {}", id), id, id);
        std::string ingredients = "Ingredients: ";
        for (std::size_t i = 0; i < recipe->ingredients.size(); ++i) {
            if (i) ingredients += ", ";
            ingredients += std::format("{} x{}", id_to_item(static_cast<u_short>(recipe->ingredients[i].first)).raw_name, recipe->ingredients[i].second);
        }
        d += std::format("add_smalltext|{}|left|\nadd_spacer|small|\n", ingredients);
    }
    if (ids.empty()) d += "add_textbox|No custom recipes configured. A developer can add them in resources/custom_items.txt.|left|\n";
    d += "add_button|craft_submit|Craft Selected Recipe|noflags|0|0|\nadd_button|hub_back|Back to Gurotopia Hub|noflags|0|0|\nend_dialog|gurotopia_craft|Cancel|Craft|\nadd_quick_exit|\n";
    open_dialog(event, std::move(d));
}
void content_dialog(ENetEvent& event, std::string notice = {}) {
    auto *p = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!p || p->role != DEVELOPER) { if (event.peer) send_varlist(event.peer, {"OnConsoleMessage", "Only developers can use Content Manager."}); return; }
    std::string d = header("Content Manager", 32, "Developer tools for custom items, recipes, quests and achievements");
    if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);
    d += std::format("add_textbox|Custom items: {}   Recipes: {}|left|\nadd_textbox|Quests: {}   Achievements: {}|left|\nadd_spacer|small|\n", custom_content::items().size(), custom_content::recipe_count(), quest_system::all().size(), achievement_system::all().size());
    d += "add_button|content_validate|Validate Configuration|noflags|0|0|\nadd_button|content_reload|Reload All Content|noflags|0|0|\nadd_smalltext|Validation checks custom_items.txt without changing active content. Reload applies valid content and refreshes quest and achievement definitions.|left|\nadd_button|hub_back|Back to Gurotopia Hub|noflags|0|0|\nend_dialog|gurotopia_content|Close|Select|\nadd_quick_exit|\n";
    open_dialog(event, std::move(d));
}
}

void content_status(ENetEvent& event, const std::string_view text) {
    if (!event.peer) return;
    if (text.find("validate") != std::string_view::npos) {
        const auto errors = custom_content::validate();
        if (errors.empty()) { send_varlist(event.peer, {"OnConsoleMessage", "[CONTENT] Validation passed. No custom item or recipe errors found."}); return; }
        send_varlist(event.peer, {"OnConsoleMessage", std::format("[CONTENT] Validation failed with {} error(s).", errors.size())});
        constexpr std::size_t limit = 8;
        for (std::size_t i = 0; i < std::min(errors.size(), limit); ++i) send_varlist(event.peer, {"OnConsoleMessage", std::format("[CONTENT] {}", errors[i])});
        if (errors.size() > limit) send_varlist(event.peer, {"OnConsoleMessage", std::format("[CONTENT] {} additional error(s) omitted.", errors.size() - limit)});
        return;
    }
    if (text.find("hub") != std::string_view::npos || text.find("features") != std::string_view::npos) { hub(event); return; }
    if (text.find("craft") != std::string_view::npos) { craft_dialog(event); return; }
    if (text.find("quests") != std::string_view::npos) { goals_dialog(event, false); return; }
    if (text.find("achievements") != std::string_view::npos) { goals_dialog(event, true); return; }
    auto *p = static_cast<::peer*>(event.peer->data);
    if (p && p->role == DEVELOPER) content_dialog(event);
    else send_varlist(event.peer, {"OnConsoleMessage", std::format("[CONTENT] custom_items={} recipes={} quests={} achievements={} | use /content validate or /features", custom_content::items().size(), custom_content::recipe_count(), quest_system::all().size(), achievement_system::all().size())});
}
void quests_command(ENetEvent& event, const std::string_view) { goals_dialog(event, false); }
void achievements_command(ENetEvent& event, const std::string_view) { goals_dialog(event, true); }
void features_command(ENetEvent& event, const std::string_view) { hub(event); }
void craft_dialog_command(ENetEvent& event, const std::string_view) { craft_dialog(event); }
void content_dialog_command(ENetEvent& event, const std::string_view) { content_dialog(event); }

void handle_content_dialog_return(ENetEvent& event, const ::hPipe& pipe) {
    const std::string button = pipe["buttonClicked"];
    if (button == "hub_quests") { goals_dialog(event, false); return; }
    if (button == "hub_achievements") { goals_dialog(event, true); return; }
    if (button == "hub_craft") { craft_dialog(event); return; }
    if (button == "hub_content") { content_dialog(event); return; }
    if (button == "hub_back") { hub(event); return; }
    if (button == "content_validate") {
        const auto errors = custom_content::validate();
        content_dialog(event, errors.empty() ? "Validation passed - no configuration errors." : std::format("Validation found {} error(s). Use /content validate for line details.", errors.size()));
        return;
    }
    if (button == "content_reload") { content_dialog(event, std::string(runtime_reload::result_message(runtime_reload::reload("content")))); return; }
    if (button == "craft_submit") {
        const int id = std::atoi(pipe["craft_item"].c_str());
        const int amount = std::atoi(pipe["craft_amount"].c_str());
        craft(event, std::format("/craft {} {}", id, amount));
        craft_dialog(event, id, amount);
    }
}

void daily_command(ENetEvent& event, const std::string_view)
{
    ::peer *pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
    if (!pPeer) return;

    constexpr u_int cooldown = 20u * 3600u;   // @note claimable roughly once a day
    constexpr u_int streak_window = 48u * 3600u; // @note miss two days and the streak restarts
    const u_int now = static_cast<u_int>(std::time(nullptr));

    if (pPeer->last_daily != 0 && now >= pPeer->last_daily && now - pPeer->last_daily < cooldown)
    {
        const u_int left = cooldown - (now - pPeer->last_daily);
        on::ConsoleMessage(event.peer, std::format("`4Daily reward already claimed.`` Come back in `w{}h {}m``. Current streak: `w{}`` day(s).",
            left / 3600u, (left % 3600u) / 60u, pPeer->daily_streak));
        return;
    }

    if (pPeer->last_daily == 0 || now < pPeer->last_daily || now - pPeer->last_daily > streak_window) pPeer->daily_streak = 1;
    else ++pPeer->daily_streak;
    pPeer->last_daily = now;

    const int day = (pPeer->daily_streak - 1) % 7 + 1; // @note 1..7, then the cycle starts again
    goals::reward prize{};
    prize.gems = 1000 * day;
    prize.xp = 250 * day;
    if (day == 7) prize.items.emplace_back(3402/*Golden Booty Chest*/, 1);

    const std::string given = goals::grant(event.peer, prize);
    pPeer->save_goals();

    on::ConsoleMessage(event.peer, std::format("`2Daily reward claimed!`` Day `w{}`` of your streak ({}/7 this week): `2{}``",
        pPeer->daily_streak, day, given));
    if (pPeer->netid != 0)
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, std::format("`2Daily reward! Streak: {}``", pPeer->daily_streak), 0u, 1u });
}
