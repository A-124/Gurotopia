#include "pch.hpp"

#include <charconv>
#include <ctime>

#include "trade.hpp"
#include "tools/create_dialog.hpp"
#include "onVariant/ConsoleMessage.hpp"

extern ENetHost *host;

std::unordered_map<ENetPeer*, std::shared_ptr<trade::session>> trade::sessions;

namespace
{
    // =====================================================================
    //  THEME — dito mo palitan ang itsura ng trade window
    // =====================================================================
    constexpr int BG_COLOR[4]     = {14, 16, 24, 240};  // dark navy   (r, g, b, alpha 0-255)
    constexpr int BORDER_COLOR[4] = {0, 210, 180, 255}; // teal accent (r, g, b, alpha 0-255)
    // Text color codes na ginamit sa baba (palitan sa show() kung gusto mo):
    //   `w white   `o light gray   `c cyan (accent)   `2 green   `9 yellow   `4 red

    // @note true = ipakita rin ang lumang listahan ng "Add xN" buttons sa ilalim ng picker.
    constexpr bool SHOW_ADD_GRID = false;

    // @note ilang segundong walang pindot bago ituring na abandoned ang trade
    constexpr std::time_t TRADE_IDLE_LIMIT = 120;

    ::create_dialog themed()
    {
        return ::create_dialog()
            .set_default_color("`w")
            .set_bg_color(BG_COLOR[0], BG_COLOR[1], BG_COLOR[2], BG_COLOR[3])
            .set_border_color(BORDER_COLOR[0], BORDER_COLOR[1], BORDER_COLOR[2], BORDER_COLOR[3]);
    }

    bool parse_int(const std::string &text, int &value)
    {
        if (text.empty()) return false;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc{} && end == text.data() + text.size();
    }

    ::peer *pp(ENetPeer *p) { return p ? static_cast<::peer*>(p->data) : nullptr; }

    bool same_world(ENetPeer *a, ENetPeer *b)
    {
        const ::peer *pa = pp(a), *pb = pp(b);
        // @note netid == 0 means the player already left to the world menu
        return pa && pb && pa->netid != 0 && pb->netid != 0
            && !pa->recent_worlds.back().empty()
            && pa->recent_worlds.back() == pb->recent_worlds.back();
    }

    void touch(trade::session &s) { s.last_active = std::time(nullptr); }

    /* abandoned / broken trade: someone left the world, disconnected,
       or nobody clicked anything for TRADE_IDLE_LIMIT seconds */
    bool session_dead(const trade::session &s)
    {
        if (!pp(s.a) || !pp(s.b) || !same_world(s.a, s.b)) return true;
        return (std::time(nullptr) - s.last_active) > TRADE_IDLE_LIMIT;
    }

    bool offers_valid(const ::peer &p, const std::array<trade::offer, 4> &offers)
    {
        std::unordered_map<short, int> need;
        for (const auto &o : offers)
        {
            if (o.item <= 0 && o.count <= 0) continue;
            if (o.item <= 0 || o.count <= 0 || o.count > 200) return false;
            if (o.item >= static_cast<int>(items.size()) || id_to_item(static_cast<u_short>(o.item)).id != o.item) return false;
            if (id_to_item(static_cast<u_short>(o.item)).cat & CAT_UNTRADEABLE) return false;
            need[o.item] += o.count;
            if (need[o.item] > 800) return false; // @note 4 slots * 200
        }
        for (const auto &[id, n] : need)
        {
            int have = 0;
            for (const auto &s : p.slots) if (s.id == id) have += s.count;
            if (have < n) return false;
        }
        return true;
    }

    /* can `p` hold `incoming` AFTER it has handed over `outgoing`? (items given away free up slots) */
    bool fits(const ::peer &p, const std::array<trade::offer, 4> &outgoing, const std::array<trade::offer, 4> &incoming)
    {
        std::vector<::slot> sim = p.slots;
        for (const auto &o : outgoing)
        {
            if (o.item <= 0 || o.count <= 0) continue;
            int left = o.count;
            for (auto &sl : sim)
            {
                if (left <= 0) break;
                if (sl.id != o.item) continue;
                const int take = std::min<int>(left, sl.count);
                sl.count = static_cast<short>(sl.count - take);
                left -= take;
            }
        }
        std::erase_if(sim, [](const ::slot &sl) { return sl.count <= 0; });

        std::unordered_map<short, int> need;
        for (const auto &o : incoming)
            if (o.item > 0 && o.count > 0) need[o.item] += o.count;
        int free_slots = p.slot_size - static_cast<int>(sim.size());
        for (const auto &[id, n] : need)
        {
            int room = 0;
            for (const auto &s : sim) if (s.id == id) room += 200 - s.count;
            int left = n - room;
            while (left > 0) { if (--free_slots < 0) return false; left -= 200; }
        }
        return true;
    }

