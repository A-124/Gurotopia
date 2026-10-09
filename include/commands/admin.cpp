#include "pch.hpp"

#include <charconv>
#include <ctime>
#include <limits>
#include <unordered_map>

#include "admin.hpp"
#include "action/join_request.hpp"
#include "action/quit_to_exit.hpp"
#include "https/server_data.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/SetBux.hpp"
#include "tools/create_dialog.hpp"

namespace
{
    constexpr std::size_t max_players = 40;
    constexpr std::size_t page_size = 10; // @note keeps the dialog small; oversized dialogs crash the client

    constexpr short wl_id = 242;

    struct AdminUiState
    {
        std::string search;
        std::size_t page{};
    };

    // Keep panel navigation separate for each developer using /admin.
    std::unordered_map<int, AdminUiState> admin_ui;

    bool parse_positive_int(const std::string &text, int maximum, int &value)
    {
        if (text.empty()) return false;
        const char *begin = text.data();
        const char *end = begin + text.size();
        const auto [parsed_end, error] = std::from_chars(begin, end, value);
        return error == std::errc{} && parsed_end == end && value > 0 && value <= maximum;
    }

    std::pair<ENetPeer*, ::peer*> find_online_player(int user_id)
    {
        for (ENetPeer *connection : peers())
        {
            if (!connection || !connection->data) continue;
            auto *target = static_cast<::peer*>(connection->data);
            if (target->user_id == user_id) return { connection, target };
        }
        return {};
    }

    void admin_error(ENetEvent &event, std::string_view message, int selected_target_uid = 0)
    {
        if (!event.peer) return; // @note target left mid-action; nothing to redraw
        on::ConsoleMessage(event.peer, std::string(message));
        show_admin_panel(event, selected_target_uid);
    }

    /* offline-capable moderation: banned/muted players can't be online,
       so these hit the DB directly by uid instead of requiring a live peer. */
    bool db_peer_exists(int uid)
    {
        ::hStmt stmt{"SELECT 1 FROM peer WHERE uid = ? LIMIT 1"};
        MYSQL_BIND param = make_bind_in(uid);
        stmt.bind_param(&param);
        stmt.execute();
        return (!mysql_stmt_store_result(stmt.pStmt) && mysql_stmt_num_rows(stmt.pStmt) > 0);
    }

    std::string db_peer_growid(int uid)
    {
        ::hStmt stmt{"SELECT growid FROM peer WHERE uid = ? LIMIT 1"};
        MYSQL_BIND param = make_bind_in(uid);
        stmt.bind_param(&param);
        stmt.execute();
        std::string growid;
        u_long length = 0;
        MYSQL_BIND result = make_bind_out(growid);
        result.length = &length;
        mysql_stmt_bind_result(stmt.pStmt, &result);
        stmt.execute();
        stmt.fetch();
        growid.resize(length);
        return growid;
    }

    void db_set_banned(int uid, bool ban)
    {
        const signed value = ban ? 1 : 0;
        ::hStmt stmt{"UPDATE peer SET banned = ? WHERE uid = ?"};
        MYSQL_BIND params[2] = { make_bind_in(value), make_bind_in(uid) };
        stmt.bind_param(params);
        stmt.execute();
    }

    void db_set_muted_until(int uid, unsigned until)
    {
        ::hStmt stmt{"UPDATE peer SET muted_until = ? WHERE uid = ?"};
        MYSQL_BIND params[2] = { make_bind_in(until), make_bind_in(uid) };
        stmt.bind_param(params);
        stmt.execute();
    }

    /* forward declarations — definitions live after admin_panel_return */
    void admin_action_give_wl(ENetEvent &event, ENetPeer *target_connection, ::peer *target, int target_uid, const ::hPipe &hPipe);
    void admin_action_set_level(ENetEvent &event, ::peer *target, int target_uid, const ::hPipe &hPipe);
    void admin_action_kick(ENetEvent &event, ENetPeer *target_connection, ::peer *target, int target_uid);
    void admin_action_ban(ENetEvent &event, ENetPeer *target_connection, ::peer *target, int target_uid, bool ban);
    void admin_action_mute(ENetEvent &event, ::peer *target, int target_uid, const ::hPipe &hPipe, bool mute);
    void admin_action_teleport(ENetEvent &event, ::peer *admin, ENetPeer *target_connection, ::peer *target, int target_uid, bool pull);
}

