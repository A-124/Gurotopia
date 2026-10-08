#include "pch.hpp"
#include "action/__action.hpp"
#include "gamePacket/_gamePacket.hpp"
#include "receive.hpp"

void receive(ENetEvent& event) 
{
    std::span<const enet_uint8> data{event.packet->data, event.packet->dataLength};
    if (data.empty() || event.peer == nullptr || event.peer->data == nullptr) // @note empty packet, or peer already quit (data deleted)
    {
        enet_packet_destroy(event.packet);
        return;
    }
    switch (data[0ull]) 
    {
        case 2: case 3: 
        {
            if (data.size() < 5) break; // @note need 4 byte type + at least 1 byte before the terminator
            std::string header{data.begin() + 4, data.end() - 1};
            // @note never log login packets, they contain the player's password.
            if (!header.starts_with("tankIDName") && !header.starts_with("requestedName") && header.find("tankIDPass") == std::string::npos)
                puts(header.c_str());
            
            std::ranges::replace(header, '\n', '|');
            const std::vector<std::string> pipes = readch(header, '|');
            if (pipes.size() < 2) break;
            
            std::string action{};
            if (pipes[0ull] == "protocol" || pipes[0ull] == "tankIDName")
            {
                action = pipes[0ull];
            }
            else action = std::format("{}|{}", pipes[0ull], pipes[1ull]);

            if (const auto i = action_pool.find(action); i != action_pool.end())
                i->second(event, header);
            break;
        }
        case 4: 
        {
            if (event.packet->dataLength < sizeof(::gamePacket)) break;

            ::gamePacket gamePacket = make_gamePacket(event.packet->data);
            gamePacket.size = event.packet->dataLength - sizeof(::gamePacket); // @todo did i do this right? or check for flag ::EXTENDED

            if (const auto i = gamePacket_pool.find(gamePacket.type); i != gamePacket_pool.end())
                i->second(event, std::move(gamePacket));
            break;
        }
        default: printf("received unqiue: %d\n", data[0ull]);
    }
    enet_packet_destroy(event.packet);
}
