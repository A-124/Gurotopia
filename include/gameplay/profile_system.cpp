#include "pch.hpp"
#include <ctime>
#include <algorithm>
#include <unordered_map>
#include "gameplay/profile_system.hpp"
#include "gameplay/title_system.hpp"
#include "gameplay/content_commands.hpp"
#include "commands/event_manager.hpp"
#include "commands/social.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/SetClothing.hpp"
#include "tools/ui.hpp"
#include "action/dialog_return/popup.hpp"

extern std::unordered_map<std::string_view, std::string_view> emoticon; // @note onVariant/EmoticonDataChanged.cpp

namespace profile_system
{
namespace
{
    constexpr std::string_view slot_names[10] = { "Hair", "Shirt", "Pants", "Feet", "Face", "Hand", "Back", "Head", "Charm", "Ancestral" };

    ::peer *peer_of(ENetPeer *p) { return (p && p->data) ? static_cast<::peer*>(p->data) : nullptr; }

    /* @return the peer standing in the same world with this netid */
    ENetPeer *find_by_netid(::peer &self, int netid)
    {
        ENetPeer *found = nullptr;
        if (netid <= 0 || self.netid == 0) return nullptr;
        peers(self.recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &other)
        {
            if (peer_of(&other) && peer_of(&other)->netid == netid) found = &other;
        });
        return found;
    }

    void open(ENetEvent &event, std::string d) { send_varlist(event.peer, { "OnDialogRequest", std::move(d) }); }

    std::string clothes_list(const ::peer &p)
    {
        std::string d;
        bool any = false;
        for (std::size_t i = 0; i < p.clothing.size(); ++i)
        {
            const int id = static_cast<int>(p.clothing[i]);
            if (id <= 0 || id >= static_cast<int>(items.size())) continue;
            any = true;
            d += std::format("add_label_with_icon|small|`o{}:`` `w{}``|left|{}|\n", slot_names[i], id_to_item(static_cast<u_short>(id)).raw_name, id);
        }
        if (!any) d += "add_textbox|`oNothing is being worn.``|left|\n";
        return d;
    }

