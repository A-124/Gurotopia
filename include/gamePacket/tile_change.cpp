#include "pch.hpp"
#include "onVariant/NameChanged.hpp"
#include "onVariant/SetClothing.hpp"
#include "onVariant/Action.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "commands/weather.hpp"
#include "item_activate.hpp"
#include "tools/random.hpp"
#include "tools/time.hpp"
#include "tools/create_dialog.hpp"
#include "action/quit_to_exit.hpp"
#include "action/join_request.hpp"
#include "item_activate_object.hpp"
#include "action/dialog_return/vending.hpp"
#include "automate/holiday.hpp"
#include "commands/event_manager.hpp"
#include "database/custom_content.hpp"
#include "core/event_bus.hpp"

#include "tile_change.hpp"

#include <ctime>

void tile_change(ENetEvent& event, ::gamePacket gamePacket) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    try
    {
        auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
        if (world == worlds.end()) return;

        ::block &block = world->blocks[cord(gamePacket.punch.x, gamePacket.punch.y)];

        const ::item &item = id_to_item((gamePacket.id != 32 && gamePacket.id != 18) ? gamePacket.id : (block.fg != 0) ? block.fg : block.bg);
        if (item.id == 0) return;

        // A vending machine is a public shop interaction: visitors can wrench it to buy,
        // even when the surrounding world is private. This does not grant build access.
        const bool vending_interaction = gamePacket.id == 32 && item.type == type::VENDING_MACHINE;

        if (block.state[3] & S_FIRE) // @note allow anyone to take out fire
            if (pPeer->clothing[clothing::HAND] == 3066/* fire hose */)
            {
                remove_fire(event, gamePacket, block, *world);
                return; // @note avoid hitting the block
            }

        const bool punching_public_block = gamePacket.id == 18 && (item.cat & CAT_PUBLIC);
        if (!punching_public_block && !vending_interaction)
            if ((world->owner && !world->is_public && !pPeer->role) &&
                (pPeer->user_id != world->owner && std::ranges::find(world->access, pPeer->user_id) == world->access.end())) return;

        bool tile_update{};
        bool lock_visuals{}; // @todo this looks sloppy
        
        if (gamePacket.id == 18) // @note punching a block
        {
            if (item.hit_reset > 0)
            {
                const std::size_t layer = (block.fg != 0) ? 0u : 1u;
                const u_int now = ticks();
                if (block.last_hit[layer] != 0 && now >= block.last_hit[layer] &&
                    now - block.last_hit[layer] >= static_cast<u_int>(item.hit_reset))
                    block.hits[layer] = 0;
                block.last_hit[layer] = now;
            }

            static bool punch{}; // @note true if tile_change has been called within this inital (punch)

            if (!punch) // @note put all multiple punch features here
            {
                punch = true;
                if (pPeer->clothing[clothing::HAND] == 5480) // @note Rayman's Fist
                {
                    ::gamePacket copy_gamePacket = gamePacket;

                    /* @note up and down */
                    if (gamePacket.punch.y == gamePacket.pos.by_32(true).y)
                    {
                        copy_gamePacket.punch.x += (pPeer->facing_left) ? -1 : 1;
                        tile_change(event, std::move(copy_gamePacket));
                        
                        copy_gamePacket.punch.x += (pPeer->facing_left) ? -1 : 1;
                        tile_change(event, std::move(copy_gamePacket));
                    }
                    /* @note left and right <- -> */
                    else if (gamePacket.punch.x == gamePacket.pos.by_32(true).x)
                    {
                        copy_gamePacket.punch.y += (gamePacket.punch.y < gamePacket.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_gamePacket));

                        copy_gamePacket.punch.y += (gamePacket.punch.y < gamePacket.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_gamePacket));
                    }
                    /* @note horizontal adjacent \/ */
                    else if (gamePacket.punch.y != gamePacket.pos.by_32(true).y)
                    {
                        copy_gamePacket.punch.x += (pPeer->facing_left) ? -1 : 1;
                        copy_gamePacket.punch.y += (gamePacket.punch.y < gamePacket.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_gamePacket));

                        copy_gamePacket.punch.x += (pPeer->facing_left) ? -1 : 1;
                        copy_gamePacket.punch.y += (gamePacket.punch.y < gamePacket.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_gamePacket));
                    }
                }
                punch = false;
            }
            u_char apply_damage_value{}; // @note used to change a tile value without using send_tile_update() 

            if (pPeer->clothing[clothing::HAND] == 2952/*Digger's Spade*/)
            {
                if(item.id == 2/*Dirt*/ || item.id == 14)
                {
                    if (block.fg != 0) block.hits[0] = 3;
                    else block.hits[1] = 3;
    
                    int color = (item.id ==  2/*Dirt*/) ? RandomRange(0x02, 0x03)/* @note idk if this is the correct one, at least by looking at the color it looks like dirt*/ : 
                                  (item.id == 14/*Cave Background*/) ? RandomRange(0x0e, 0x0f) : 0x02;

                    send_particle_effect(event, gamePacket.punch.by_32(), {color, 0x61});
                }
            }
            switch (item.id)
            {
                case 758: // @note Roulette Wheel
                {
                    const u_char number = RandomRange(0, 37); // upper bound is exclusive: 0 through 36.
                    constexpr std::array<u_char, 18> red_numbers{
                        1, 3, 5, 7, 9, 12, 14, 16, 18,
                        19, 21, 23, 25, 27, 30, 32, 34, 36
                    };
                    const char color = (number == 0) ? '2' :
                        (std::ranges::find(red_numbers, number) != red_numbers.end()) ? '4' : 'b';
                    const std::string message = std::format("[{} spun the wheel and got `{}{}``!]", pPeer->display_growid, color, number);
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&event, &pPeer, message](ENetPeer& peer)
                    {
                        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, message }, -1, 2000);
                        on::ConsoleMessage(event.peer, message, 2000);
                    });
                    break;
                }
            }
            switch (item.type)
            {
                case type::STRONG:
                    if (!pPeer->one_hit) throw std::runtime_error("It's too strong to break.");
                    break;
                case type::MAIN_DOOR: throw std::runtime_error("(stand over and punch to use)");
                case type::LOCK:
                {
                    if (is_tile_lock(item.id)) break; // @todo seperate area for 'range_lock'

                    if (world->owner != pPeer->user_id)
                        throw std::runtime_error(std::format("`5[```w{}`` `$World Locked`` by (null)`5]``", world->name)); // @todo add owner name
                    break;
                }
                case type::VENDING_MACHINE:
                {
                    const auto machine = std::ranges::find(world->vending_machines, gamePacket.punch,
                        &::vending_machine_state::pos);
                    if (machine != world->vending_machines.end() && (machine->stock > 0 || machine->bank > 0))
                        throw std::runtime_error("Empty the Vending Machine and collect its balance before breaking it.");
                    break;
                }
                case type::PROVIDER:
                {
                    const u_int now = static_cast<u_int>(std::time(nullptr));
                    auto cooldown = std::ranges::find(world->provider_cooldowns, gamePacket.punch, &::provider_cooldown::pos);
                    const bool cooling = item.tick > 0 && cooldown != world->provider_cooldowns.end() && now >= cooldown->last_used &&
                        now - cooldown->last_used < static_cast<u_int>(item.tick);
                    if (cooling)
                    {
                        const u_int seconds_left = static_cast<u_int>(item.tick) - (now - cooldown->last_used);
                        const u_int days = seconds_left / 86400u;
                        const u_int hours = (seconds_left % 86400u) / 3600u;
                        const u_int minutes = (seconds_left % 3600u) / 60u;
                        const u_int seconds = seconds_left % 60u;
                        std::string wait;
                        if (days > 0) wait = std::format("{}d {}h {}m", days, hours, minutes);
                        else if (hours > 0) wait = std::format("{}h {}m", hours, minutes);
                        else if (minutes > 0) wait = std::format("{}m {}s", minutes, seconds);
                        else wait = std::format("{}s", seconds);
                        on::ConsoleMessage(event.peer, std::format("This provider is not ready yet. Try again in {}.", wait));
                        // @note no return: punches must still damage/break the provider below.
                    }
                    else
                    {
                        switch (item.id)
                        {
                            case 1008: // @note ATM
                            {
                                u_char gems = RandomRange(1, 101); // RandomRange uses an exclusive upper bound.
                                for (short i : {100, 50, 10, 5, 1}/* gem type */)
                                    for (; gems >= i; gems -= i/* downgrade type */)
                                        add_drop(event, {112, i}, gamePacket.punch.by_32(), *world);
                                        
                                break;
                            }
                            case 872: /* chicken */ case 866: /* cow */
                            {
                                add_drop(event, ::slot(item.id + 2, RandomRange(1, 3)), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            case 1632: // Coffee Maker produces one Coffee.
                            {
                                add_drop(event, ::slot(1634, 1), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            case 3888: // Sheep produces 1-3 Wool.
                            {
                                add_drop(event, ::slot(3890, RandomRange(1, 4)), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            case 1044: // Buffalo produces Milk (item 868), unlike its unrelated neighboring IDs.
                            {
                                add_drop(event, ::slot(868, RandomRange(1, 3)), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            case 5116:/*tea set*/
                            {
                                add_drop(event, ::slot(item.id - 2, RandomRange(1, 3)), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            case 2798:/*well*/
                            {
                                add_drop(event, ::slot(822/*water bucket*/, RandomRange(1, 3)), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            case 928:/*science station*/ // @note source: https://growtopia.fandom.com/wiki/Science_Station
                            {
                                short chemcial = 
                                    (!RandomRange(0, 16)) ? chemcial = 918/*P*/ : 
                                    (!RandomRange(0, 8))  ? chemcial = 920/*B*/ : 
                                    (!RandomRange(0, 6))  ? chemcial = 924/*Y*/ : 
                                    (!RandomRange(0, 4))  ? chemcial = 916/*R*/ : chemcial = 914/*G*/;
                                add_drop(event, {chemcial, 1}, gamePacket.punch.by_32(), *world);
                                break;
                            }
                            case 3044: // Tackle Box: one or two of its seven bait types.
                            {
                                constexpr std::array<short, 7> bait_ids{
                                    2914, // Wiggly Worm
                                    3012, // Shiny Flashy Thing
                                    3014, // Salmon Eggs
                                    3016, // Fishing Fly
                                    3018, // Shrimp Lure
                                    5526, // Uranium Glowing Lure
                                    5528  // Mega-Pellet Bait
                                };
                                const short bait_id = bait_ids[RandomRange(0, static_cast<int>(bait_ids.size()))];
                                add_drop(event, ::slot(bait_id, RandomRange(1, 3)), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            default:
                            {
                                // @note unlisted provider (verified type 0x26 in items.dat,
                                // e.g. Gumball Machine 16104, Ice Cream Truck 16238,
                                // Well of Love 10656, Wonder Provider 12680, Balloon
                                // Filling Station 4858): cooldown + damage/break work,
                                // but no drop — items.dat stores no drop tables, so a
                                // drop case is added only with a wiki-verified mapping.
                                break;
                            }
                        }
                        if (cooldown == world->provider_cooldowns.end())
                            world->provider_cooldowns.emplace_back(now, gamePacket.punch);
                        else cooldown->last_used = now;
                        world->save_provider_cooldowns();
                    }
                    // @note harvest or cooldown-notice done: fall through so the punch
                    // still damages/breaks the provider per its items.dat hits.
                    break;
                }
                case type::SEED:
                {
                    auto tree = std::ranges::find(world->trees, gamePacket.punch, &::tree::pos);
                    if (tree == world->trees.end())
                    {
                        on::ConsoleMessage(event.peer, "This tree is missing its saved growth data.");
                        return;
                    }
                    const u_int now = static_cast<u_int>(std::time(nullptr));
                    if (now >= tree->tick && now - tree->tick >= static_cast<u_int>(std::max(item.tick, 0)))
                    {
                        block.hits[0] = 99;
                        add_drop(event, ::slot(item.id - 1, RandomRange(1, tree->fruit*3)), gamePacket.punch.by_32(), *world); // @note fruit (from tree)
                    }
                    break;
                }
                case type::WEATHER_MACHINE:
                case type::SFX_WEATHER_MACHINE:
                case type::SPRITE_WEATHER_MACHINE:
                {
                    const int weather_id = get_weather_id(item.id);
                    if (weather_id == 0 && item.id != 932)
                    {
                        on::ConsoleMessage(event.peer, "`4This weather machine needs configuration that the server does not support yet.``");
                        return;
                    }
                    // @note only one weather machine can be on at a time: switch the previous one off.
                    if (world->weather != gamePacket.punch)
                    {
                        const int old_x = world->weather.x_int(), old_y = world->weather.y_int();
                        if (old_x >= 0 && old_x < 100 && old_y >= 0 && old_y < 60)
                        {
                            ::block &previous = world->blocks[cord(old_x, old_y)];
                            if (previous.fg != 0 && is_weather_machine(id_to_item(previous.fg)) && (previous.state[2] & S_TOGGLE))
                            {
                                previous.state[2] &= ~S_TOGGLE;
                                ::gamePacket previous_packet = gamePacket;
                                previous_packet.punch = world->weather;
                                send_tile_update(event, std::move(previous_packet), previous, *world); // @note also saves the blocks
                            }
                        }
                    }
                    block.state[2] ^= S_TOGGLE; // @note if punched twice it can detoggle that is why we use ^= not |=

                    if (block.state[2] & S_TOGGLE) world->weather = gamePacket.punch;
                    else if (world->weather == gamePacket.punch) world->weather = ::pos{};

                    world->save_blocks(); // @note the toggle lives in the block state, save it now or the weather is lost on restart

                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [block, item](ENetPeer& p)
                    {
                        send_varlist(&p, { "OnSetCurrentWeather", (block.state[2] & S_TOGGLE) ? get_weather_id(item.id) : 0 });
                    });
                    break;
                }
                case type::TOGGLEABLE_BLOCK:
                case type::TOGGLEABLE_ANIMATED_BLOCK:
                case type::TOGGLEABLE_DEADLY:
                case type::TOGGLEABLE_MULTI_FRAME_BLOCK:
                {
                    block.state[2] ^= S_TOGGLE;
                    break;
                }
                case type::CHEST:
                case type::PINATA:
                case type::VALHOWLA_TREASURE:
                    break; // @note break-loot handled below like normal blocks (Lucky granted on break)
                case type::DONATION_BOX:
                {
                    // @note items.dat: "People can drop gifts into this box for you to pickup later!"
                    // @todo real donation storage (drop items in, owner picks up later).
                    if (block.hits[0] == 0)
                        on::ConsoleMessage(event.peer, "`oDrop items here for the owner to pick up later. (Donation storage coming soon.)``");
                    break;
                }
                case type::MAILBOX:
                {
                    // @note items.dat: "You've got mail! People can write you private messages with this."
                    // @todo real mailbox messages.
                    if (block.hits[0] == 0)
                        on::ConsoleMessage(event.peer, "`oNo new mail.``");
                    break;
                }
                case type::BULLETIN:
                {
                    // @note items.dat: "Lets people communicate by leaving messages to each other."
                    // @todo real bulletin messages.
                    if (block.hits[0] == 0)
                        on::ConsoleMessage(event.peer, "`oNobody has written here yet.``");
                    break;
                }
                case type::RANDOM:
                {
                    apply_damage_value = 
                        (item.id == 456/*Dice*/) ? RandomRange(0, 5) : 
                        (item.id == 1300/*Roshambo*/) ? RandomRange(1, 3) : 0;

                    auto random = std::ranges::find(world->random_blocks, gamePacket.punch, &::random_block::pos);
                    if (random == world->random_blocks.end())
                    {
                        world->random_blocks.emplace_back(::random_block{apply_damage_value, gamePacket.punch});
                    }
                    else random->value = apply_damage_value;
                    break;
                }
            }
            // /1hit primes the active layer so the normal damage pipeline breaks it on this punch.
            if (pPeer->one_hit && item.hits > 0)
            {
                const std::size_t hit_layer = (block.fg != 0) ? 0u : 1u;
                block.hits[hit_layer] = static_cast<u_char>(item.hits - 1);
            }

            tile_apply_damage(event, std::move(gamePacket), block, apply_damage_value);

            if (block.hits[0] >= item.hits) block.fg = 0, block.hits[0] = 0, block.last_hit[0] = 0;
            else if (block.hits[1] >= item.hits) block.bg = 0, block.hits[1] = 0, block.last_hit[1] = 0;
            else return;

            u_int displayed_item_id{};
            if (item.type == type::DISPLAY_BLOCK)
            {
                auto display = std::ranges::find(world->displays, gamePacket.punch, &::display::pos);
                if (display != world->displays.end())
                {
                    if (display->id > 0 && static_cast<std::size_t>(display->id) < items.size())
                        displayed_item_id = display->id;
                    world->displays.erase(display);
                }
            }
            else if (item.type == type::VENDING_MACHINE)
            {
                auto machine = std::ranges::find(world->vending_machines, gamePacket.punch,
                    &::vending_machine_state::pos);
                if (machine != world->vending_machines.end())
                {
                    world->vending_machines.erase(machine);
                    world->save_vending_machines();
                }
            }

            world->save_blocks();
            event_bus::emit({ event_bus::type::block_changed, event.peer, {}, item.id, 1 }); // @note quests & achievements

            /* @todo update these changes with tile_update() */
            block.state[2] = 0x00; // @note reset tile direction
            block.state[3] &= ~S_VANISH; // @note remove paint

            if (is_weather_machine(item) && world->weather == gamePacket.punch)
            {
                world->weather = ::pos{}; // @note machine is gone, so is the weather
                peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [](ENetPeer& p)
                {
                    send_varlist(&p, { "OnSetCurrentWeather", 0 });
                });
            }

            if (displayed_item_id != 0)
            {
                const short displayed_item = static_cast<short>(displayed_item_id);
                const auto held = std::ranges::find(pPeer->slots, displayed_item, &::slot::id);
                const bool can_fit = held != pPeer->slots.end()
                    ? held->count < 200
                    : pPeer->slots.size() < static_cast<std::size_t>(std::max(pPeer->slot_size, 0));
                if (can_fit)
                {
                    modify_item_inventory(event, ::slot(displayed_item, 1));
                    on::ConsoleMessage(event.peer, std::format("`2Returned {} from the Display Block to your backpack.``",
                        id_to_item(displayed_item).raw_name));
                }
                else
                {
                    const int uid = add_object(event, ::slot(displayed_item, 1), gamePacket.pos, *world);
                    item_activate_object(event, ::gamePacket{.id = uid, .punch = gamePacket.punch});
                    on::ConsoleMessage(event.peer, std::format("`4Your backpack is full; {} was dropped at your feet.``",
                        id_to_item(displayed_item).raw_name));
                }
            }
            
            if (item.id == 392/*Heartstone*/ || item.id == 3402/*GBC*/ || item.id == 9350/*Super GBC*/)
            {
                short reward =
                    (!RandomRange(0, 99)) ? 1458 : // @note GHC
                    (!RandomRange(0, 20)) ? 362 : // @note Angel Wings
                    (!RandomRange(0, 8))  ? 366 : // @note Heartbow
                    (!RandomRange(0, 8))  ? 1470 : // @note Ruby Necklace
                    (!RandomRange(0, 20)) ? 2384 : // @note Love Bug
                    (!RandomRange(0, 4))  ? 2396 : // @note Valensign
                    (!RandomRange(0, 10)) ? 3388 : // @note Heartbreaker Hammer
                    (!RandomRange(0, 10)) ? 2390 : // @note Teeny Angel Wings
                    (!RandomRange(0, 10)) ? 3396 : // @note Lovebird Pendant
                    (!RandomRange(0, 2))  ? 3404 : // @note Sour Lollipop
                    (!RandomRange(0, 4))  ? 3406 : // @note Sweet Lollipop
                    (!RandomRange(0, 2))  ? 3408 : // @note Pink Marble Arch
                    388; // @note Perfume
                    // @todo add all the remaining drops - https://growtopia.fandom.com/wiki/Golden_Booty_Chest

                add_drop(event, ::slot(reward, (reward == 3408 || reward == 3404) ? 10 : 1), gamePacket.punch.by_32(), *world);
                if (reward == 1458)
                {
                    std::string message = std::format("msg|`4The Power of Love! `2{} found a `#Golden Heart Crystal`2 in a `#{}`2!", pPeer->growid, item.raw_name);
                    peers(pPeer->recent_worlds.back(), PEER_ALL, [message](ENetPeer &p)
                    {
                        send_action(p, "log", message.c_str());
                    });
                }
                if (++pPeer->gbc_pity % 100 == 0) modify_item_inventory(event, ::slot{9350, 1});
            }
            else if (item.type == type::LOCK && !is_tile_lock(item.id))
            {
                pPeer->update_display_growid();
                on::NameChanged(event);
                
                world->owner = 0; // @todo have a seperate thing for 'range_lock'
                world->save_metadata();
            }

            // Items with property 0x08 are indestructible: breaking one returns it
            // as a world object, just like the CAT_RETURN item category.
            if ((item.cat & CAT_RETURN) || (item.property & 0x08))
            {
                int uid = add_object(event, ::slot(item.id, 1), gamePacket.pos, *world);
                item_activate_object(event, ::gamePacket{.id = uid, .punch = gamePacket.punch});
            }
            else // @note normal break (drop gem, seed, block & give XP)
            {
                if (item.type != type::SEED)
                { /* gem drop */
                    /* if greater than 1, assume it's a farmable.*/
                    u_char rarity_to_gem =
                        (item.rarity >= 87) ? 22 : 
                        (item.rarity >= 68) ? 18 : 
                        (item.rarity >= 53) ? 14 : 
                        (item.rarity >= 41) ? 11 : 
                        (item.rarity >= 36) ? 10 :
                        (item.rarity >= 32) ? 9 :
                        (item.rarity >= 24) ? 5 : 1;

                    const auto* custom_block = custom_content::find_item(item.id);
                    if (custom_block && custom_block->block_kind == custom_content::custom_block_kind::lucky_box)
                    {
                        // Positional weights: with N configured IDs, weights are N, N-1, ..., 1.
                        // This makes the leftmost entry most likely and the rightmost least likely.
                        const auto& drops = custom_block->lucky_box_drops;
                        int total_weight = static_cast<int>(drops.size() * (drops.size() + 1) / 2);
                        int roll = RandomRange(0, total_weight);
                        for (std::size_t n = 0; n < drops.size(); ++n)
                        {
                            const int weight = static_cast<int>(drops.size() - n);
                            if (roll < weight)
                            {
                                add_drop(event, ::slot(static_cast<short>(drops[n]), 1), gamePacket.punch.by_32(), *world);
                                break;
                            }
                            roll -= weight;
                        }
                    }
                    else if (custom_block && custom_block->block_kind == custom_content::custom_block_kind::pot_gold)
                    {
                        // Pot Gold guarantees a gem payout; the configured multiplier stacks with server events.
                        const int multiplier = std::max(custom_block->pot_gold_gem_multiplier, 1);
                        int gems = static_cast<int>(rarity_to_gem) * multiplier * get_gem_multiplier();
                        for (int i : {100, 50, 10, 5, 1}/* gem type, the denominations the client draws */)
                            for (; gems >= i; gems -= i)
                                add_drop(event, {112, static_cast<short>(i)}, gamePacket.punch.by_32(), *world);
                    }
                    else
                    {
                        if (!RandomRange(0, (rarity_to_gem > 1) ? 2 : 4)) // @note double chances if farmable. (RandomRange's upper bound is exclusive: (0, 1) was always 0, i.e. a 100% drop)
                        {
                            // @note RandomRange's upper bound is exclusive, so +1 makes rarity_to_gem a possible roll.
                            int gems = static_cast<int>(RandomRange(1, rarity_to_gem + 1) * get_gem_multiplier());
                            for (int i : {100, 50, 10, 5, 1}/* gem type, the denominations the client draws */)
                                for (; gems >= i; gems -= i/* downgrade type */)
                                    add_drop(event, {112, static_cast<short>(i)}, gamePacket.punch.by_32(), *world);
                        }
                        const bool never_drops_seed = (item.property & 0x04) != 0;
                        if (!never_drops_seed && !RandomRange(0, (rarity_to_gem > 1) ? 2 : 4))
                            add_drop(event, ::slot(item.id + 1, 1), gamePacket.punch.by_32(), *world);
                        else if (!RandomRange(0, (rarity_to_gem > 1) ? 4 : 8))
                            add_drop(event, ::slot(item.id, 1), gamePacket.punch.by_32(), *world);
                    }
                } /* ~gem drop */

                pPeer->add_xp(event, std::trunc(1.0f + item.rarity / 5.0f));
            }
        } // @note delete im, id
        else if (item.cloth_type != clothing::NONE) 
        {
            if (gamePacket.punch != pPeer->pos.by_32(true)) throw std::runtime_error("To wear clothing, use on yourself");

            item_activate(event, gamePacket);
            return; 
        }
        else if (item.type == type::CONSUMEABLE) 
        {
            if ((item.property & 0x80) && world->owner == 0)
                throw std::runtime_error("This item can only be used in a World-Locked world.");

            if (item.raw_name.find(" Blast") != std::string::npos)
            {
                send_varlist(event.peer, {
                    "OnDialogRequest",
                    std::format(
                        "set_default_color|`o\n"
                        "embed_data|id|{0}\n"
                        "add_label_with_icon|big|`w{1}``|left|{0}|\n"
                        "add_label|small|This item creates a new world! Enter a unique name for it.|left\n"
                        "add_text_input|name|New World Name||24|\n"
                        "end_dialog|create_blast|Cancel|Create!|\n", // @todo rgt "Create!" is a purple-ish pink color
                        item.id, item.raw_name
                    )
                });
            }

            if (item.raw_name.find("Paint Bucket - ") != std::string::npos && pPeer->clothing[clothing::HAND] != 3494) throw std::runtime_error("you need a Paintbrush to apply paint!");
            if (item.raw_name.find("Hair Dye") != std::string::npos)
            {
                if (gamePacket.punch != pPeer->pos.by_32(true)) throw std::runtime_error("Don't spill your dye!");
                else if (world->blocks[cord(pPeer->pos.by_32(true).x, pPeer->pos.by_32(true).y)].fg != 230/*Bathtub*/) throw std::runtime_error("You'll make a huge mess if you do that outside the Bathtub!");

                on::Action(event, "shower");
                // audio/shower.wav
                send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "You dyed your hair!", 0u, 1u });
            }
            float color{}; // @note the color of the particle effect.
            float particle{};
            switch (item.id)
            {
                case 1404: // @note Door Mover
                {
                    if (!door_mover(*world, gamePacket.punch)) throw std::runtime_error("There's no room to put the door there! You need 2 empty spaces vertically.");

                    std::string remember_name = world->name;
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
                    { 
                        ENetEvent fake{.peer = &p};
                        action::quit_to_exit(fake, "", true); // @note everyone in world exits
                        action::join_request(fake, "", remember_name); // @note everyone in world re-joins
                    });
                    return;
                }
                case 822: // @note Water Bucket
                {
                    if (block.state[3] & S_FIRE) remove_fire(event, gamePacket, block, *world);
                    else block.state[3] ^= S_WATER;
                    break;
                }
                case 1866: // @note Block Glue
                {
                    block.state[3] ^= S_GLUE;
                    break;
                }
                case 3062: // @note Pocket Lighter
                {
                    if (block.fg == 0 && block.bg == 0) throw std::runtime_error("There's nothing to burn!");
                    if (block.state[3] & (S_FIRE | S_WATER)) return; // @note avoid fire on water & fire on fire

                    block.state[3] |= S_FIRE;

                    std::string message = "`7[```4MWAHAHAHA!! FIRE FIRE FIRE```7]``";
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
                    {
                        send_varlist(&p, { "OnTalkBubble", pPeer->netid, message, 0u });
                        on::ConsoleMessage(&p, message);
                    });
                    particle = 0x96;

                    if (block.fg == 3090) // @note Highly Combustible Box
                    {
                        block.fg = 3128; // @note Combusted Box
                        if (!(block.state[2] & S_TOGGLE)/*closed*/) {} // @todo recipes: https://growtopia.fandom.com/wiki/Guide:Highly_Combustible_Box
                    }
                    break;
                }
                case 3404:/*Sour Lollipop*/ case 3406:/*Sweet Lollipop*/
                {
                    send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`#YUM!:D", 0u });

                    break;
                }
                case 3400: // @note Love Potion #8
                {
                    if (block.fg != 10) return; // @note Rock

                    block.fg = 392; // @note Heartstone
                    particle = 0x2c;
                    break;
                }
                case 1488: // @note Experience Potion
                {
                    send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`#GULP! You got smarter!", 0u });
                    pPeer->add_xp(event, 10000);
                    break;
                }
                case 834: // @note Fireworks
                {
                    fireworks(event, gamePacket.punch.by_32());
                    break;
                }
                case 2480: // @note Megaphone
                {
                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        ::create_dialog()
                            .set_default_color("`o")
                            .add_label_with_icon("big", "`wMegaphone``", item.id)
                            .add_textbox("Enter a message you want to broadcast to every player in Growtopia! This will use up 1 Megaphone")
                            .add_text_input("message", "", "", 128)
                            .end_dialog("megaphone", "Nevermind", "Broadcast")
                    });
                    break;
                }
                case 408: // @note Duct Tape
                {
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
                    {
                        ::peer *_p = static_cast<::peer*>(p.data);

                        if (gamePacket.punch == _p->pos.by_32(true))
                        {
                            _p->state |= S_DUCT_TAPE; // @todo add a 10 minute timer that will remove it.
                            on::SetClothing(p);
                        }
                    });
                    break;
                }
                case 3478: // @note Paint Bucket - Red
                {
                    block.state[3] |= S_RED;
                    color = bgra::RED, particle = 0xa8; 
                    break;
                }
                case 3480: // @note Paint Bucket - Yellow
                {
                    block.state[3] |= S_YELLOW;
                    color = bgra::GREEN | bgra::RED, particle = 0xa8;
                    break;
                }
                case 3482: // @note Paint Bucket - Green
                {
                    block.state[3] |= S_GREEN;
                    color = bgra::GREEN, particle = 0xa8;
                    break;
                }
                case 3484: // @note Paint Bucket - Aqua
                {
                    block.state[3] |= S_AQUA;
                    color = bgra::BLUE | bgra::GREEN, particle = 0xa8;
                    break;
                }
                case 3486: // @note Paint Bucket - Blue
                {
                    block.state[3] |= S_BLUE;
                    color = bgra::BLUE, particle = 0xa8;
                    break;
                }
                case 3488: // @note Paint Bucket - Purple
                {
                    block.state[3] |= S_PURPLE;
                    color = bgra::BLUE | bgra::RED, particle = 0xa8; // @note blue + red
                    break;
                }
                case 3490: // @note Paint Bucket - Charcoal
                {
                    block.state[3] |= S_CHARCOAL;
                    color = bgra::BLUE | bgra::GREEN | bgra::RED | bgra::ALPHA, particle = 0xa8; // @note max will provide a pure black color. idk if growtopia is the same.
                    break;
                }
                case 3492: // @note Paint Bucket - Vanish
                {
                    block.state[3] &= ~S_VANISH;
                    color = bgra::BLUE | bgra::GREEN | bgra::RED, particle = 0xa8; // @todo get exact color. I just guessed T-T
                    break;
                }
                case 3822: break; // Red Hair Dye
                case 3824: break; // Green Hair Dye
                case 3826: break; // Blue Hair Dye
                default: return; // @note prevent taking the consumeable if nothing happended
            }
            if (particle > 0.0f)
            {
                send_particle_effect(event, gamePacket.punch.by_32(), {color, particle});
            }
            send_tile_update(event, std::move(gamePacket), block, *world);

            modify_item_inventory(event, ::slot(item.id, -1));
            pPeer->add_xp(event, 1);
            return;
        }
        else if (gamePacket.id == 32)
        {
            switch (item.type)
            {
                case type::LOCK:
                {
                    if (is_tile_lock(item.id)) break; // @todo seperate area for 'range_lock'

                    if (pPeer->user_id == world->owner)
                    {
                        send_varlist(event.peer, {
                            "OnDialogRequest",
                            std::format(
                                "set_default_color|`o\n"
                                "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                                "add_popup_name|LockEdit|\n"
                                "add_label|small|`wAccess list:``|left\n"
                                "embed_data|tilex|{}\n"
                                "embed_data|tiley|{}\n"
                                "add_spacer|small|\n"
                                "add_label|small|Select an online player to add them to the access list.|left\n"
                                "add_spacer|small|\n"
                                "add_player_picker|playerNetID|`wAdd``|\n"
                                "add_checkbox|checkbox_public|Allow anyone to Build and Break|{}\n"
                                "add_checkbox|checkbox_disable_music|Disable Custom Music Blocks|{}\n"
                                "add_text_input|tempo|Music BPM|100|3|\n"
                                "add_checkbox|checkbox_disable_music_render|Make Custom Music Blocks invisible|0\n"
                                //"add_smalltext|Your current home world is: JOLEIT|left|\n"           // @todo only show when peer has a set home world.
                                "add_checkbox|checkbox_set_as_home_world|Set as Home World|0|\n"
                                "add_text_input|minimum_entry_level|World Level: |{}|3|\n"
                                "add_smalltext|Set minimum world entry level.|\n"
                                "add_button|sessionlength_dialog|`wSet World Timer``|noflags|0|0|\n"
                                "add_button|changecat|`wCategory: None``|noflags|0|0|\n"
                                "add_button|getKey|Get World Key|noflags|0|0|\n"
                                "end_dialog|lock_edit|Cancel|OK|\n",
                                item.raw_name, item.id, gamePacket.punch.x, gamePacket.punch.y, to_char(world->is_public), (world->lock_state & DISABLE_MUSIC) ? "1" : "0", world->minimum_entry_level
                            )
                        });
                    }
                    break;
                }
                case type::DOOR:
                case type::PORTAL:
                {
                    std::string label, dest, id{};
                    for (::door& door : world->doors)
                        if (door.pos == gamePacket.punch) { label = door.label, dest = door.dest, id = door.id; break; }
                        
                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        std::format(
                            "set_default_color|`o\n"
                            "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                            "add_text_input|door_name|Label|{}|100|\n"
                            "add_popup_name|DoorEdit|\n"
                            "add_text_input|door_target|Destination|{}|24|\n"
                            "add_smalltext|Enter a Destination in this format: `2WORLDNAME:ID``|left|\n"
                            "add_smalltext|Leave `2WORLDNAME`` blank (:ID) to go to the door with `2ID`` in the `2Current World``.|left|\n"
                            "add_text_input|door_id|ID|{}|11|\n"
                            "add_smalltext|Set a unique `2ID`` to target this door as a Destination from another!|left|\n"
                            "add_checkbox|checkbox_locked|Is open to public|1\n"
                            "embed_data|tilex|{}\n"
                            "embed_data|tiley|{}\n"
                            "end_dialog|door_edit|Cancel|OK|", 
                            item.raw_name, item.id, label, dest, id, gamePacket.punch.x, gamePacket.punch.y
                        )
                    });
                    break;
                }
                case type::SIGN:
                {
                    std::string label{};
                    for (::sign& sign : world->signs)
                        if (sign.pos == gamePacket.punch) { label = sign.label; break; }

                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        std::format(
                            "set_default_color|`o\n"
                            "add_popup_name|SignEdit|\n"
                            "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                            "add_textbox|What would you like to write on this sign?``|left|\n"
                            "add_text_input|sign_text||{}|128|\n"
                            "embed_data|tilex|{}\n"
                            "embed_data|tiley|{}\n"
                            "end_dialog|sign_edit|Cancel|OK|", 
                            item.raw_name, item.id, label, gamePacket.punch.x, gamePacket.punch.y
                        )
                    });
                    break;
                }
                case type::ENTRANCE:
                {
                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        std::format(
                            "set_default_color|`o\n"
                            "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                            "add_checkbox|checkbox_public|Is open to public|{}\n"
                            "embed_data|tilex|{}\n"
                            "embed_data|tiley|{}\n"
                            "end_dialog|gateway_edit|Cancel|OK|\n", 
                            item.raw_name, item.id, to_char((block.state[2] & S_PUBLIC)), gamePacket.punch.x, gamePacket.punch.y
                        )
                    });
                    break;
                }
                case type::DISPLAY_BLOCK:
                {
                    const auto display = std::ranges::find(world->displays, gamePacket.punch, &::display::pos);
                    auto dialog = ::create_dialog()
                        .set_default_color("`o")
                        .add_label_with_icon("big", std::format("`w{}``", item.raw_name), item.id)
                        .add_spacer("small")
                        .embed_data("tilex", gamePacket.punch.x)
                        .embed_data("tiley", gamePacket.punch.y);

                    if (display == world->displays.end())
                        dialog.add_textbox("This display is empty. Put an item on top of it to display it.");
                    else
                        dialog.add_label_with_icon("small", std::format("Displayed: `w{}``", id_to_item(display->id).raw_name), display->id)
                            .add_button("take_display", "Take Displayed Item");

                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        dialog.end_dialog("display_edit", "Close", "Done")
                    });
                    break;
                }
                case type::VENDING_MACHINE:
                {
                    open_vending_dialog(event.peer, *pPeer, *world, gamePacket.punch, item.id);
                    break;
                }
            }
            return; // @note leave early else wrench will act as a block unlike fist which breaks. this is cause of state_visuals()
        }
        else // @note placing a block
        {
            if ((item.property & 0x80) && world->owner == 0)
                throw std::runtime_error("This item can only be used in a World-Locked world.");

            if (block.fg != 0) // @note placing something ontop of exisitng block
            {
                bool update_tile{};
                switch (items[world->blocks[cord(gamePacket.punch.x, gamePacket.punch.y)].fg].type)
                {
                    case type::DISPLAY_BLOCK:
                    {
                        if (std::ranges::find(world->displays, gamePacket.punch, &::display::pos) != world->displays.end())
                            throw std::runtime_error("This Display Block already has an item. Take it out first.");
                        world->displays.emplace_back(::display(item.id, gamePacket.punch));
                        update_tile = true;
                        break;
                    }
                    case type::SEED:
                    {
                        auto tree = std::ranges::find(world->trees, gamePacket.punch, &::tree::pos);
                        if (tree == world->trees.end())
                            throw std::runtime_error("This tree is missing its saved growth data.");
                        if (block.state[2] == S_SPLICED) throw std::runtime_error("It would be too dangerous to try to mix three seeds.");
                        // @note recipe lookup uses items.dat splice[0]/splice[1] fields directly.
                        const ::item &held_seed = id_to_item(gamePacket.id);
                        const ::item &tree_seed = id_to_item(block.fg);
                        if (held_seed.type != type::SEED || tree_seed.type != type::SEED)
                            throw std::runtime_error("You can only splice two seeds together.");
                        if (held_seed.id != gamePacket.id || tree_seed.id != block.fg)
                            throw std::runtime_error("Unknown seed.");
                        const ::item *recipe = nullptr;
                        for (const ::item &candidate : items)
                        {
                            if (candidate.splice[0] == 0 && candidate.splice[1] == 0) continue;
                            if ((candidate.splice[0] == gamePacket.id && candidate.splice[1] == block.fg) ||
                                (candidate.splice[1] == gamePacket.id && candidate.splice[0] == block.fg))
                            {
                                recipe = &candidate;
                                break;
                            }
                        }
                        if (!recipe)
                            throw std::runtime_error(std::format("Hmm, it looks like `w{}`` and `w{}`` can't be spliced.", tree_seed.raw_name, held_seed.raw_name));
                        if ((recipe->cat & CAT_HOLIDAY) && !is_holiday_item_creation_season())
                            throw std::runtime_error("That recipe is only available during its holiday season.");
                        {
                            const ::item &splice0 = id_to_item(recipe->splice[0]);
                            const ::item &splice1 = id_to_item(recipe->splice[1]);
                            std::string tree_name = recipe->raw_name;
                            constexpr std::string_view seed_suffix = " Seed";
                            if (tree_name.ends_with(seed_suffix)) tree_name.resize(tree_name.size() - seed_suffix.size());

                            send_varlist(event.peer, {
                                "OnTalkBubble",
                                pPeer->netid,
                                std::format("`w{}`` and `w{}`` have been spliced to make a `${} Tree``!",
                                    splice0.raw_name, splice1.raw_name, tree_name),
                                0u, 1u
                            });
                            tree->tick = static_cast<u_int>(std::time(nullptr));
                            tree->fruit = static_cast<u_char>(RandomRange(1, 3)); // @note same 1-3 range as fresh planting below
                            block.state[2] = S_SPLICED;

                            block.fg = recipe->id;
                            update_tile = true;
                        }
                        break;
                    }
                }
                if (update_tile)
                {
                    modify_item_inventory(event, ::slot(gamePacket.id, -1));
                    send_tile_update(event, std::move(gamePacket), block, *world);
                    return;
                }
            }
            if ((item.type == type::BACKGROUND && block.bg != 0) ||
                (item.type != type::BACKGROUND && block.fg != 0)) return; // @note an extra check, i will later make this cleaner.
            if (item.collision == collision::FULL)
            {
                if (gamePacket.punch == gamePacket.pos.by_32(true)) return; // @todo when moving avoid collision.
            }
            switch (item.type)
            {
                case type::DOOR:
                {
                    world->doors.emplace_back("","","", gamePacket.punch);
                    tile_update = true;
                    break;
                }
                case type::SIGN:
                {
                    world->signs.emplace_back("", gamePacket.punch);
                    tile_update = true;
                    break;
                }
                case type::LOCK:
                {
                    if (is_tile_lock(item.id)) break; // @note seperate area for 'range_lock'

                    if (!world->owner)
                    {
                        world->owner = pPeer->user_id;
                        lock_visuals = true;

                        pPeer->update_display_growid();
                        on::NameChanged(event);
                        if (std::ranges::find(pPeer->my_worlds, world->name) == pPeer->my_worlds.end()) 
                        {
                            std::ranges::rotate(pPeer->my_worlds, pPeer->my_worlds.begin() + 1);
                            pPeer->my_worlds.back() = world->name;
                        }
                        std::string placed_message = std::format("`5[```w{}`` has been `$World Locked`` by {}`5]``", world->name, pPeer->growid);
                        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&event, &pPeer, placed_message](ENetPeer& peer) 
                        {
                            send_varlist(&peer, { "OnTalkBubble", pPeer->netid, placed_message });
                            on::ConsoleMessage(&peer, placed_message);
                        });
                    }
                    else throw std::runtime_error("Only one `$World Lock`` can be placed in a world, you'd have to remove the other one first.");
                    break;
                }
                case type::SEED:
                {
                    world->trees.emplace_back(static_cast<u_int>(std::time(nullptr)), RandomRange(1, 3), gamePacket.punch);
                    block.state[2] = 0x11; // @todo
                    tile_update = true;
                    break;
                }
                case type::ENTRANCE:
                {
                    block.state[2] |= S_PUBLIC;
                    tile_update = true;
                    break;
                }
                case type::PROVIDER:
                {
                    // A provider's items.dat tick is its initial grow time too.
                    // Start its persisted cooldown when placed so it cannot be
                    // harvested immediately after installation.
                    if (item.tick > 0)
                    {
                        const u_int placed_at = static_cast<u_int>(std::time(nullptr));
                        auto cooldown = std::ranges::find(world->provider_cooldowns, gamePacket.punch, &::provider_cooldown::pos);
                        if (cooldown == world->provider_cooldowns.end())
                            world->provider_cooldowns.emplace_back(placed_at, gamePacket.punch);
                        else
                            cooldown->last_used = placed_at;
                        world->save_provider_cooldowns();
                    }
                    // Providers need a tile-update packet so the client receives
                    // their tile type and renders the placed block correctly.
                    tile_update = true;
                    break;
                }
            }
            block.state[2] &= ~S_LEFT;
            if ((item.property & 0x01) && pPeer->facing_left)
                block.state[2] |= S_LEFT;
            if (item.type == type::BACKGROUND)
            {
                block.bg = gamePacket.id;
                block.hits[1] = 0;
                block.last_hit[1] = 0;
            }
            else
            {
                block.fg = gamePacket.id;
                block.hits[0] = 0;
                block.last_hit[0] = 0;
            }
            pPeer->emplace(::slot(item.id, -1));
            event_bus::emit({ event_bus::type::block_placed, event.peer, {}, item.id, 1 }); // @note quests & achievements
        }
        gamePacket.netid = pPeer->netid; // @todo sometimes rgt has this as 0
        state_visuals(*event.peer, std::move(gamePacket)); // finished.
        if (tile_update) 
        {
            send_tile_update(event, std::move(gamePacket), block, *world);
        }
        else
        {
            // Special blocks without tile-specific data still need their
            // placement saved when no tile-update packet is sent.
            world->save_blocks();
            world->save_metadata();
        }
        if (lock_visuals) 
        {
            state_visuals(*event.peer, ::gamePacket{
                .type = 0x0f, // @note PACKET_SEND_LOCK
                .netid = world->owner, 
                .state = state::S_EXTENDED, 
                .id = gamePacket.id,
                .punch = gamePacket.punch
            });
        }
    }
    catch (const std::exception& exc)
    {
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, exc.what(), 0u, 1u });
        return;
    }
}