    /* offers are reservations: the backpack is only touched here, at confirm. */
    void take_offer(ENetEvent &event, const std::array<trade::offer, 4> &offers)
    {
        for (const auto &o : offers)
        {
            if (o.item <= 0 || o.count <= 0) continue;
            int left = o.count;
            while (left > 0)
            {
                const short chunk = static_cast<short>(std::min(left, 200));
                modify_item_inventory(event, ::slot(o.item, static_cast<short>(-chunk)));
                left -= chunk;
            }
        }
    }

    void give_offer(ENetEvent &event, const std::array<trade::offer, 4> &offers)
    {
        for (const auto &o : offers)
        {
            if (o.item <= 0 || o.count <= 0) continue;
            int left = o.count;
            while (left > 0)
            {
                const short chunk = static_cast<short>(std::min(left, 200));
                modify_item_inventory(event, ::slot(o.item, chunk));
                left -= chunk;
            }
        }
    }

    /* "5x Dirt, 1x Rock" — text version of an offer, used for instant notices */
    std::string offer_text(const std::array<trade::offer, 4> &offers)
    {
        std::string text;
        for (const auto &o : offers)
        {
            if (o.item <= 0 || o.count <= 0) continue;
            if (!text.empty()) text += ", ";
            text += std::format("{}x {}", o.count, id_to_item(static_cast<u_short>(o.item)).raw_name);
        }
        return text.empty() ? std::string{"nothing"} : text;
    }

    int offer_total(const std::array<trade::offer, 4> &offers)
    {
        int total = 0;
        for (const auto &o : offers) if (o.item > 0 && o.count > 0) total += o.count;
        return total;
    }

    /* "Dirt x200" */
    std::string item_line(const trade::offer &o)
    {
        return std::format("`w{}`` `ox{}``", id_to_item(static_cast<u_short>(o.item)).raw_name, o.count);
    }

    std::string chip(bool accepted)
    {
        return accepted ? "`2[ ACCEPTED ]``" : "`9[ WAITING ]``";
    }

    std::string status_hint(const std::string &partner, bool i_accept, bool they_accept, bool any_items)
    {
        if (they_accept && !i_accept) return std::format("`2{} accepted.`` `oCheck the offer, then tap Accept.``", partner);
        if (i_accept && !they_accept) return std::format("`oWaiting for`` {} `oto accept...``", partner);
        if (!any_items) return "`oTap the picker below to add items, then tap Accept.``";
        return "`oReview both offers, then tap Accept. Both players must accept.``";
    }

    /* @param amount 0 = whole remaining stack (max 200 per offer slot), >0 = that many */
    bool add_to_offer(ENetEvent &event, std::array<trade::offer, 4> &my_off,
        int id, int amount, std::string &err)
    {
        ::peer *me = pp(event.peer);
        if (!me) { err = "`4Trade error.``"; return false; }
        if (id <= 0 || id >= static_cast<int>(items.size()) ||
            id_to_item(static_cast<u_short>(id)).id != id) { err = "`4Unknown item.``"; return false; }
        if (id == 18 || id == 32) { err = "`4That item cannot be traded.``"; return false; } // @note fist + wrench
        const ::item &it = id_to_item(static_cast<u_short>(id));
        if (it.cat & CAT_UNTRADEABLE) { err = "`4That item cannot be traded.``"; return false; }
        int have = 0;
        for (const auto &sl : me->slots) if (sl.id == id) have += sl.count;
        int already = 0;
        for (const auto &o : my_off) if (o.item == id) already += o.count;
        const int can_add = have - already;
        if (can_add <= 0) { err = "`4You don't have any more of that item to offer.``"; return false; }

        int want = std::min(can_add, 200);
        if (amount > 0) want = std::min(want, amount);

        int room = 0;
        for (const auto &o : my_off)
        {
            if (o.item == static_cast<short>(id) && o.count < 200) room += 200 - o.count;
            else if (o.item <= 0) room += 200;
        }
        if (room < want) { err = "`4Offer full (4 slots). Remove one first.``"; return false; }

        int left = want;
        for (auto &o : my_off)
        {
            if (left <= 0) break;
            if (o.item == static_cast<short>(id) && o.count < 200)
            {
                const int add = std::min(200 - o.count, left);
                o.count = static_cast<short>(o.count + add);
                left -= add;
            }
        }
        for (auto &o : my_off)
        {
            if (left <= 0) break;
            if (o.item <= 0)
            {
                const int add = std::min(200, left);
                o.item = static_cast<short>(id);
                o.count = static_cast<short>(add);
                left -= add;
            }
        }
        return true;
    }