void admin(ENetEvent& event, const std::string_view text)
{
    show_admin_panel(event);
}

void show_admin_panel(ENetEvent& event, int selected_target_uid)
{
    if (!event.peer) return; // @note quit/disconnect path can carry a null peer
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer || pPeer->role != DEVELOPER)
    {
        on::ConsoleMessage(event.peer, "`4You do not have permission to use the admin panel.``");
        return;
    }

    AdminUiState &ui = admin_ui[pPeer->user_id];
    const std::vector<ENetPeer*> online = peers();
    create_dialog dialog;
    dialog.set_bg_color(13, 18, 28, 246)
        .set_border_color(56, 189, 248, 255)
        .set_default_color("`w")
        .add_label_with_icon("big", "`2Gurotopia`` `wAdmin Panel", 7190)
        .add_smalltext("`oDEVELOPER CONTROL CENTER``  `w·``  Manage players and server tools.")
        .add_textbox(std::format("`2● ONLINE``  `w{} connected``  `o|  Maintenance: {}``",
            online.size(), gServer_data.maintenance ? "`4ON``" : "`2OFF``"))
        .add_spacer("small")
        .add_label_with_icon("medium", "`cTARGET PLAYER``", 1280)
        .add_text_input("target_uid", "Player UID", selected_target_uid > 0 ? std::to_string(selected_target_uid) : "", 10)
        .add_text_input("player_search", "Search name...", ui.search, 18)
        .add_button("search_player", "`wSearch / Filter``")
        .add_spacer("small")
        .add_label_with_icon("medium", "`cMODERATION``", 1016)
        .add_button("kick_player", "`4Kick Player``")
        .add_button("ban_player", "`4Ban Player``")
        .add_button("unban_player", "`2Unban Player``")
        .add_text_input("mute_minutes", "Mute minutes", "", 5)
        .add_button("mute_player", "`4Mute Player``")
        .add_button("unmute_player", "`2Unmute Player``")
        .add_spacer("small")
        .add_label_with_icon("medium", "`cTELEPORT``", 32)
        .add_button("pull_player", "`wPull to Me``")
        .add_button("goto_player", "`wGo to Player``")
        .add_spacer("small")
        .add_label_with_icon("medium", "`cITEMS & CURRENCY``", 242)
        .add_text_input("item_id", "Item ID", "", 7)
        .add_text_input("item_count", "Amount (1-200)", "1", 3)
        .add_button("give_item", "`2Give Item to Player``")
        .add_button("give_wl", "`2Give World Locks``")
        .add_spacer("small")
        .add_smalltext("`oGems``")
        .add_text_input("gem_amount", "Gem amount", "", 10)
        .add_button("add_gems", "`2Add Gems to Player``")
        .add_spacer("small")
        .add_label_with_icon("medium", "`cPROGRESSION``", 1486)
        .add_text_input("set_level", "Level 1-125", "", 3)
        .add_button("set_level_btn", "`2Set Player Level``")
        .add_spacer("small")
        .add_label_with_icon("medium", "`cSERVER CONTROLS``", 3802)
        .add_button("toggle_maint", gServer_data.maintenance ? "`4Disable Maintenance``" : "`2Enable Maintenance``")
        .add_smalltext(gServer_data.maintenance ? "`4● Maintenance is ON (players blocked)``" : "`2● Maintenance is OFF``")
        .add_spacer("small");

    if (selected_target_uid > 0)
    {
        const auto [selected_connection, selected_player] = find_online_player(selected_target_uid);
        if (selected_connection && selected_player)
        {
            const std::string role_name = selected_player->role == DEVELOPER ? "Developer" :
                selected_player->role == MODERATOR ? "Moderator" : "Player";
            const std::string world_name = selected_player->recent_worlds.back();
            dialog.add_spacer("small")
                .add_label_with_icon("medium", "`2SELECTED PLAYER``", 1280)
                .add_textbox(std::format("`w{}``  `o(UID {})``  `2● {}``",
                    selected_player->growid, selected_target_uid, role_name))
                .add_smalltext(std::format("`oLevel:`` `w{}``   `oGems:`` `w{}``   `oBackpack:`` `w{}/{}``",
                    selected_player->level[0], selected_player->gems,
                    selected_player->slots.size(), std::max(0, selected_player->slot_size)))
                .add_smalltext(world_name.empty()
                    ? "`oCurrent world:`` `4Not in a world``"
                    : std::format("`oCurrent world:`` `w{}``", world_name));
        }
        else
            dialog.add_smalltext(std::format("`4● UID {}``  `o(player offline — only offline-supported actions work)``", selected_target_uid));
    }

    dialog.add_spacer("small")
        .add_label_with_icon("medium", "`cONLINE PLAYERS``", 1280)
        .add_smalltext("`oTap a name to select that player.``")
        .add_button("refresh_admin", "`wRefresh Online List``")
        .add_button("page_prev", "`w< Prev``")
        .add_button("page_next", "`wNext >``");

    std::size_t shown{};
    std::size_t skipped{};
    std::string needle = ui.search;
    for (char &c : needle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    // @note collect matches first so paging is stable even with a search filter.
    std::vector<std::pair<int, std::string>> matches;
    matches.reserve(page_size);
    std::size_t total{};
    for (ENetPeer *connection : online)
    {
        if (!connection || !connection->data) continue;
        const ::peer *target = static_cast<const ::peer*>(connection->data);
        if (!needle.empty())
        {
            std::string hay = target->growid;
            for (char &c : hay) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (hay.find(needle) == std::string::npos && std::to_string(target->user_id).find(needle) == std::string::npos)
                continue;
        }
        ++total;
        if (total > max_players) break; // @note hard cap: oversized dialogs crash the client
        if (skipped < ui.page * page_size) { ++skipped; continue; }
        if (matches.size() >= page_size) continue;
        std::string_view badge = "`o●``";
        switch (target->role)
        {
            case PLAYER: badge = "`o●``"; break;
            case MODERATOR: badge = "`b●``"; break;
            case DEVELOPER: badge = "`2●``"; break;
            default: break;
        }

        std::string name = target->growid;
        if (name.size() > 16) name.resize(16); // @note keep button text bounded
        for (char &character : name)
            if (character == '|' || character == '\n' || character == '\r' || character == '`')
                character = '_';

        if (target->role == MODERATOR)
            name = std::format("`b{}``", name);
        else if (target->role == DEVELOPER)
            name = std::format("`2{}``", name);

        matches.emplace_back(target->user_id,
            std::format("{} `w{}``  `o{}``{}", badge, name, target->user_id,
                target->user_id == selected_target_uid ? " `2✓``" : ""));
        ++shown;
    }

    for (auto &[uid, label] : matches)
        dialog.add_button(std::format("select_target_{}", uid), label);

    const std::size_t pages = (total + page_size - 1) / page_size;
    if (ui.page >= pages && pages > 0)
    {
        // @note list shrank (player left) while admin sat on a later page.
        ui.page = pages - 1;
        show_admin_panel(event, selected_target_uid);
        return;
    }
    if (shown == 0 && total == 0) dialog.add_label("small", "`oNo online player data is available.``");
    if (pages > 1) dialog.add_smalltext(std::format("`oPage {} of {} ({} players)``", ui.page + 1, pages, total));
    else if (total > shown) dialog.add_smalltext(std::format("Showing {} of {} connected players.", shown, total));

    dialog.add_quick_exit();
    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("admin_panel", "Close", "Close") });
}

