#include "pch.hpp"

#include "onVariant/ConsoleMessage.hpp"
#include "action/join_request.hpp"
#include "resetallworld.hpp"

namespace
{
    void reset_world_data(::world &world)
    {
        world.owner = 0;
        world.access.fill(0);
        world.is_public = false;
        world.lock_state = 0;
        world.minimum_entry_level = 1;
        world.doors.clear();
        world.signs.clear();
        world.trees.clear();
        world.displays.clear();
        world.random_blocks.clear();
        world.provider_cooldowns.clear();
        world.vending_machines.clear();
        world.objects.clear();
        world.last_object_uid = 0;
        world.weather = {0, 0};
        generate_world(world);
        world.save_metadata();
        world.save_provider_cooldowns();
        world.save_vending_machines();
        world.save_blocks();
        world.save_objects();
    }
}

void resetallworld(ENetEvent &event, const std::string_view text)
{
    if (!event.peer) return;
    auto *player = static_cast<::peer*>(event.peer->data);
    if (!player) return;

    if (player->role < DEVELOPER)
    {
        on::ConsoleMessage(event.peer, "`4You do not have permission to reset all worlds.``");
        return;
    }

    const std::string current_world = player->recent_worlds.back();
    if (current_world.empty())
    {
        on::ConsoleMessage(event.peer, "`4Enter a world before resetting all worlds.``");
        return;
    }

    if (text != "resetallworld confirm")
    {
        on::ConsoleMessage(event.peer,
            "`4WARNING: this permanently clears every world's builds, locks, items, and machine data.`` "
            "If you intend to proceed, type `/resetallworld confirm`.``");
        return;
    }

    if (peers().size() != 1)
    {
        on::ConsoleMessage(event.peer, "`4All other players must be offline before resetting every world.``");
        return;
    }

    if (!db || mysql_query(db, "SELECT name FROM world") != 0)
    {
        on::ConsoleMessage(event.peer, "`4Could not read the world list from MariaDB.``");
        return;
    }

    MYSQL_RES *result = mysql_store_result(db);
    if (!result)
    {
        on::ConsoleMessage(event.peer, "`4Could not load the world list from MariaDB.``");
        return;
    }

    std::vector<std::string> world_names;
    while (MYSQL_ROW row = mysql_fetch_row(result))
        if (row[0]) world_names.emplace_back(row[0]);
    mysql_free_result(result);

    if (world_names.empty())
    {
        on::ConsoleMessage(event.peer, "`4There are no worlds to reset.``");
        return;
    }

    for (const std::string &name : world_names)
    {
        auto loaded = std::ranges::find(worlds, name, &::world::name);
        if (loaded != worlds.end())
        {
            reset_world_data(*loaded);
        }
        else
        {
            ::world unloaded_world{name};
            reset_world_data(unloaded_world);
        }
    }

    on::ConsoleMessage(event.peer, std::format("`2Reset {} worlds. Rejoining `{}`...``", world_names.size(), current_world));
    auto loaded_current = std::ranges::find(worlds, current_world, &::world::name);
    if (loaded_current != worlds.end()) worlds.erase(loaded_current);
    player->netid = 0;
    action::join_request(event, "", current_world);
}
