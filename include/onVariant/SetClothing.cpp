#include "pch.hpp"
#include "SetClothing.hpp"

void on::SetClothing(ENetPeer &peer)
{
    ::peer *pPeer = static_cast<::peer*>(peer.data);

    SetClothing(peer, *pPeer);
    state_visuals(peer, ::gamePacket{
        .type = 0x14 | ((0x808000 + pPeer->punch_effect) << 8),
        .netid = pPeer->netid,
        .count = 125.0f,
        .id = pPeer->state,
        .pos = ::pos{ 1200.0f, 200.0f },
        .speed = ::pos{ 250.0f, 1000.0f },
        .punch = ::pos{ pPeer->hair_color, 0x00000000u }
    });
}

void on::SetClothing(ENetPeer &recipient, const ::peer &subject)
{

    send_varlist(&recipient, {
        "OnSetClothing", 
        CL_Vec3f{subject.clothing[clothing::HAIR], subject.clothing[clothing::SHIRT], subject.clothing[clothing::LEGS]}, 
        CL_Vec3f{subject.clothing[clothing::FEET], subject.clothing[clothing::FACE], subject.clothing[clothing::HAND]}, 
        CL_Vec3f{subject.clothing[clothing::BACK], subject.clothing[clothing::HEAD], subject.clothing[clothing::CHARM]}, 
        (subject.state & S_GHOST) ? -140 : subject.skin_color,
        CL_Vec3f{subject.clothing[clothing::ANCES], 0.0f, 0.0f}
    }, subject.netid);

    ::gamePacket gamePacket {
        .type = 0x14 | ((0x808000 + subject.punch_effect) << 8), // @note 0x8080{}14 - PACKET_SET_CHARACTER_STATE
        .netid = subject.netid,
        .count = 125.0f, // @note gtnoob has this as 'waterspeed'
        .id = subject.state, // @note 04 invisible (only eyes/mouth), 08 no arms, 16 no face, 32 invisible (only legs/arms), 64 devil horns, 128 angel halo, 2048 frozen, 4096 gray skin?,8192 ducttape, 16384 Onion effect, 32768 stars effect, 65536 zombie, 131072 hit by lava, 262144 shadow effect, 524288 irradiated effect, 1048576 spotlight, 2097152 pineapple thingy
        .pos = ::pos{ 1200.0f, 200.0f }, // @todo magic numbers
        .speed = ::pos{ 250.0f, 1000.0f }, // @todo magic numbers
        .punch = ::pos{ subject.hair_color, 0x00000000u } // @todo can this even be unsigned?
        
    };
    send_data(recipient, compress_state(gamePacket));
}