    void notebook_dialog(ENetEvent &event, std::string notice = {})
    {
        ::peer *p = peer_of(event.peer);
        if (!p) return;
        std::string d = ui::header("Notebook", 32, "Private notes that follow your account.");
        if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);
        d += std::format("add_text_input|notebook_text|Notes:|{}|200|\n", ui::sanitize(p->notebook, 200));
        d += "add_smalltext|Only you can see this. Up to 200 characters.|left|\n";
        d += "add_button|clear_notebook|`4Clear Notebook``|noflags|0|0|\n";
        d += "embed_data|profile_page|notebook\n";
        d += ui::footer("profile_menu", "Close", "Save");
        open(event, std::move(d));
    }

    void bio_dialog(ENetEvent &event, std::string notice = {})
    {
        ::peer *p = peer_of(event.peer);
        if (!p) return;
        std::string d = ui::header("Personalize Profile", 18, "What other players see when they wrench you.");
        if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);
        d += std::format("add_text_input|bio_text|About me:|{}|80|\n", ui::sanitize(p->bio, 80));
        d += "add_smalltext|Colour codes like `` `2 `` work. Up to 80 characters.|left|\n";
        d += ui::section("Quick links");
        d += "add_button|open_titles|Choose Title|noflags|0|0|\n";
        d += "embed_data|profile_page|bio\n";
        d += ui::footer("profile_menu", "Close", "Save");
        open(event, std::move(d));
    }

    void wardrobe_dialog(ENetEvent &event, std::string notice = {})
    {
        ::peer *p = peer_of(event.peer);
        if (!p) return;
        std::string d = ui::header("Wardrobe", 18, "Everything you are wearing right now.");
        if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);
        d += clothes_list(*p);
        d += ui::section("Owned clothing");
        int owned = 0;
        for (const ::slot &s : p->slots)
        {
            if (s.id <= 0 || s.id >= static_cast<int>(items.size())) continue;
            const ::item &it = id_to_item(static_cast<u_short>(s.id));
            if (it.cloth_type == clothing::NONE) continue;
            ++owned;
            const bool worn = static_cast<int>(p->clothing[it.cloth_type]) == s.id;
            d += std::format("add_label_with_icon|small|`w{}`` `o({}){}``|left|{}|\n", it.raw_name, slot_names[it.cloth_type], worn ? " `2worn" : "", s.id);
        }
        if (owned == 0) d += "add_textbox|`oYou do not own any clothing yet.``|left|\n";
        d += "add_spacer|small|\nadd_button|unequip_all|`4Take Everything Off``|noflags|0|0|\n";
        d += "embed_data|profile_page|wardrobe\n";
        d += ui::footer("profile_menu", "Close", "");
        open(event, std::move(d));
    }

    void growmojis_dialog(ENetEvent &event)
    {
        std::vector<std::string_view> names;
        for (const auto &[key, value] : emoticon) names.push_back(key);
        std::ranges::sort(names);
        std::string d = ui::header("Growmojis", 1366, std::format("{} emojis unlocked. Type the name in brackets in chat.", names.size()));
        std::string line;
        int in_line = 0;
        for (std::string_view name : names)
        {
            line += std::format("`w({})`` {}   ", name, emoticon.at(name));
            if (++in_line == 4) { d += std::format("add_smalltext|{}|left|\n", line); line.clear(); in_line = 0; }
        }
        if (!line.empty()) d += std::format("add_smalltext|{}|left|\n", line);
        d += "embed_data|profile_page|none\n";
        d += ui::footer("profile_menu", "Close", "");
        open(event, std::move(d));
    }

    void bank_dialog(ENetEvent &event)
    {
        ::peer *p = peer_of(event.peer);
        if (!p) return;
        std::string d = ui::header("World Lock Bank", 242, "Your locks and the worlds you own.");
        for (const int id : { 242, 1796, 7188 })
        {
            int count = 0;
            for (const ::slot &s : p->slots) if (s.id == id) count += s.count;
            d += std::format("add_label_with_icon|small|`w{}``: `2{}``|left|{}|\n", id_to_item(static_cast<u_short>(id)).raw_name, count, id);
        }
        const auto locked = std::ranges::count_if(p->my_worlds, [](const std::string &w) { return !w.empty(); });
        d += ui::section("Worlds");
        d += std::format("add_textbox|`oYou own `w{}`` locked world(s).``|left|\n", locked);
        d += std::format("add_textbox|`oGems: `w{}``|left|\n", p->gems);
        d += "add_button|open_myworlds|Show My Worlds|noflags|0|0|\n";
        d += "embed_data|profile_page|bank\n";
        d += ui::footer("profile_menu", "Close", "");
        open(event, std::move(d));
    }

    void other_clothes_dialog(ENetEvent &event, ::peer &target)
    {
        std::string d = ui::header(std::format("{}'s Outfit", target.growid), 18);
        d += clothes_list(target);
        d += "embed_data|profile_page|none\n";
        d += ui::footer("profile_menu", "Close", "");
        open(event, std::move(d));
    }

    void pm_dialog(ENetEvent &event, ::peer &target, int netid, std::string notice = {})
    {
        std::string d = ui::header("Send Message", 1366, std::format("To {}", target.growid));
        if (!notice.empty()) d += std::format("add_textbox|{}|left|\n", notice);
        d += "add_text_input|pm_text|Message:||120|\n";
        d += std::format("embed_data|profile_page|pm\nembed_data|target_netid|{}\n", netid);
        d += ui::footer("profile_menu", "Cancel", "Send");
        open(event, std::move(d));
    }
}

std::string playtime_text(const ::peer &p) { return ui::duration(p.total_playtime()); }

std::string account_age_text(const ::peer &p)
{
    const long long days = p.created_at > 0 ? std::max<long long>(0, (std::time(nullptr) - p.created_at) / 86400) : 0;
    return days == 1 ? "1 day" : std::format("{} days", days);
}