    /* Layout (top -> bottom):
       [Refresh]  Trade with X  status hint
       > THEY GIVE [state]   items
       > YOU GIVE  [state]   items (+Remove)
       summary
       > ADD ITEM   inventory picker + amount
       [ACCEPT TRADE]   [Cancel] */
    void show(ENetPeer *viewer, trade::session &s, bool is_a)
    {
        ::peer *me = pp(viewer);
        ENetPeer *other_p = is_a ? s.b : s.a;
        ::peer *other = pp(other_p);
        if (!me || !other) return;
        const auto &my_off = is_a ? s.offers_a : s.offers_b;
        const auto &their_off = is_a ? s.offers_b : s.offers_a;
        const bool i_accept = is_a ? s.accept_a : s.accept_b;
        const bool they_accept = is_a ? s.accept_b : s.accept_a;
        const int give_total = offer_total(my_off);
        const int get_total = offer_total(their_off);

        auto dialog = themed()
            .add_button("trade_refresh", "`cRefresh``") // @note PINAKA-TAAS para madaling i-tap
            .add_spacer("small")
            .add_label_with_icon("big", std::format("`wTrade with `c{}``", other->display_growid), 242)
            .add_smalltext(status_hint(other->display_growid, i_accept, they_accept, (give_total + get_total) > 0))
            .add_spacer("small")
            .add_textbox(std::format("`c> THEY GIVE``   {}", chip(they_accept)));
        {
            bool any = false;
            for (const auto &o : their_off)
            {
                if (o.item <= 0 || o.count <= 0) continue;
                any = true;
                dialog.add_label_with_icon("small", item_line(o), o.item);
            }
            if (!any) dialog.add_smalltext("`oNothing offered yet.``");
        }
        dialog.add_spacer("small")
            .add_textbox(std::format("`c> YOU GIVE``   {}", chip(i_accept)));
        {
            bool any = false;
            for (std::size_t i = 0; i < my_off.size(); ++i)
            {
                const auto &o = my_off[i];
                if (o.item <= 0 || o.count <= 0) continue;
                any = true;
                dialog.add_label_with_icon("small", item_line(o), o.item);
                dialog.add_button(std::format("tremove_{}", i), std::format("`4Remove`` `ox{}``", o.count));
            }
            if (!any) dialog.add_smalltext("`oNothing offered yet.``");
        }
        dialog.add_spacer("small")
            .add_smalltext(std::format("`oYou give `w{}`` item(s)   |   You get `w{}`` item(s)``", give_total, get_total));

        // @note same as the vending machine: tap the picker -> choose an item from your inventory.
        //       The picker posts "itemID" back; "amount" (optional) is sent along with it.
        dialog.add_spacer("small")
            .add_textbox("`c> ADD ITEM``")
            .add_item_picker("itemID", "`wTap to pick an item from your inventory``", "Choose the item you want to offer.")
            .add_text_input("amount", "Amount (blank = all)", std::string{}, 4);

        if (SHOW_ADD_GRID)
        {
            int shown = 0;
            for (const auto &sl : me->slots)
            {
                if (shown >= 30) break;
                if (sl.id <= 0 || sl.count <= 0) continue;
                if (sl.id == 18 || sl.id == 32) continue;
                if (sl.id >= static_cast<int>(items.size())) continue;
                const ::item &it = id_to_item(static_cast<u_short>(sl.id));
                if (it.id != sl.id || (it.cat & CAT_UNTRADEABLE)) continue;
                int reserved = 0;
                for (const auto &o : my_off) if (o.item == sl.id) reserved += o.count;
                const int left = sl.count - reserved;
                if (left <= 0) continue;
                dialog.add_label_with_icon("small", std::format("`w{}`` `ox{}``", it.raw_name, left), sl.id);
                dialog.add_button(std::format("tadd_{}", sl.id), std::format("Add x{}", left));
                ++shown;
            }
        }

        dialog.add_spacer("small")
            .add_button("trade_accept", i_accept ? "`2ACCEPTED - tap to undo``" : "`2ACCEPT TRADE``")
            .embed_data("trade_seq", s.seq)
            .embed_data("trade_win", 1)
            .add_quick_exit();
        std::printf("[trade] dialog sent to %s (seq %d)\n", me->growid.c_str(), s.seq);
        // @note bottom standard button = Cancel only (the Accept button is the green one above)
        send_varlist(viewer, {"OnDialogRequest", dialog.end_dialog("trade", "Cancel", "")});
    }

