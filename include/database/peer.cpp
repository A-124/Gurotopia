#include "pch.hpp"

#include <cctype>
#include <cstring>

#include "items.hpp"
#include "world.hpp"
#include "onVariant/SetClothing.hpp"
#include "onVariant/CountryState.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "commands/punch.hpp"
#include "tools/string.hpp"
#include "commands/event_manager.hpp"

#include "peer.hpp"

bool peer::exists(const std::string &growid)
{
    ::hStmt hStmt{ "SELECT 1 FROM peer WHERE growid = ? LIMIT 1" };

    MYSQL_BIND param = make_bind_in(growid); // WHERE
    hStmt.bind_param(&param);
    hStmt.execute();

    return (!mysql_stmt_store_result(hStmt.pStmt) && mysql_stmt_num_rows(hStmt.pStmt) > 0);
}

::blob slot::to_blob() const
{
    ::blob blob;
    blob.i16(this->id);
    blob.i16(this->count);

    return blob;
}

template<typename T>
void peer::mysql_insert(const std::string &column, const T &value)
{
    ::hStmt hStmt{ std::format("INSERT INTO peer ({}) VALUES (?)", column).c_str() };

    MYSQL_BIND param = make_bind_in(value); // VALUES
    hStmt.bind_param(&param);
    hStmt.execute();
}
template void peer::mysql_insert<signed>(const std::string&, const signed&);
template void peer::mysql_insert<unsigned>(const std::string&, const unsigned&);
template void peer::mysql_insert<float>(const std::string&, const float&);
template void peer::mysql_insert<std::string>(const std::string&, const std::string&);

template<typename T>
void peer::mysql_update(const std::string &column, const T &value)
{
    ::hStmt hStmt{ std::format("UPDATE peer SET {} = ? WHERE growid = ?", column).c_str() };

    MYSQL_BIND params[2] = {
        make_bind_in(value),       // SET
        make_bind_in(this->growid) // WHERE
    };
    hStmt.bind_param(params);
    hStmt.execute();
}
template void peer::mysql_update<signed>(const std::string&, const signed&);
template void peer::mysql_update<unsigned>(const std::string&, const unsigned&);
template void peer::mysql_update<float>(const std::string&, const float&);
template void peer::mysql_update<std::string>(const std::string&, const std::string&);
template void peer::mysql_update<std::vector<u_char>>(const std::string&, const std::vector<u_char>&);

template<typename T>
T peer::mysql_select(const std::string &column, const std::string &arg)
{
    T value{};
    ::hStmt hStmt{ std::format("SELECT {}({}) FROM peer WHERE growid = ? LIMIT 1", arg, column).c_str() };

    MYSQL_BIND param = make_bind_in(this->growid); // WHERE
    hStmt.bind_param(&param);

    u_long length = 0;
    MYSQL_BIND result = make_bind_out(value);
    result.length = &length;
    mysql_stmt_bind_result(hStmt.pStmt, &result);

    hStmt.execute();
    hStmt.fetch();
    if constexpr (std::is_same_v<T, std::string>)
        value.resize(length);

    return value;
}
/* since we will only select during mysql_select_all */ // @note add templates here if use select outside of this file.

void peer::mysql_select_all()
{
    this->user_id    = this->mysql_select<signed>("uid");
    this->growid     = this->mysql_select<std::string>("growid");
    this->password   = this->mysql_select<std::string>("password");
    this->created_at = this->mysql_select<std::time_t>("created_at", "UNIX_TIMESTAMP");
    this->role = static_cast<u_char>(this->mysql_select<unsigned>("role"));
    this->gems = this->mysql_select<signed>("gems");
    this->level[0] = this->mysql_select<unsigned>("level");
    this->level[1] = this->mysql_select<unsigned>("xp");
    const auto saved_clothing = this->mysql_select<std::vector<u_char>>("clothing");
    if (saved_clothing.size() >= sizeof(this->clothing))
        memcpy(this->clothing.data(), saved_clothing.data(), sizeof(this->clothing));
    this->skin_color = this->mysql_select<unsigned>("skin_color");
    this->hair_color = this->mysql_select<unsigned>("hair_color");
    this->banned = this->mysql_select<unsigned>("banned") != 0;
    this->muted_until = this->mysql_select<unsigned>("muted_until");
    this->update_effects();
    auto blob = this->mysql_select<std::vector<u_char>>("inventory");
    const u_char *u8 = blob.data();

    int pos{};
    memcpy(&this->slot_size, u8 + pos, sizeof(int)); pos += sizeof(int);
    short size{};
    memcpy(&size, u8 + pos, sizeof(short)); pos += sizeof(short);
    this->slots.resize(size);
    for (::slot &slot : this->slots)
    {
        memcpy(&slot.id,    u8 + pos, sizeof(short)); pos += sizeof(short);
        memcpy(&slot.count, u8 + pos, sizeof(short)); pos += sizeof(short);
    }
}