std::string effects_text(const ::peer &p)
{
    std::vector<std::string> out;
    if (p.state & S_DOUBLE_JUMP) out.emplace_back("`2Double Jump``");
    if (p.punch_effect != 0) out.emplace_back(std::format("`2Punch effect #{}``", static_cast<int>(p.punch_effect)));
    if (p.state & S_GHOST) out.emplace_back("`2Ghost``");
    if (is_event_active()) out.emplace_back(std::format("`2Event x{:.1f} gems, x{:.1f} XP``", get_gem_multiplier(), get_xp_multiplier()));
    return out.empty() ? std::string{"`oNone``"} : join(out, ", ");
}

bool handle_popup(ENetEvent &event, const ::hPipe &pipe)
{
    ::peer *p = peer_of(event.peer);
    if (!p) return false;
    const std::string button = pipe["buttonClicked"];

    if (button == "title_edit")              { title_system::show(event); return true; }
    if (button == "notebook_edit")           { notebook_dialog(event); return true; }
    if (button == "open_personlize_profile") { bio_dialog(event); return true; }
    if (button == "wardrobe_customization")  { wardrobe_dialog(event); return true; }
    if (button == "emojis")                  { growmojis_dialog(event); return true; }
    if (button == "open_worldlock_storage")  { bank_dialog(event); return true; }
    if (button == "marvelous_missions")      { features_command(event, ""); return true; }

    // @note buttons on someone else's wrench menu
    if (button == "show_clothes" || button == "sendpm")
    {
        ENetPeer *other = find_by_netid(*p, std::atoi(pipe["netID"].c_str()));
        ::peer *target = peer_of(other);
        if (!target)
        {
            on::ConsoleMessage(event.peer, "`4That player is no longer here.``");
            return true;
        }
        if (button == "show_clothes") other_clothes_dialog(event, *target);
        else if (target == p) on::ConsoleMessage(event.peer, "`4You can't message yourself.``");
        else pm_dialog(event, *target, target->netid);
        return true;
    }
    return false;
}

void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe)
{
    ::peer *p = peer_of(event.peer);
    if (!p) return;

    const std::string button = pipe["buttonClicked"];
    const std::string page = pipe["profile_page"];

    if (button == "open_titles")  { title_system::show(event); return; }
    if (button == "open_myworlds") { ::hPipe fake{ "buttonClicked|my_worlds|" }; ::popup(event, fake); return; }

    if (page == "notebook")
    {
        if (button == "clear_notebook") { p->notebook.clear(); p->save_profile(); notebook_dialog(event, "`2Notebook cleared.``"); return; }
        p->notebook = ui::sanitize(pipe["notebook_text"], 200);
        p->save_profile();
        on::ConsoleMessage(event.peer, "`2Notebook saved.``");
    }
    else if (page == "bio")
    {
        p->bio = ui::sanitize(pipe["bio_text"], 80);
        p->save_profile();
        on::ConsoleMessage(event.peer, p->bio.empty() ? "`oProfile line removed.``" : "`2Profile updated.`` Others will see it when they wrench you.");
    }
    else if (page == "wardrobe" && button == "unequip_all")
    {
        p->clothing.fill(0.0f);
        p->update_effects();
        p->save_clothing();
        if (p->netid != 0 && !p->recent_worlds.back().empty())
            peers(p->recent_worlds.back(), PEER_SAME_WORLD, [p](ENetPeer &r) { on::SetClothing(r, *p); });
        wardrobe_dialog(event, "`2You took everything off.``");
    }
    else if (page == "pm")
    {
        ENetPeer *other = find_by_netid(*p, std::atoi(pipe["target_netid"].c_str()));
        ::peer *target = peer_of(other);
        const std::string message = ui::sanitize(pipe["pm_text"], 120);
        if (!target) { on::ConsoleMessage(event.peer, "`4That player is no longer here.``"); return; }
        if (message.empty()) return;
        command_msg(event, std::format("msg {} {}", target->growid, message)); // @note reuse /msg: mute checks, reply target and formatting
    }
}
}