void admin_panel_return(ENetEvent& event, const ::hPipe &hPipe)
{
    if (!event.peer) return;
    ::peer *pAdmin = static_cast<::peer*>(event.peer->data);
    if (!pAdmin || pAdmin->role != DEVELOPER)
    {
        if (event.peer) on::ConsoleMessage(event.peer, "`4You do not have permission to use the admin panel.``");
        return;
    }

    AdminUiState &ui = admin_ui[pAdmin->user_id];
    const std::string action = hPipe["buttonClicked"];
    if (action == "refresh_admin")
    {
        int selected_uid{};
        const std::string uid_text = hPipe["target_uid"];
        ui.search = hPipe["player_search"];
        show_admin_panel(event, parse_positive_int(uid_text, std::numeric_limits<int>::max(), selected_uid) ? selected_uid : 0);
        return;
    }
    if (action == "search_player")
    {
        int selected_uid{};
        const std::string uid_text = hPipe["target_uid"];
        ui.search = hPipe["player_search"];
        ui.page = 0; // New filters should always start on the first page.
        show_admin_panel(event, parse_positive_int(uid_text, std::numeric_limits<int>::max(), selected_uid) ? selected_uid : 0);
        return;
    }
    if (action == "toggle_maint")
    {
        gServer_data.maintenance = !gServer_data.maintenance; // @note single source of truth, no shadow copy
        gServer_data.save_maintenance();
        // @note NO dialog redraw here: the redraw after OFF is what crashes when
        // the player list is large. Just confirm + tell admin to reopen /admin.
        on::ConsoleMessage(event.peer, gServer_data.maintenance ?
            "`4Maintenance mode ON — regular players can no longer log in. Reopen /admin to see the panel.``" :
            "`2Maintenance mode OFF — players can log in again. Reopen /admin to see the panel.``");
        return;
    }
    if (action == "page_prev" || action == "page_next")
    {
        int selected_uid{};
        const std::string uid_text = hPipe["target_uid"];
        if (parse_positive_int(uid_text, std::numeric_limits<int>::max(), selected_uid)) { /* keep */ }
        ui.search = hPipe["player_search"];
        if (action == "page_prev") ui.page = (ui.page == 0) ? 0 : ui.page - 1;
        else ui.page = std::min(ui.page + 1, (max_players - 1) / page_size);
        show_admin_panel(event, selected_uid);
        return;
    }
    constexpr std::string_view select_prefix = "select_target_";
    if (action.starts_with(select_prefix))
    {
        int selected_uid{};
        const std::string_view uid_text(action.data() + select_prefix.size(), action.size() - select_prefix.size());
        const auto [parsed_end, error] = std::from_chars(uid_text.data(), uid_text.data() + uid_text.size(), selected_uid);
        if (error == std::errc{} && parsed_end == uid_text.data() + uid_text.size() && selected_uid > 0 && find_online_player(selected_uid).first)
        {
            show_admin_panel(event, selected_uid);
            return;
        }
        admin_error(event, "`4That player is no longer online.``");
        return;
    }
    if (action != "give_item" && action != "give_wl" && action != "add_gems" && action != "set_level_btn" &&
        action != "kick_player" && action != "ban_player" && action != "unban_player" &&
        action != "mute_player" && action != "unmute_player" &&
        action != "pull_player" && action != "goto_player") return;

    int target_uid{};
    if (!parse_positive_int(hPipe["target_uid"], std::numeric_limits<int>::max(), target_uid))
    {
        admin_error(event, "`4Enter a valid target UID.``");
        return;
    }

    // @note ban/unban/mute/unmute work offline via direct DB update,
    // since a banned player can never appear in the online list.
    if (action == "ban_player" || action == "unban_player" ||
        action == "mute_player" || action == "unmute_player")
    {
        const auto [online_conn, online_peer] = find_online_player(target_uid);
        if (action == "ban_player") { admin_action_ban(event, online_conn, online_peer, target_uid, true); return; }
        if (action == "unban_player") { admin_action_ban(event, online_conn, online_peer, target_uid, false); return; }
        if (action == "mute_player") { admin_action_mute(event, online_peer, target_uid, hPipe, true); return; }
        if (action == "unmute_player") { admin_action_mute(event, online_peer, target_uid, hPipe, false); return; }
    }

    const auto [target_connection, target] = find_online_player(target_uid);
    if (!target_connection || !target)
    {
        admin_error(event, "`4That player is not online.``", target_uid);
        return;
    }
    if (target == pAdmin && action != "pull_player" && action != "goto_player")
    {
        admin_error(event, "`4You cannot use moderation actions on yourself.``", target_uid);
        return;
    }

    if (action == "give_item")
    {
        if (items.empty())
        {
            admin_error(event, "`4The item database is not loaded.``", target_uid);
            return;
        }

        const int max_item_id = static_cast<int>(std::min<std::size_t>(
            items.size() - 1, static_cast<std::size_t>(std::numeric_limits<short>::max())));
        int item_id{};
        int item_count{};
        if (!parse_positive_int(hPipe["item_id"], max_item_id, item_id) ||
            !parse_positive_int(hPipe["item_count"], 200, item_count))
        {
            admin_error(event, "`4Enter a valid item ID and amount from 1 to 200.``", target_uid);
            return;
        }

        const ::item &item = id_to_item(static_cast<u_short>(item_id));
        if (item.id != item_id)
        {
            admin_error(event, "`4That item ID does not exist.``", target_uid);
            return;
        }

        const auto existing = std::ranges::find(target->slots, static_cast<short>(item_id), &::slot::id);
        if (existing == target->slots.end() && target->slots.size() >= static_cast<std::size_t>(std::max(0, target->slot_size)))
        {
            admin_error(event, "`4That player's backpack has no free slots.``", target_uid);
            return;
        }

        const u_short excess = target->emplace(::slot(static_cast<short>(item_id), static_cast<short>(item_count)));
        const int granted = item_count - excess;
        if (granted <= 0)
        {
            admin_error(event, "`4That player's item stack is already full.``", target_uid);
            return;
        }

        target->save_inventory();
        send_inventory_state(*target_connection);
        on::ConsoleMessage(target_connection, std::format("`2You received {} {}``.", granted, item.raw_name));
        on::ConsoleMessage(event.peer, std::format("`2Gave {} {} to {} (UID {}).``", granted, item.raw_name, target->growid, target->user_id));
        show_admin_panel(event, target_uid);
        return;
    }

    int gem_amount{};
    if (action == "add_gems")
    {
        if (!parse_positive_int(hPipe["gem_amount"], std::numeric_limits<int>::max(), gem_amount))
        {
            admin_error(event, "`4Enter a positive gem amount.``", target_uid);
            return;
        }

        target->gems = std::clamp(target->gems, 0, std::numeric_limits<signed>::max());
        if (gem_amount > std::numeric_limits<signed>::max() - target->gems)
        {
            admin_error(event, "`4That would exceed the maximum gem balance.``", target_uid);
            return;
        }

        target->gems += gem_amount;
        on::SetBux(*target_connection);
        on::ConsoleMessage(target_connection, std::format("`2You received {} gems.``", gem_amount));
        on::ConsoleMessage(event.peer, std::format("`2Added {} gems to {} (UID {}).``", gem_amount, target->growid, target->user_id));
        show_admin_panel(event, target_uid);
        return;
    }

    if (action == "give_wl") { admin_action_give_wl(event, target_connection, target, target_uid, hPipe); return; }
    if (action == "set_level_btn") { admin_action_set_level(event, target, target_uid, hPipe); return; }
    if (action == "kick_player") { admin_action_kick(event, target_connection, target, target_uid); return; }
    if (action == "ban_player") { admin_action_ban(event, target_connection, target, target_uid, true); return; }
    if (action == "unban_player") { admin_action_ban(event, target_connection, target, target_uid, false); return; }
    if (action == "mute_player") { admin_action_mute(event, target, target_uid, hPipe, true); return; }
    if (action == "unmute_player") { admin_action_mute(event, target, target_uid, hPipe, false); return; }
    if (action == "pull_player") { admin_action_teleport(event, pAdmin, target_connection, target, target_uid, true); return; }
    if (action == "goto_player") { admin_action_teleport(event, pAdmin, target_connection, target, target_uid, false); return; }
}

