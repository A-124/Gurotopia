/*
    @copyright gurotopia (c) 2024-05-25
    @version parent SHA: 8f2b5977fd057b99e2e2a6f0e65170145decc159 2026-9-30
*/
#include "include/pch.hpp"
#include "include/eventType/_eventType.hpp"

#include "include/database/shouhin.hpp" // @note init_shouhin_tachi()
#include "include/https/https.hpp" // @note https::listener()
#include "include/https/server_data.hpp" // @note gServer_data
#include "include/database/database_config.hpp" // @note load_database_config(), gDatabase_config
#include "include/automate/holiday.hpp" // @note holiday
#include <csignal>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include "include/commands/event_manager.hpp" // @note event_manager_tick()
#include "include/core/event_bus.hpp"
#include "include/core/runtime_reload.hpp"
#include "include/gameplay/quest_system.hpp"
#include "include/gameplay/achievement_system.hpp"
#include "include/gameplay/title_system.hpp"

namespace
{
    volatile std::sig_atomic_t gSignal = 0;
}
static void signal_handler(int signal) { gSignal = signal; }

int main()
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler); // @note `docker stop` and systemd send SIGTERM: without this worlds were not saved on shutdown
    std::setvbuf(stdout, nullptr, _IOLBF, 0); // @note line-buffered so `docker logs` shows output immediately
#ifdef SIGHUP // @note unix
    std::signal(SIGHUP, signal_handler); // @note PuTTY, SSH problems
#endif

    std::srand(static_cast<unsigned>(std::time(nullptr))); // @note RandomRange() uses rand(); unseeded, every restart replayed the same drops

    mysql_library_init(0, NULL, NULL);
    enet_initialize();
    {
        gServer_data.init(); // @note ./server_data.php

        // Bind to 0.0.0.0 (all network interfaces) so LAN devices can connect.
        // ENetAddress{} zero-initializes every field, so host defaults to 0
        // which is equivalent to ENET_HOST_ANY (listen on all interfaces).
        ENetAddress address{};
        address.type = ENET_ADDRESS_TYPE_IPV4;
        address.port = gServer_data.port;

        std::size_t max_peers = 50; // @note override with GURO_MAX_PEERS
        if (const char *v = std::getenv("GURO_MAX_PEERS"); v && *v)
            max_peers = static_cast<std::size_t>(std::clamp(std::strtol(v, nullptr, 10), 1l, 1024l));

        host = enet_host_create(ENET_ADDRESS_TYPE_IPV4, &address, max_peers, 2ull, 0u, 0u);
        if (!host)
        {
            std::fprintf(stderr, "could not open game port %hu (is another server using it?)\n", gServer_data.port);
            return EXIT_FAILURE;
        }
        std::thread(&https::listener).detach();
    } // @note delete address
    host->usingNewPacketForServer = true;
    host->checksum = enet_crc32;
    enet_host_compress_with_range_coder(host);

    gDb_config.init();
    mysql_connect();
    decode_items();      // @note reads items.dat into legible class members (id, item name, ect)
    parse_store();       // @todo thread loop this so the store can update without restarting server (stored in .\resource\store.txt)
    check_for_holiday(); // @note check for any holidays using local time (your VPS or local time)
    runtime_reload::reload("content");
    event_bus::subscribe(event_bus::type::item_changed, quest_system::on_event);
    event_bus::subscribe(event_bus::type::block_changed, quest_system::on_event);
    event_bus::subscribe(event_bus::type::block_placed, quest_system::on_event);
    event_bus::subscribe(event_bus::type::player_entered_world, quest_system::on_event);
    event_bus::subscribe(event_bus::type::item_changed, achievement_system::on_event);
    event_bus::subscribe(event_bus::type::block_changed, achievement_system::on_event);
    event_bus::subscribe(event_bus::type::block_placed, achievement_system::on_event);
    event_bus::subscribe(event_bus::type::player_entered_world, achievement_system::on_event);

    event_bus::subscribe(event_bus::type::tick, [](const event_bus::event&) { title_system::on_tick(); });

    std::printf("gurotopia is ready: game port %hu/udp, %zu max players\n", gServer_data.port, static_cast<std::size_t>(host->peerCount));

    ENetEvent event{};
    std::time_t next_db_ping = 0;
    while (!gSignal)
    {
        if (const std::time_t now = std::time(nullptr); now >= next_db_ping)
        {
            next_db_ping = now + 60;
            mysql_ping(db); // @note keeps the connection alive and reconnects if MariaDB restarted
        }
        event_manager_tick(); // @note check if gem/xp multiplier event has expired
        event_bus::emit({ event_bus::type::tick });

        while (enet_host_service(host, &event, 1000/*ms*/) > 0)
            if (const auto i = eventType_pool.find(event.type); i != eventType_pool.end())
                i->second(event);
    }

    std::printf("shutting down (signal %d): saving players and worlds...\n", static_cast<int>(gSignal));
    safe_disconnect_peers(gSignal);
    worlds.clear(); // @note ~world() saves every loaded world; it must happen before the database closes (otherwise players' builds, drops and weather are lost on shutdown)
    mysql_close(db); // @note deletes db (MYSQL* allocation)
    mysql_library_end();

    puts("killed gurotopia! [EXIT_SUCCESS]");
    return EXIT_SUCCESS;
}