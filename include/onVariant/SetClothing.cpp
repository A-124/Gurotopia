#include "pch.hpp"
#include "SetClothing.hpp"
#include "commands/useitems.hpp"

/* @note /invis: the game draws only eyes and mouth (state flag 0x4) and hides the clothes sent here.
 * /ghost keeps see-through skin instead, since the game would tint the face purple otherwise.
 * The invisible marker itself stays on the server. */

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
    const bool invis = (subject.state & S_INVISIBLE) == S_INVISIBLE;
    const bool ghost = (subject.state & S_GHOST) != 0;
    auto worn = [&](auto slot) { auto v = subject.clothing[slot]; if (invis) v = 0; return v; };

    u_int skin = item_skin(subject);
    if (invis)                                                          skin = ghost ? 0xFFFFFF00u : 0xFFFFFFFFu;
    else if (ghost)                                                     skin = static_cast<u_int>(-140);
    else if (subject.frozen || (subject.state & S_FROZEN))              skin = 0xFFB47880u;

    send_varlist(&recipient, {
        "OnSetClothing", 
        CL_Vec3f{worn(clothing::HAIR), worn(clothing::SHIRT), worn(clothing::LEGS)}, 
        CL_Vec3f{worn(clothing::FEET), worn(clothing::FACE), worn(clothing::HAND)}, 
        CL_Vec3f{worn(clothing::BACK), worn(clothing::HEAD), worn(clothing::CHARM)}, 
        skin,
        CL_Vec3f{worn(clothing::ANCES), 0.0f, 0.0f}
    }, subject.netid);

    const bool wizard_wand = subject.clothing[clothing::HAND] == 12412;
    ::gamePacket gamePacket {
        .type = 0x14 | ((0x808000 + (wizard_wand ? 0x200000 : 0) + subject.punch_effect) << 8), // @note 0x8080{}14 - PACKET_SET_CHARACTER_STATE
        .netid = subject.netid,
        .count = 125.0f, // @note gtnoob has this as 'waterspeed'
        .id = (subject.state & ~S_DOUBLE_JUMP & ~S_INVISIBLE) | ((invis && !ghost) ? 0x4 : 0) | double_jump_state(subject) | ((subject.curse_until > std::time(nullptr)) ? 0x3000 : 0) | item_effect_state(subject), // @note 04 invisible (only eyes/mouth), 08 no arms, 16 no face, 32 invisible (only legs/arms), 64 devil horns, 128 angel halo, 2048 frozen, 4096 gray skin?,8192 ducttape, 16384 Onion effect, 32768 stars effect, 65536 zombie, 131072 hit by lava, 262144 shadow effect, 524288 irradiated effect, 1048576 spotlight, 2097152 pineapple thingy
        .pos = ::pos{ 1200.0f, 200.0f }, // @todo magic numbers
        .speed = item_speed(subject),
        .punch = ::pos{ subject.hair_color, 0x00000000u } // @todo can this even be unsigned?
    };
    send_data(recipient, compress_state(gamePacket));
}

/* minimal version for bots: default clothing, only needs a netid */
void on::SetClothing(ENetPeer &to, int netid, u_int skin, int state, const std::array<float, 10ull> *cloth)
{
    send_varlist(&to, {
        "OnSetClothing",
        CL_Vec3f{cloth ? (*cloth)[0] : 0.0f, cloth ? (*cloth)[1] : 0.0f, cloth ? (*cloth)[2] : 0.0f},
        CL_Vec3f{cloth ? (*cloth)[3] : 0.0f, cloth ? (*cloth)[4] : 0.0f, cloth ? (*cloth)[5] : 0.0f},
        CL_Vec3f{cloth ? (*cloth)[6] : 0.0f, cloth ? (*cloth)[7] : 0.0f, cloth ? (*cloth)[8] : 0.0f},
        (state & S_GHOST) ? (u_int)(-140) : skin,
        CL_Vec3f{cloth ? (*cloth)[9] : 0.0f, 0.0f, 0.0f}
    }, netid);

    ::gamePacket gp{
        .type = 0x14 | (0x808000 << 8),
        .netid = netid,
        .count = 125.0f,
        .id = state,
        .pos = ::pos{ 1200.0f, 200.0f },
        .speed = ::pos{ 250.0f, 1000.0f },
        .punch = ::pos{ 0.0f, 0.0f }
    };
    send_data(to, compress_state(gp));
}
