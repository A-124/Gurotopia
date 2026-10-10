#include "pch.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include "RequestWorldSelectMenu.hpp"

void on::RequestWorldSelectMenu(ENetEvent& event) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    
    auto section = [](const auto& range, const std::string &color) 
    {
        std::string result;
        for (const auto &name : range) 
            if (!name.empty()) 
            {
                auto world = std::ranges::find(worlds, name, &::world::name); // @todo reduce iteration.
                result.append((world != worlds.end()) ? 
                    std::format("add_floater|{}|{}|0.5|{}\n", name, world->visitors, color) :
                    std::format("add_floater|{}|0|0.5|{}\n", name, color));
            }
        return result;
    };

    // @note Top Worlds: busiest first, only the top 10 (was every world with a player, in creation order)
    std::vector<const ::world*> busy{};
    for (const ::world &world : worlds)
        if (world.visitors > 0) busy.push_back(&world);
    std::ranges::stable_sort(busy, [](const ::world *l, const ::world *r) { return l->visitors > r->visitors; });
    if (busy.size() > 10) busy.resize(10);
    std::vector<std::string> popular_names{};
    for (const ::world *world : busy) popular_names.emplace_back(world->name);

    // @note recently visited: newest first, like the real game (the array stores the newest at the back)
    std::vector<std::string> recent_names(pPeer->recent_worlds.rbegin(), pPeer->recent_worlds.rend());

    send_varlist(event.peer, { 
        "OnRequestWorldSelectMenu", 
        std::format(
            "add_filter|\n"
            "add_heading|Top Worlds<ROW2>|\n{}{}"
            "add_heading|My Worlds<CR>|\n{}"
            "add_heading|Recently Visited Worlds<CR>|\n{}",
            "add_floater|wotd_world|\u013B WOTD|0|0.5|3529161471\n", 
            section(popular_names, "3529161471"), 
            section(pPeer->my_worlds, "2147418367"), 
            section(recent_names, "3417414143")
        ), 
        1
    });
    on::ConsoleMessage(event.peer, std::format("Where would you like to go? (`w{}`` online)", peers().size()));
}