::blob peer::serialize_inventory() const
{
    ::blob blob{};
    blob.i32(this->slot_size);
    blob.i16(this->slots.size());
    for (const ::slot &slot : this->slots)
    {
        blob.push_back(slot.to_blob());
    }
    return blob;
}

void peer::load(const std::string &growid, const std::string &password)
{
    if (!this->exists(growid)) 
    {
        this->mysql_insert("growid", growid);
        this->mysql_update("password", password);

        this->slots.resize(3ull); // @note since it's pre-determined we don't need do peer::emplace, and less iteration
        this->slots[0ull] = ::slot{18, 1};   // @note Fist
        this->slots[1ull] = ::slot{32, 1};   // @note Wrench
        this->slots[2ull] = ::slot{9640, 1}; // @note My First World Lock
        this->mysql_update<std::vector<u_char>>("inventory", this->serialize_inventory().data());
    }
    this->mysql_select_all();
}

void peer::update_display_growid()
{
    switch (this->role)
    {
        case MODERATOR:
            this->display_growid = std::format("`c{}``", this->growid);
            break;
        case DEVELOPER:
            this->display_growid = std::format("`6{}``", this->growid);
            break;
        default:
            this->display_growid = this->growid;
            break;
    }
}

void peer::save_inventory()
{
    this->mysql_update<std::vector<u_char>>("inventory", this->serialize_inventory().data());
}

void peer::save_progress()
{
    const unsigned saved_level = this->level[0];
    const unsigned saved_xp = this->level[1];
    this->mysql_update<unsigned>("level", saved_level);
    this->mysql_update<unsigned>("xp", saved_xp);
}

void peer::save_clothing()
{
    std::vector<u_char> saved_clothing(sizeof(this->clothing));
    memcpy(saved_clothing.data(), this->clothing.data(), sizeof(this->clothing));
    this->mysql_update<std::vector<u_char>>("clothing", saved_clothing);
    this->mysql_update<unsigned>("skin_color", this->skin_color);
    this->mysql_update<unsigned>("hair_color", this->hair_color);
}

void peer::save_moderation()
{
    this->mysql_update<signed>("banned", this->banned ? 1 : 0);
    this->mysql_update<unsigned>("muted_until", this->muted_until);
}

peer::~peer()
{
    if (!this->growid.empty())
    {
        this->save_inventory();
        this->save_progress();
        this->save_clothing();
    }
}

u_short peer::emplace(::slot slot) 
{
    if (auto it = std::ranges::find(this->slots, slot.id, &::slot::id); it != this->slots.end()) 
    {
        const u_short excess = std::max(0, (it->count + slot.count) - 200);
        it->count = std::min(it->count + slot.count, 200);
        if (it->count == 0)
        {
            const ::item &item = id_to_item(it->id);
            if (item.cloth_type != clothing::NONE)
            {
                this->clothing[item.cloth_type] = 0;
                this->update_effects();
                this->save_clothing();
            }
        }
        this->save_inventory();
        return excess;
    }
    else
    {
        this->slots.emplace_back(std::move(slot)); // @note no such item in inventory, so we create a new entry.
        this->save_inventory();
    }
    return 0;
}

void peer::add_xp(ENetEvent &event, u_short value) 
{
    value = static_cast<u_short>(value * get_xp_multiplier());
    u_int &lvl = this->level.front();
    u_int &xp = this->level.back() += value; // @note factor the new xp amount

    for (; lvl < 125; )
    {
        u_int xp_formula = 50u * (lvl * lvl + 2u); // @author https://www.growtopiagame.com/forums/member/553046-kasete
        if (xp < xp_formula) break;

        xp -= xp_formula;
        lvl++;

        if (lvl == 50) 
        {
            modify_item_inventory(event, ::slot{1400, 1}); // @note Mini Growtopian
            /* @todo based on account age give peer other items... */
        }
        if (lvl == 125) on::CountryState(event);
        send_varlist(event.peer, { "OnPlayerLeveledUp", lvl });
        send_varlist(event.peer, { "OnParticleEffect", 46u, CL_Vec2f{1812.0f, 1724.0f}, 0.0f, 0.0f });

        std::string message = std::format("{} is now level {}!", this->display_growid, lvl);
        send_varlist(event.peer, { "OnTalkBubble", this->netid, message, 0u });
        on::ConsoleMessage(event.peer, message);
    }

    this->save_progress();
}