    /* Re-send BOTH dialogs, then instantly tell the partner what changed.
       The notice (screen overlay + chat) shows the actor's FULL current offer,
       so the partner sees it right away even if their open dialog is not
       replaced by the client yet.
       @param actor peer who changed something (nullptr = no notice)
       @param what  short text, e.g. "added an item" */
    void show_both(trade::session &s, ENetPeer *actor = nullptr, const std::string &what = "")
    {
        touch(s);
        ++s.seq; // @note unique payload per mutation
        show(s.a, s, true);
        show(s.b, s, false);

        if (actor)
        {
            ENetPeer *partner = (actor == s.a) ? s.b : s.a;
            ::peer *who = pp(actor);
            if (partner && who)
            {
                const auto &actor_off = (actor == s.a) ? s.offers_a : s.offers_b;
                const std::string msg = std::format("`w{}`` {}. `oOffer:`` `2{}``",
                    who->display_growid, what, offer_text(actor_off));
                send_varlist(partner, {"OnTextOverlay", msg});
                on::ConsoleMessage(partner, msg);
            }
        }
        if (host) enet_host_flush(host); // @note send now, not on the next service tick
    }
}

trade::session *trade::find(ENetPeer *p)
{
    auto it = sessions.find(p);
    return it == sessions.end() ? nullptr : it->second.get();
}

void trade::close(ENetPeer *p, bool notify)
{
    auto it = sessions.find(p);
    if (it == sessions.end()) return;
    ENetPeer *other = (it->second->a == p) ? it->second->b : it->second->a;
    sessions.erase(it);
    auto jt = sessions.find(other);
    if (jt != sessions.end() && (jt->second->a == p || jt->second->b == p)) sessions.erase(jt);
    if (notify && other && pp(other))
    {
        send_varlist(other, {"OnTextOverlay", "`4Trade cancelled.``"});
        on::ConsoleMessage(other, "`4Trade cancelled.``");
        send_varlist(other, {"OnDialogRequest", themed()
            .add_label("big", "`4Trade cancelled``")
            .add_spacer("small")
            .add_smalltext("`oThe trade was cancelled. No items were moved.``")
            .end_dialog("trade_closed", "Close", "OK")});
    }
}

void trade::open(ENetEvent &event, int target_netid)
{
    ::peer *me = pp(event.peer);
    if (!me) return;

    ENetPeer *target = nullptr;
    peers(me->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p) {
        ::peer *o = pp(&p);
        if (o && o->netid == target_netid && &p != event.peer) target = &p;
    });
    if (!target) { on::ConsoleMessage(event.peer, "`4That player is not here.``"); return; }
    if (!same_world(event.peer, target)) { on::ConsoleMessage(event.peer, "`4You must be in the same world to trade.``"); return; }

    // @note 1) I already have a trade (maybe I just closed its window with the X)
    if (session *mine = trade::find(event.peer))
    {
        ENetPeer *partner = (mine->a == event.peer) ? mine->b : mine->a;
        if (partner == target && !session_dead(*mine))
        {
            // same partner -> just re-open the SAME trade, offers are still there
            touch(*mine);
            show(event.peer, *mine, mine->a == event.peer);
            return;
        }
        trade::close(event.peer, true); // abandoned, or I'm starting one with someone else
    }

    // @note 2) the other player is busy — but only if their trade is still alive
    if (session *busy = trade::find(target))
    {
        if (!session_dead(*busy)) { on::ConsoleMessage(event.peer, "`4That player is already trading.``"); return; }
        trade::close(target, true); // abandoned trade, clean it up
    }

    auto s = std::make_shared<session>();
    s->a = event.peer;
    s->b = target;
    s->last_active = std::time(nullptr);
    sessions[event.peer] = s;
    sessions[target] = s;
    send_varlist(target, {"OnTextOverlay", std::format("`2{} opened a trade!``", me->display_growid)});
    on::ConsoleMessage(target, std::format("`2{} opened a trade!``", me->display_growid));
    auto *mine = trade::find(event.peer);
    if (mine) show_both(*mine); // @note both windows open together
}