namespace
{

void admin_action_give_wl(ENetEvent &event, ENetPeer *target_connection, ::peer *target, int target_uid, const ::hPipe &hPipe)
{
    int wl_count{};
    if (!parse_positive_int(hPipe["item_count"], 20000, wl_count))
    {
        admin_error(event, "`4Enter a World Lock amount from 1 to 20000.``", target_uid);
        return;
    }
    int remaining = wl_count;
    // @note emplace() caps stacks at 200; loop so big grants land across stacks.
    while (remaining > 0)
    {
        const int chunk = std::min(remaining, 200);
        const auto existing = std::ranges::find(target->slots, wl_id, &::slot::id);
        if (existing == target->slots.end() && target->slots.size() >= static_cast<std::size_t>(std::max(0, target->slot_size)))
        {
            admin_error(event, "`4That player's backpack has no free slots.``", target_uid);
            return;
        }
        const u_short excess = target->emplace(::slot(wl_id, static_cast<short>(chunk)));
        remaining -= (chunk - excess);
        if (excess > 0) break; // @note stack full and no room to split further
    }
    const int granted = wl_count - remaining;
    if (granted <= 0)
    {
        admin_error(event, "`4That player's World Lock stacks are full.``", target_uid);
        return;
    }
    target->save_inventory();
    send_inventory_state(*target_connection);
    on::ConsoleMessage(target_connection, std::format("`2You received {} World Lock{}.``", granted, granted == 1 ? "" : "s"));
    on::ConsoleMessage(event.peer, std::format("`2Gave {} World Lock{} to {} (UID {}).``", granted, granted == 1 ? "" : "s", target->growid, target_uid));
    show_admin_panel(event, target_uid);
}

void admin_action_set_level(ENetEvent &event, ::peer *target, int target_uid, const ::hPipe &hPipe)
{
    int level{};
    if (!parse_positive_int(hPipe["set_level"], 125, level))
    {
        admin_error(event, "`4Enter a level from 1 to 125.``", target_uid);
        return;
    }
    target->level[0] = static_cast<u_int>(level);
    target->level[1] = 0;
    target->save_progress();
    on::ConsoleMessage(event.peer, std::format("`2Set {} (UID {}) to level {}.``", target->growid, target_uid, level));
    show_admin_panel(event, target_uid);
}

void admin_action_kick(ENetEvent &event, ENetPeer *target_connection, ::peer *target, int target_uid)
{
    on::ConsoleMessage(target_connection, "`4You were kicked by an admin.``");
    ENetEvent fake{.peer = target_connection};
    action::quit_to_exit(fake, "", true);
    enet_peer_disconnect(target_connection, 0);
    on::ConsoleMessage(event.peer, std::format("`2Kicked {} (UID {}).``", target->growid, target_uid));
    show_admin_panel(event, target_uid);
}

void admin_action_ban(ENetEvent &event, ENetPeer *target_connection, ::peer *target, int target_uid, bool ban)
{
    if (!target || !target_connection)
    {
        // @note offline path: banned players can't be online, so update DB directly.
        if (!db_peer_exists(target_uid))
        {
            admin_error(event, std::format("`4UID {} does not exist.``", target_uid), target_uid);
            return;
        }
        db_set_banned(target_uid, ban);
        const std::string name = db_peer_growid(target_uid);
        on::ConsoleMessage(event.peer, ban ?
            std::format("`2Banned {} (UID {}) — offline, takes effect on next login.``", name.empty() ? "player" : name, target_uid) :
            std::format("`2Unbanned {} (UID {}). They can log in again.``", name.empty() ? "player" : name, target_uid));
        show_admin_panel(event, target_uid);
        return;
    }
    target->banned = ban;
    target->save_moderation();
    if (ban)
    {
        on::ConsoleMessage(target_connection, "`4You were banned by an admin.``");
        ENetEvent fake{.peer = target_connection};
        action::quit_to_exit(fake, "", true);
        enet_peer_disconnect(target_connection, 0);
        on::ConsoleMessage(event.peer, std::format("`2Banned {} (UID {}).``", target->growid, target_uid));
    }
    else on::ConsoleMessage(event.peer, std::format("`2Unbanned {} (UID {}). They can log in again.``", target->growid, target_uid));
    show_admin_panel(event, target_uid);
}

void admin_action_mute(ENetEvent &event, ::peer *target, int target_uid, const ::hPipe &hPipe, bool mute)
{
    std::string disp_name = target ? target->growid : db_peer_growid(target_uid);
    if (disp_name.empty()) disp_name = "player";
    if (!mute)
    {
        if (target) { target->muted_until = 0; target->save_moderation(); }
        else
        {
            // @note offline path
            if (!db_peer_exists(target_uid))
            {
                admin_error(event, std::format("`4UID {} does not exist.``", target_uid), target_uid);
                return;
            }
            db_set_muted_until(target_uid, 0);
        }
        on::ConsoleMessage(event.peer, std::format("`2Unmuted {} (UID {}).``", disp_name, target_uid));
        show_admin_panel(event, target_uid);
        return;
    }
    int minutes{};
    if (!parse_positive_int(hPipe["mute_minutes"], 60 * 24 * 30, minutes))
    {
        admin_error(event, "`4Enter mute minutes from 1 to 43200 (30 days).``", target_uid);
        return;
    }
    const unsigned until = static_cast<u_int>(std::time(nullptr)) + static_cast<u_int>(minutes) * 60u;
    if (target) { target->muted_until = until; target->save_moderation(); }
    else
    {
        if (!db_peer_exists(target_uid))
        {
            admin_error(event, std::format("`4UID {} does not exist.``", target_uid), target_uid);
            return;
        }
        db_set_muted_until(target_uid, until);
    }
    on::ConsoleMessage(event.peer, std::format("`2Muted {} (UID {}) for {} minute{}.``", disp_name, target_uid, minutes, minutes == 1 ? "" : "s"));
    show_admin_panel(event, target_uid);
}

void admin_action_teleport(ENetEvent &event, ::peer *admin, ENetPeer *target_connection, ::peer *target, int target_uid, bool pull)
{
    if (pull)
    {
        // @note bring target to admin's world + position
        const std::string &world_name = admin->recent_worlds.back();
        if (world_name.empty())
        {
            admin_error(event, "`4Enter a world before pulling players.``", target_uid);
            return;
        }
        ENetEvent fake{.peer = target_connection};
        action::quit_to_exit(fake, "", true);
        action::join_request(fake, "", world_name);
        target->pos = admin->pos;
        send_varlist(target_connection, {"OnSetPos", CL_Vec2f{target->pos.x, target->pos.y}}, target->netid);
        on::ConsoleMessage(event.peer, std::format("`2Pulled {} (UID {}) to you.``", target->growid, target_uid));
    }
    else
    {
        // @note take admin to target's world + position
        const std::string &world_name = target->recent_worlds.back();
        if (world_name.empty())
        {
            admin_error(event, "`4That player is not inside a world.``", target_uid);
            return;
        }
        action::quit_to_exit(event, "", true);
        action::join_request(event, "", world_name);
        admin->pos = target->pos;
        send_varlist(event.peer, {"OnSetPos", CL_Vec2f{admin->pos.x, admin->pos.y}}, admin->netid);
        on::ConsoleMessage(event.peer, std::format("`2Warped to {} (UID {}) in {}.``", target->growid, target_uid, world_name));
    }
    show_admin_panel(event, target_uid);
}

} // namespace