void peer::update_effects()
{
    this->punch_effect = 0;
    bool has_double_jump = false;
    for (float cloth : this->clothing)
    {
        const u_short item_id = static_cast<u_short>(cloth);
        u_char punch_id = get_punch_id(item_id);
        if (punch_id != 0) // @note an actual change rather than no effect.
            this->punch_effect = punch_id;

        if (item_id != 0)
        {
            const ::item &equipped = id_to_item(item_id);
            if (equipped.cloth_type == clothing::BACK)
            {
                std::string name = equipped.raw_name;
                std::ranges::transform(name, name.begin(), [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
                if (name.find("wing") != std::string::npos)
                    has_double_jump = true;
            }
        }
    }

    if (has_double_jump) this->state |= S_DOUBLE_JUMP;
    else this->state &= ~S_DOUBLE_JUMP;
}

ENetHost *host;

std::vector<ENetPeer*> peers(const std::string &world, peer_condition condition, std::function<void(ENetPeer&)> fun)
{
    std::vector<ENetPeer*> _peers{};
    _peers.reserve(host->peerCount);

    for (ENetPeer &peer : std::span(host->peers, host->peerCount))
        if (peer.state == ENET_PEER_STATE_CONNECTED)
        {
            ::peer *pOthers = static_cast<::peer*>(peer.data);
            if (!pOthers) continue; // @note slot connected but login not finished yet
            if (condition == peer_condition::PEER_SAME_WORLD)
            {
                if (pOthers->netid == 0 || (pOthers->recent_worlds.back() != world)) continue;
            }
            fun(peer);
            _peers.push_back(&peer);
        }

    return _peers;
}

void safe_disconnect_peers(int code)
{
    peers("", peer_condition::PEER_ALL, [](ENetPeer &p)
    {
        if (p.data != nullptr)
        {
            ::peer *pPeer = static_cast<::peer*>(p.data);
            if (!pPeer->growid.empty()) pPeer->save_inventory();
        }
        enet_peer_disconnect(&p, 0);
    });
    enet_host_flush(host);
    
    enet_host_destroy(host);
    host = nullptr; // @todo clean this up better
    enet_deinitialize();
}

gamePacket make_gamePacket(const enet_uint8 *data) 
{
    const int   *i32   = reinterpret_cast<const int*>(data);
    const u_int *u32 = reinterpret_cast<const u_int*>(data);
    const float *f32 = reinterpret_cast<const float*>(data);

    return gamePacket{
        .type  = i32[1],
        .netid = i32[2],
        .uid   = i32[3],
        .state = i32[4],
        .count = f32[5],
        .id    = i32[6],
        .pos   = ::pos{f32[7], f32[8]},
        .speed = ::pos{f32[9], f32[10]},
        .idk   = f32[11],
        .punch = ::pos{i32[12], i32[13]},
        .size  = u32[14]
    };
}

::blob compress_state(const gamePacket &gamePacket) 
{
    ::blob blob{};
    
    blob.i32(gamePacket.packet_create);
    blob.i32(gamePacket.type);
    blob.i32(gamePacket.netid);
    blob.i32(gamePacket.uid);
    blob.i32(gamePacket.state);
    blob.f32(gamePacket.count);
    blob.i32(gamePacket.id);
    blob.f32(gamePacket.pos.x);
    blob.f32(gamePacket.pos.y);
    blob.f32(gamePacket.speed.x);
    blob.f32(gamePacket.speed.y);
    blob.f32(gamePacket.idk);
    blob.i32(gamePacket.punch.x);
    blob.i32(gamePacket.punch.y);
    blob.i32(gamePacket.size);

    return blob;
}

void send_inventory_state(ENetPeer &enet_peer)
{
    ::peer *pPeer = static_cast<::peer*>(enet_peer.data);
    if (!pPeer) return;

    ::blob blob = compress_state(::gamePacket{
        .type = 0x09, // @note PACKET_SEND_INVENTORY_STATE
        .netid = pPeer->netid,
        .state = state::S_EXTENDED
    });
    blob.u8(0x01); // @note enable flag for big backpack

    blob.push_back(pPeer->serialize_inventory());

    ENetPacket *packet = enet_packet_create(blob.data().data(), blob.size(), ENET_PACKET_FLAG_RELIABLE);
	if (enet_peer_send(&enet_peer, 0, packet)) enet_packet_destroy(packet);
}

void send_inventory_state(ENetEvent &event)
{
    if (event.peer) send_inventory_state(*event.peer);
}