void trade::handle(ENetEvent &event, const ::hPipe &hPipe)
{
    const std::string action = hPipe["buttonClicked"];
    ::peer *dbg_me = pp(event.peer);
    std::printf("[trade] return from=%s action='%s' dialog='%s' seq='%s' itemID='%s' amount='%s'\n",
        dbg_me ? dbg_me->growid.c_str() : "?",
        action.c_str(), hPipe["dialog_name"].c_str(), hPipe["trade_seq"].c_str(),
        hPipe["itemID"].c_str(), hPipe["amount"].c_str());

    // @note wrench Trade button (dialog "popup") carries netID of the target.
    if (action == "trade")
    {
        int netid = 0;
        if (!parse_int(hPipe["netID"], netid) && !parse_int(hPipe["netid"], netid))
        {
            on::ConsoleMessage(event.peer, "`4Could not tell who to trade with. Wrench them again.``");
            return;
        }
        trade::open(event, netid);
        return;
    }

    trade::session *s = trade::find(event.peer);
    if (!s)
    {
        // @note stale dialog left open after the trade ended
        if (hPipe["dialog_name"] == "trade")
            on::ConsoleMessage(event.peer, "`oThis trade is no longer active.``");
        return;
    }
    touch(*s); // @note any click counts as activity
    const bool is_a = (s->a == event.peer);
    ENetPeer *other_p = is_a ? s->b : s->a;
    ::peer *me = pp(event.peer);
    if (!me || !same_world(event.peer, other_p))
    {
        on::ConsoleMessage(event.peer, "`4Trade cancelled — you left the world.``");
        trade::close(event.peer);
        return;
    }

    if (action == "trade_cancel" || action == "Close" || action == "Cancel" || action == "Back")
    {
        trade::close(event.peer);
        on::ConsoleMessage(event.peer, "`oTrade cancelled.``");
        return;
    }

    // @note Refresh: re-send the current dialog only (does NOT bump seq)
    if (action == "trade_refresh")
    {
        show(event.peer, *s, is_a);
        return;
    }

    auto &my_off = is_a ? s->offers_a : s->offers_b;

    // @note INVENTORY PICKER (like the vending machine): the client posts "itemID"
    //       when the player taps an item. It only counts as a pick when the click is
    //       NOT one of our own buttons (Accept / Remove / Add), because those can
    //       also carry an old itemID along.
    const std::string picked = hPipe["itemID"];
    if (!picked.empty() &&
        !action.starts_with("tadd_") && !action.starts_with("tremove_") &&
        action != "trade_accept" && action != "Accept")
    {
        int id = 0;
        if (!parse_int(picked, id))
        {
            on::ConsoleMessage(event.peer, "`4Unknown item.``");
            show(event.peer, *s, is_a);
            return;
        }
        int amount = 0; // @note blank / invalid = whole remaining stack
        const std::string amount_text = hPipe["amount"];
        if (!amount_text.empty() && (!parse_int(amount_text, amount) || amount < 0)) amount = 0;

        std::string err;
        if (!add_to_offer(event, my_off, id, amount, err))
        {
            on::ConsoleMessage(event.peer, err);
            show(event.peer, *s, is_a);
            return;
        }
        s->accept_a = s->accept_b = false;
        show_both(*s, event.peer, std::format("added `2{}``", id_to_item(static_cast<u_short>(id)).raw_name));
        return;
    }

    // @note old grid button (only used when SHOW_ADD_GRID = true)
    if (action.starts_with("tadd_"))
    {
        int id = 0;
        if (!parse_int(action.substr(5), id))
        {
            on::ConsoleMessage(event.peer, "`4Unknown item.``");
            show(event.peer, *s, is_a);
            return;
        }
        std::string err;
        if (!add_to_offer(event, my_off, id, 0, err))
        {
            on::ConsoleMessage(event.peer, err);
            show(event.peer, *s, is_a);
            return;
        }
        s->accept_a = s->accept_b = false;
        show_both(*s, event.peer, std::format("added `2{}``", id_to_item(static_cast<u_short>(id)).raw_name));
        return;
    }

    // @note tap "Remove" -> remove that offer line.
    if (action.starts_with("tremove_"))
    {
        int idx = -1;
        if (!parse_int(action.substr(8), idx) || idx < 0 || idx >= static_cast<int>(my_off.size()))
        {
            show(event.peer, *s, is_a);
            return;
        }
        my_off[static_cast<std::size_t>(idx)] = {};
        s->accept_a = s->accept_b = false;
        show_both(*s, event.peer, "removed an item");
        return;
    }

    // @note Accept is ONLY the explicit Accept button.
    const bool is_accept = action == "trade_accept" || action == "Accept";
    if (is_accept)
    {
        // @note STALE-DIALOG GUARD: if the player is looking at an old dialog
        // (partner changed the offer meanwhile), do NOT accept — show the fresh one.
        // Un-accepting is always allowed. Empty seq (client did not echo it) is let through.
        const bool already = is_a ? s->accept_a : s->accept_b;
        const std::string seq_text = hPipe["trade_seq"];
        if (!already && !seq_text.empty())
        {
            int seq = 0;
            if (!parse_int(seq_text, seq) || seq != s->seq)
            {
                on::ConsoleMessage(event.peer, "`4The offer changed! Check it again, then accept.``");
                send_varlist(event.peer, {"OnTextOverlay", "`4The offer changed! Check it again, then accept.``"});
                show(event.peer, *s, is_a);
                return;
            }
        }

        if (!offers_valid(*me, my_off) || !offers_valid(*pp(other_p), is_a ? s->offers_b : s->offers_a))
        {
            on::ConsoleMessage(event.peer, "`4An offer became invalid (items moved?). Re-add and try again.``");
            s->accept_a = s->accept_b = false;
            show_both(*s);
            return;
        }
        if (is_a) s->accept_a = !s->accept_a;
        else s->accept_b = !s->accept_b;

        if (s->accept_a && s->accept_b && !s->locked)
        {
            s->locked = true;
            ::peer *pa = pp(s->a), *pb = pp(s->b);
            bool ok = pa && pb && same_world(s->a, s->b) &&
                offers_valid(*pa, s->offers_a) && offers_valid(*pb, s->offers_b) &&
                fits(*pa, s->offers_a, s->offers_b) && fits(*pb, s->offers_b, s->offers_a);
            if (!ok)
            {
                s->locked = false;
                s->accept_a = s->accept_b = false;
                on::ConsoleMessage(s->a, "`4Trade failed: inventory changed or no space.``");
                on::ConsoleMessage(s->b, "`4Trade failed: inventory changed or no space.``");
                show_both(*s);
                return;
            }
            ENetEvent fa{.peer = s->a}, fb{.peer = s->b};
            take_offer(fa, s->offers_a);
            take_offer(fb, s->offers_b);
            give_offer(fa, s->offers_b);
            give_offer(fb, s->offers_a);
            pa->save_inventory(); pb->save_inventory();
            send_inventory_state(*s->a);
            send_inventory_state(*s->b);
            std::printf("[trade] %s (UID %d) gave [%s] <-> %s (UID %d) gave [%s]\n",
                pa->growid.c_str(), pa->user_id, offer_text(s->offers_a).c_str(),
                pb->growid.c_str(), pb->user_id, offer_text(s->offers_b).c_str());
            on::ConsoleMessage(s->a, "`2Trade complete!``");
            on::ConsoleMessage(s->b, "`2Trade complete!``");
            send_varlist(s->a, {"OnTextOverlay", "`2Trade complete!``"});
            send_varlist(s->b, {"OnTextOverlay", "`2Trade complete!``"});
            ENetPeer *sa = s->a, *sb = s->b;
            sessions.erase(sa);
            sessions.erase(sb);
            return;
        }
        const bool now_accepted = is_a ? s->accept_a : s->accept_b;
        show_both(*s, event.peer, now_accepted ? "accepted the trade" : "unaccepted the trade");
        return;
    }
}