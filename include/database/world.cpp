#include "pch.hpp"
#include <cstring>
#include <ctime>
#include "tools/random.hpp"
#include "tools/time.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "commands/weather.hpp"
#include "core/event_bus.hpp"
#include "custom_content.hpp"

#include "world.hpp"

char get_type(const ::item &item)
{
    switch (item.type)
    {
        case type::MAIN_DOOR: case type::DOOR: case type::PORTAL: return '\x01';
        case type::SIGN: return '\x02';
        case type::LOCK: return '\x03';
        case type::SEED: return '\x04';
        case type::RANDOM: return '\x08';
        case type::PROVIDER: return '\x09';
        case type::DISPLAY_BLOCK: return '\x17';
        case type::VENDING_MACHINE: return '\x18';
    }
    return '\x00';
}

namespace
{
    u_int provider_elapsed_seconds(const ::world &world, const ::pos &tile_pos)
    {
        const auto cooldown = std::ranges::find(world.provider_cooldowns, tile_pos, &::provider_cooldown::pos);
        if (cooldown == world.provider_cooldowns.end()) return 0;

        const u_int now = static_cast<u_int>(std::time(nullptr));
        return now >= cooldown->last_used ? now - cooldown->last_used : 0u;
    }
}

bool is_weather_machine(const ::item &item) noexcept
{
    return item.type == type::WEATHER_MACHINE ||
           item.type == type::SFX_WEATHER_MACHINE ||
           item.type == type::SPRITE_WEATHER_MACHINE;
}

int world::weather_id() const
{
    const int x = this->weather.x_int(), y = this->weather.y_int();
    if (x < 0 || x >= 100 || y < 0 || y >= 60) return 0;
    if (static_cast<std::size_t>(cord(x, y)) >= this->blocks.size()) return 0;

    const ::block &machine = this->blocks[cord(x, y)];
    if (machine.fg == 0 || !(machine.state[2] & S_TOGGLE)) return 0;

    const ::item &item = id_to_item(machine.fg);
    return is_weather_machine(item) ? get_weather_id(item.id) : 0;
}

void block::reset()
{
    this->fg = 0;
    this->bg = 0;
    for (u_char &s : this->state) s = 0;
    for (u_char &hit : this->hits) hit = 0;
    for (u_int &last_hit : this->last_hit) last_hit = 0;
}

::blob block::to_blob() const
{
    blob blob;
    blob.i16(this->fg);
    blob.i16(this->bg);
    blob.u8(this->state[0]);
    blob.u8(this->state[1]);
    blob.u8(this->state[2]);
    blob.u8(this->state[3]);

    return blob;
}

::blob object::to_blob() const
{
    blob blob;
    blob.i16(this->id);
    blob.f32(this->pos.x);
    blob.f32(this->pos.y);
    blob.i16(this->count);
    blob.i32(this->uid);

    return blob;
}

::blob door::to_blob() const
{
    blob blob;
    blob.i16(this->label.length());
    for (char c : this->label) blob.u8(c);
    blob.u8('\0');

    return blob;
}

::blob sign::to_blob() const
{
    blob blob;
    blob.i16(this->label.length());
    for (char c : this->label) blob.u8(c);
    blob.u32(this->idk);

    return blob;
}

::blob tree::to_blob(bool seconds) const
{
    blob blob;
    const u_int now = static_cast<u_int>(std::time(nullptr));
    blob.i32((seconds) ? ((now >= this->tick) ? now - this->tick : 0u) : this->tick);
    blob.u8(this->fruit);

    return blob;
}

::blob world::serialize()
{
    blob blob;
    blob.i16(0x00); // @todo my rgt world says: 19 00
    blob.i32(0x00); // @todo my rgt world says: 40 00 00 00
    blob.i16(this->name.length());
    for (char c : this->name) blob.u8(c);

    const int y = this->blocks.size() / 100;
    const int x = this->blocks.size() / y;
    blob.i32(x);
    blob.i32(y);
    blob.i16(this->blocks.size());

    /*@todo*/
    blob.i32(0x00);
    blob.i16(0x00);
    blob.u8(0x00);

    for (u_short i = 0; const ::block &block : this->blocks)
    {
        blob.push_back(block.to_blob());

        if (block.fg != 0 || block.fg!=2||block.fg!=4||block.fg!=8||block.fg!=14) // @note so we can save time
        if (char type = get_type(id_to_item(block.fg)); type > '\x00')
        {
            blob.u8(type);

            const ::pos block_pos{i % x, i / x};
            if (type == '\x01'/*doors, portal*/)
            {
                if (block.fg == 6/*Main Door*/) this->spawn = block_pos.by_32(false);

                auto door = std::ranges::find(this->doors, block_pos, &::door::pos);
                if (door != this->doors.end())
                {
                    blob.push_back(door->to_blob());
                }
            }
            else if (type == '\x02'/*sign*/)
            {
                auto sign = std::ranges::find(this->signs, block_pos, &::sign::pos);
                if (sign != this->signs.end())
                {
                    blob.push_back(sign->to_blob());
                }
            }
            else if (type == '\x03'/*lock*/)
            {
                if (!is_tile_lock(block.fg)) this->is_public = (block.state[2] & S_PUBLIC); // @note check if world lock has S_PUBLIC flag, i will change this later
                int access = std::ranges::count_if(this->access, std::identity{});
                
                blob.u8(this->lock_state);
                blob.i32(this->owner);
                blob.i32(access);
                for (int user_id : this->access)
                    if (user_id != 0) blob.i32(user_id);
            }
            else if (type == '\x17'/*display block*/)
            {
                const auto display = std::ranges::find(this->displays, block_pos, &::display::pos);
                blob.u32(display == this->displays.end() ? 0u : display->id);
            }
            else if (type == '\x04'/*seed*/)
            {
                auto tree = std::ranges::find(this->trees, block_pos, &::tree::pos);
                if (tree != this->trees.end())
                {
                    blob.push_back(tree->to_blob(true));
                }
            }
            else if (type == '\x09'/*provider*/)
            {
                // Provider tile extras carry elapsed growth time after the type byte.
                blob.u32(provider_elapsed_seconds(*this, block_pos));
            }
            else if (type == '\x18'/*vending machine*/)
            {
                const auto machine = std::ranges::find(this->vending_machines, block_pos, &::vending_machine_state::pos);
                // Retail map data carries item ID and signed World Lock price.
                blob.i32(machine == this->vending_machines.end() ? 0 : machine->item_id);
                blob.i32(machine == this->vending_machines.end() || machine->legacy_gem_currency ? 0 : machine->price);
            }
        }
        ++i;
    }
    /*@todo*/
    blob.i32(0x00);
    blob.i32(0x00);
    blob.i32(0x00);

    blob.i32(static_cast<int>(this->objects.size())); // @note number of drops
    blob.i32(this->last_object_uid);
    for (const ::object &object : this->objects) 
    {
        blob.push_back(object.to_blob());
    }
    return blob;
}

bool world::exists(const std::string& name)
{
    ::hStmt hStmt{ "SELECT 1 FROM world WHERE name = ? LIMIT 1" };

    MYSQL_BIND param = make_bind_in(name); // WHERE
    hStmt.bind_param(&param);
    hStmt.execute();

    return (!mysql_stmt_store_result(hStmt.pStmt) && mysql_stmt_num_rows(hStmt.pStmt) > 0);
}

template<typename T>
void world::mysql_insert(const std::string& column, const T& value)
{
    ::hStmt hStmt{ std::format("INSERT INTO world ({}) VALUES (?)", column).c_str() };

    MYSQL_BIND param = make_bind_in(value); // VALUES
    hStmt.bind_param(&param);
    hStmt.execute();
}
template void world::mysql_insert<signed>(const std::string&, const signed&);
template void world::mysql_insert<unsigned>(const std::string&, const unsigned&);
template void world::mysql_insert<float>(const std::string&, const float&);
template void world::mysql_insert<std::string>(const std::string&, const std::string&);
template void world::mysql_insert<::blob>(const std::string&, const ::blob&);

template<typename T>
void world::mysql_update(const std::string& column, const T& value)
{
    ::hStmt hStmt{ std::format("UPDATE world SET {} = ? WHERE name = ?", column).c_str() };

    MYSQL_BIND params[2] = {
        make_bind_in(value),      // SET
        make_bind_in(this->name) // WHERE
    };
    hStmt.bind_param(params);
    hStmt.execute();
}
template void world::mysql_update<signed>(const std::string&, const signed&);
template void world::mysql_update<unsigned>(const std::string&, const unsigned&);
template void world::mysql_update<float>(const std::string&, const float&);
template void world::mysql_update<std::string>(const std::string&, const std::string&);
template void world::mysql_update<::blob>(const std::string&, const ::blob&);

template<typename T>
T world::mysql_select(const std::string &column, const std::string &arg)
{
    T value{};
    ::hStmt hStmt{ std::format("SELECT {}({}) FROM world WHERE name = ? LIMIT 1", arg, column).c_str() };

    MYSQL_BIND param = make_bind_in(this->name); // WHERE
    hStmt.bind_param(&param);
    hStmt.execute();

    u_long length = 0;
    MYSQL_BIND result = make_bind_out(value);
    result.length = &length;
    mysql_stmt_bind_result(hStmt.pStmt, &result);

    hStmt.execute();
    hStmt.fetch();
    if constexpr (std::is_same_v<T, std::string>)
        value.resize(length);
    else if constexpr (std::is_same_v<T, ::blob> || std::is_same_v<T, std::vector<u_char>>)
        value.resize(length); // @note std::vector<u_char> used to keep the whole 120000 byte bind buffer, which loaded ~7500 ghost drops

    return value;
}

void world::mysql_select_all()
{
    this->name = this->mysql_select<std::string>("name");
    this->owner = this->mysql_select<int>("owner");
    this->minimum_entry_level = static_cast<u_char>(this->mysql_select<unsigned>("minimum_entry_level"));
    this->lock_state = static_cast<u_char>(this->mysql_select<unsigned>("lock_state"));
    this->is_public = this->mysql_select<unsigned>("is_public") != 0;
    {
        const ::blob saved_access = this->mysql_select<::blob>("access");
        int pos{};
        std::size_t index{};
        while (pos + static_cast<int>(sizeof(int)) <= saved_access.size() && index < this->access.size())
        {
            std::memcpy(&this->access[index], saved_access.data().data() + pos, sizeof(int));
            pos += sizeof(int);
            ++index;
        }
    }
    {
        this->provider_cooldowns.clear();
        ::blob saved_cooldowns = this->mysql_select<::blob>("provider_cooldowns");
        int pos{};
        while (pos + static_cast<int>(sizeof(short) * 2 + sizeof(u_int)) <= saved_cooldowns.size())
        {
            short x{}, y{};
            u_int last_used{};
            saved_cooldowns.read_i16(x, pos);
            saved_cooldowns.read_i16(y, pos);
            saved_cooldowns.read_u32(last_used, pos);
            this->provider_cooldowns.emplace_back(last_used, ::pos{x, y});
        }
    }
    {
        this->vending_machines.clear();
        ::blob saved_machines = this->mysql_select<::blob>("vending_machines");
        int pos{};
        constexpr int format_marker = 0x32444e56; // "VND2"
        int marker{};
        bool current_format = false;
        if (saved_machines.size() >= sizeof(marker))
        {
            saved_machines.read_i32(marker, pos);
            current_format = marker == format_marker;
            if (!current_format) pos = 0;
        }
        if (current_format)
        {
            constexpr int record_size = static_cast<int>(sizeof(short) * 4 + sizeof(int) * 3);
            while (pos + record_size <= saved_machines.size())
            {
                short x{}, y{}, item_id{}, stock{};
                int price{}, bank{}, flags{};
                saved_machines.read_i16(x, pos);
                saved_machines.read_i16(y, pos);
                saved_machines.read_i16(item_id, pos);
                saved_machines.read_i16(stock, pos);
                saved_machines.read_i32(price, pos);
                saved_machines.read_i32(bank, pos);
                saved_machines.read_i32(flags, pos);
                this->vending_machines.emplace_back(::pos{x, y}, item_id, stock, price, bank, (flags & 1) != 0);
            }
        }
        else
        {
            constexpr int record_size = static_cast<int>(sizeof(short) * 4 + sizeof(int) * 2);
            while (pos + record_size <= saved_machines.size())
            {
                short x{}, y{}, item_id{}, stock{};
                int price{}, bank{};
                saved_machines.read_i16(x, pos);
                saved_machines.read_i16(y, pos);
                saved_machines.read_i16(item_id, pos);
                saved_machines.read_i16(stock, pos);
                saved_machines.read_i32(price, pos);
                saved_machines.read_i32(bank, pos);
                // Preserve old gem-based balances and stop presenting them as World Locks.
                this->vending_machines.emplace_back(::pos{x, y}, item_id, stock, price, bank, true);
            }
        }
    }
    {
        this->trees.clear();
        this->doors.clear();
        this->signs.clear();
        this->displays.clear();
        ::blob blob = this->mysql_select<::blob>("blocks");
        this->blocks.assign(cord(0, 60), ::block{});

        const int x = this->blocks.size() / 60;
        const int total = static_cast<int>(blob.size());
        bool corrupt = false;
        bool migrated_legacy_pot_gold_block = false;
        const auto* configured_pot_gold = custom_content::find_item(30002);
        const bool migrate_legacy_pot_gold =
            !custom_content::find_item(30001) && configured_pot_gold &&
            configured_pot_gold->block_kind == custom_content::custom_block_kind::pot_gold;
        int pos{};
        const auto has = [&](int bytes) { return bytes >= 0 && pos + bytes <= total; };
        const auto has_string = [&]()
        {
            if (!has(sizeof(short))) return false;
            short len{};
            std::memcpy(&len, blob.data().data() + pos, sizeof(short));
            return len >= 0 && has(static_cast<int>(sizeof(short)) + len);
        };

        for (u_short i = 0; ::block &block : this->blocks)
        {
            if (!has(8)) { corrupt = true; break; } // @note empty / truncated save (e.g. crashed while the world was first created)

            blob.read_i16(block.fg, pos);
            blob.read_i16(block.bg, pos);
            if (migrate_legacy_pot_gold)
            {
                if (block.fg == 30001) { block.fg = 30002; migrated_legacy_pot_gold_block = true; }
                if (block.bg == 30001) { block.bg = 30002; migrated_legacy_pot_gold_block = true; }
            }
            blob.read_u8(block.state[0], pos);
            blob.read_u8(block.state[1], pos);
            blob.read_u8(block.state[2], pos);
            blob.read_u8(block.state[3], pos);

            if (block.fg != 0)
            if (char type = get_type(id_to_item(block.fg)); type > '\x00')
            {
                const ::pos block_pos{i % x, i / x};
                if (type == '\x01'/*doors, portal*/)
                {
                    if (!has_string()) { corrupt = true; break; }
                    ::door &door = this->doors.emplace_back("","","", block_pos);

                    blob.read_string(door.label, pos);
                    pos++;// @note \0
                }
                else if (type == '\x02'/*sign*/)
                {
                    if (!has_string()) { corrupt = true; break; }
                    ::sign &sign = this->signs.emplace_back("", block_pos);

                    blob.read_string(sign.label, pos);
                    if (!has(sizeof(u_int))) { corrupt = true; break; }
                    blob.read_u32(sign.idk, pos);
                }
                else if (type == '\x04'/*seed*/)
                {
                    if (!has(sizeof(u_int) + sizeof(u_char))) { corrupt = true; break; }
                    ::tree &tree = this->trees.emplace_back(0, 0, block_pos);

                    u_int age_seconds{};
                    blob.read_u32(age_seconds, pos);
                    const u_int now = static_cast<u_int>(std::time(nullptr));
                    tree.tick = (age_seconds <= now) ? now - age_seconds : 0u;
                    blob.read_u8(tree.fruit, pos);
                }
                else if (type == '\x17'/*display block*/)
                {
                    if (!has(sizeof(u_int))) { corrupt = true; break; }
                    u_int item_id{};
                    blob.read_u32(item_id, pos);
                    if (item_id != 0) this->displays.emplace_back(item_id, block_pos);
                }
            }
            ++i;
        }
        if (corrupt)
        {
            std::fprintf(stderr, "[world] '%s' has an unreadable block save, regenerating it.\n", this->name.c_str());
            this->doors.clear(); this->signs.clear(); this->displays.clear(); this->trees.clear();
        }
        else if (migrated_legacy_pot_gold_block)
        {
            // Persist the migrated block IDs so ID 30001 can safely be used by
            // the future Lucky Box Seed without reinterpreting old Pot Gold blocks.
            this->save_blocks();
        }

        // @note weather is not stored in its own column: the machine's S_TOGGLE flag is saved with the blocks,
        //       so find the active machine again here (and join_request sends it to the player).
        this->weather = {};
        for (int i = 0; i < static_cast<int>(this->blocks.size()); ++i)
        {
            const ::block &block = this->blocks[i];
            if (block.fg == 0 || !(block.state[2] & S_TOGGLE)) continue;
            if (!is_weather_machine(id_to_item(block.fg))) continue;

            this->weather = ::pos{i % x, i / x};
            break;
        }
    } // @note delete blob, i
    {
        const auto blob = this->mysql_select<std::vector<u_char>>("objects");
        const u_char *u8 = blob.data(); // @note i did not have the brain capacity to reinterpret it. t-t (memcpy is safer anyways...)

        this->objects.clear();
        this->last_object_uid = 0;
        bool migrated_legacy_pot_gold_object = false;
        if (blob.size() >= sizeof(u_int))
        {
            std::size_t i{};
            memcpy(&this->last_object_uid, u8, sizeof(u_int));
            i += sizeof(u_int); // @todo real gt has this as 8 bits not just 4.

            constexpr std::size_t record_size = sizeof(u_short) + sizeof(float) * 2 + sizeof(u_short) + sizeof(u_int);
            const std::size_t count = (blob.size() - i) / record_size;
            this->objects.reserve(count);
            for (std::size_t n = 0; n < count; ++n)
            {
                ::object object{};
                memcpy(&object.id,    u8 + i, sizeof(u_short)); i += sizeof(u_short);
                if (migrate_legacy_pot_gold && object.id == 30001)
                {
                    object.id = 30002;
                    migrated_legacy_pot_gold_object = true;
                }
                memcpy(&object.pos.x, u8 + i, sizeof(float));   i += sizeof(float);
                memcpy(&object.pos.y, u8 + i, sizeof(float));   i += sizeof(float);
                memcpy(&object.count, u8 + i, sizeof(u_short)); i += sizeof(u_short);
                memcpy(&object.uid,   u8 + i, sizeof(u_int));   i += sizeof(u_int);

                if (object.id == 0 || object.count == 0) continue; // @note never bring back empty/ghost drops
                this->last_object_uid = std::max(this->last_object_uid, object.uid);
                this->objects.push_back(object);
            }
        }
        if (migrated_legacy_pot_gold_object) this->save_objects();
    } // @note delete blob, i
}

world::world(const std::string &name) : name(name)/*DEFAULT*/
{
    if (this->exists(this->name)) 
    {
        this->mysql_select_all();
    }
    else 
    {
        this->mysql_insert("name", this->name); // @note DEFAULT
        generate_world(*this);
        this->save_blocks();
        this->save_objects();
    }
}

void world::save_metadata()
{
    this->mysql_update("owner", this->owner);
    const unsigned minimum_entry_level = this->minimum_entry_level;
    this->mysql_update<unsigned>("minimum_entry_level", minimum_entry_level);
    const unsigned lock_state = this->lock_state;
    const unsigned is_public = this->is_public ? 1u : 0u;
    this->mysql_update<unsigned>("lock_state", lock_state);
    this->mysql_update<unsigned>("is_public", is_public);
    ::blob saved_access{};
    for (int user_id : this->access)
        if (user_id != 0) saved_access.i32(user_id);
    this->mysql_update<::blob>("access", saved_access);
}

void world::save_blocks()
{
    ::blob blob;
    const int x = this->blocks.size() / 60;
    for (u_short i = 0; const ::block &block : this->blocks)
    {
        blob.push_back(block.to_blob());
        if (block.fg != 0)
        {
            const u_char type = get_type(id_to_item(block.fg));
            if (type > 0x00)
            {
                const ::pos block_pos{i % x, i / x};
                if (type == '\x01')
                {
                    auto door = std::ranges::find(this->doors, block_pos, &::door::pos);
                    if (door != this->doors.end()) blob.push_back(door->to_blob());
                }
                else if (type == '\x02')
                {
                    auto sign = std::ranges::find(this->signs, block_pos, &::sign::pos);
                    if (sign != this->signs.end()) blob.push_back(sign->to_blob());
                }
                else if (type == '\x04')
                {
                    auto tree = std::ranges::find(this->trees, block_pos, &::tree::pos);
                    if (tree != this->trees.end()) blob.push_back(tree->to_blob(true)); // @note saved as age so load (now - age) restores the real plant time
                }
                else if (type == '\x17')
                {
                    const auto display = std::ranges::find(this->displays, block_pos, &::display::pos);
                    blob.u32(display == this->displays.end() ? 0u : display->id);
                }
            }
        }
        ++i;
    }
    this->mysql_update("blocks", blob);
}

void world::save_objects()
{
    ::blob blob{};
    blob.i32(this->last_object_uid);
    for (const ::object &object : this->objects) blob.push_back(object.to_blob());
    this->mysql_update("objects", blob.data());
}

world::~world()
{
    this->save_metadata();
    this->save_provider_cooldowns();
    this->save_vending_machines();
    this->save_blocks();
    this->save_objects();
}

void world::save_provider_cooldowns()
{
    ::blob saved_cooldowns{};
    for (const ::provider_cooldown &cooldown : this->provider_cooldowns)
    {
        saved_cooldowns.i16(static_cast<short>(cooldown.pos.x));
        saved_cooldowns.i16(static_cast<short>(cooldown.pos.y));
        saved_cooldowns.u32(cooldown.last_used);
    }
    this->mysql_update<::blob>("provider_cooldowns", saved_cooldowns);
}

void world::save_vending_machines()
{
    ::blob saved_machines{};
    saved_machines.i32(0x32444e56); // "VND2": versioned records include currency mode
    for (const ::vending_machine_state &machine : this->vending_machines)
    {
        saved_machines.i16(static_cast<short>(machine.pos.x));
        saved_machines.i16(static_cast<short>(machine.pos.y));
        saved_machines.i16(machine.item_id);
        saved_machines.i16(machine.stock);
        saved_machines.i32(machine.price);
        saved_machines.i32(machine.bank);
        saved_machines.i32(machine.legacy_gem_currency ? 1 : 0);
    }
    this->mysql_update<::blob>("vending_machines", saved_machines);
}

std::vector<world> worlds;

void send_action(ENetPeer& p, const std::string &action, const std::string &str) 
{
    const std::string &fmt_action = std::format("action|{}\n", action);
    std::vector<u_char> data(sizeof(int) + fmt_action.length() + str.length(), 0x00);
    
    data[0] = 03; // @note NET_MESSAGE_GAME_MESSAGE
    {
        const u_char *i8 = reinterpret_cast<const u_char*>(fmt_action.c_str());
        for (std::size_t i = 0ull; i < fmt_action.length(); ++i)
            data[sizeof(int) + i] = i8[i];
    }
    if (!str.empty())
    {
        const u_char *i8 = reinterpret_cast<const u_char*>(str.c_str());
        for (std::size_t i = 0ull; i < str.length(); ++i)
            data[sizeof(int) + fmt_action.length() + i] = i8[i];
    }
    
    ENetPacket *packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
    if (enet_peer_send(&p, 0, packet)) enet_packet_destroy(packet);

}

void send_data(ENetPeer &peer, const ::blob &blob)
{
    ENetPacket *packet = enet_packet_create(blob.data().data(), blob.size(), ENET_PACKET_FLAG_RELIABLE);
    if (packet == nullptr || packet->dataLength < sizeof(::gamePacket)) return;

    if (enet_peer_send(&peer, 0, packet)) enet_packet_destroy(packet);
}

void state_visuals(ENetPeer &peer, ::gamePacket &&gamePacket) 
{
    ::peer *pPeer = static_cast<::peer*>(peer.data);

    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p) 
    {
        send_data(p, compress_state(gamePacket));
    });
}

void tile_apply_damage(ENetEvent &event, ::gamePacket gamePacket, block &block, u_int value)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    (block.fg == 0) ? ++block.hits[1] : ++block.hits[0];
    gamePacket.type = (value << 24) | 0x000008; // @note 0x{}000008
    gamePacket.id = 6; // @note idk exactly
    gamePacket.netid = pPeer->netid;
	state_visuals(*event.peer, std::move(gamePacket));
}

u_short modify_item_inventory(ENetEvent &event, ::slot slot)
{   
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    ::gamePacket gamePacket{.id = slot.id};
    if (slot.count < 0) gamePacket.type = (slot.count*-1 << 16) | 0x000d; // @noote 0x00{}000d
    else                gamePacket.type = (slot.count    << 24) | 0x000d; // @noote 0x{}00000d
    send_data(*event.peer, compress_state(gamePacket)); // @note only the player whose backpack changes

    const u_short remains = pPeer->emplace(::slot(slot.id, slot.count));
    if (slot.count > 0) event_bus::emit({ event_bus::type::item_changed, event.peer, {}, slot.id, slot.count }); // @note quests & achievements
    return remains;
}

void item_change_object(ENetEvent &event, ::gamePacket gamePacket) 
{
    gamePacket.type = 0x0e; // @note PACKET_ITEM_CHANGE_OBJECT

    state_visuals(*event.peer, std::move(gamePacket));
}

void merge_object(ENetEvent &event, ::slot slot, const ::pos &pos, ::world &world)
{
    if (slot.id == 112/*gem*/) return; // @note gems never stack: each pile keeps its own denomination so the client draws the right gem

    // @note only merge into a stack that still has room (a full stack earlier in the list must not block the rest)
    auto object = std::ranges::find_if(world.objects, [&](const ::object &object) {
        return object.id == slot.id && object.count < 200 && (object.pos.by_32(true) == pos.by_32(true));
    });
    if (object == world.objects.end()) return; // @note add_object re-checks; never merge into nothing
    const int room = 200 - object->count;
    const int take = std::clamp<int>(slot.count, 0, std::max(room, 0));
    object->count += take;

    item_change_object(event, ::gamePacket{
        .netid = (int)0xfffffffd,
        .uid   = (int)object->uid,
        .count = static_cast<float>(object->count),
        .id    = object->id,
        .pos   = object->pos
    });
    world.save_objects();
    if (take < slot.count) // @note overflow spills into a fresh stack instead of vanishing
        add_object(event, ::slot(slot.id, static_cast<u_short>(slot.count - take)), pos, world);
}

void remove_object(ENetEvent& event, signed uid)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    item_change_object(event, ::gamePacket{
        .netid = pPeer->netid,
        .uid   = (int)0xffffffff,
        .id    = uid
    });
}

int add_object(ENetEvent& event, ::slot slot, const ::pos& pos, ::world &world)
{
    if (slot.count <= 0) return 0; // @note nothing to drop

    if (slot.id != 112/*gem*/) // @note gems are never merged, see merge_object
    {
        auto object = std::ranges::find_if(world.objects, [&](const ::object &object) {
            return object.id == slot.id && object.count < 200 && (object.pos.by_32(true) == pos.by_32(true));
        });
        if (object != world.objects.end())
        {
            const int uid = static_cast<int>(object->uid); // @note merge_object may grow the vector via overflow spill
            merge_object(event, slot, pos, world);
            return uid;
        }
    }
    const ::object &it = world.objects.emplace_back(::object(slot.id, static_cast<u_short>(slot.count), pos, ++world.last_object_uid));
    const int uid = static_cast<int>(it.uid);

    item_change_object(event, ::gamePacket{
        .netid = (int)0xffffffff,
        .uid   = uid,
        .count = static_cast<float>(slot.count),
        .id    = slot.id,
        .pos   = pos
    });
    world.save_objects();
    return uid;
}

void add_drop(ENetEvent &event, ::slot im, ::pos pos, ::world &world) // @todo
{
    add_object(event, im, ::pos{
        pos.x + RandomRange(0, 16),
        pos.y + RandomRange(0, 16)
    }, world);
}

void send_tile_update(ENetEvent &event, ::gamePacket gamePacket, ::block &block, ::world &world) 
{
    gamePacket.type = 05; // @note PACKET_SEND_TILE_UPDATE_DATA
    gamePacket.state = state::S_EXTENDED;
    ::blob blob = compress_state(gamePacket);

    blob.push_back(block.to_blob());

    const ::item &item = id_to_item(block.fg);
    blob.u8(get_type(id_to_item(block.fg)));
    switch (item.type)
    {
        case type::DOOR:
        {
            auto door = std::ranges::find(world.doors, gamePacket.punch, &::door::pos);
            if (door != world.doors.end())
            {
                blob.push_back(door->to_blob());
            }
            break;
        }
        case type::SIGN:
        {
            auto sign = std::ranges::find(world.signs, gamePacket.punch, &::sign::pos);
            if (sign != world.signs.end())
            {
                blob.push_back(sign->to_blob());
            }
            break;
        }
        case type::LOCK:
        {
            if (!is_tile_lock(block.fg)) world.is_public = (block.state[2] & S_PUBLIC); // @note check if world lock has S_PUBLIC flag, i will change this later
            int access = std::ranges::count_if(world.access, std::identity{});

            blob.u8(world.lock_state);
            blob.i32(world.owner);
            blob.i32(access);
            for (int user_id : world.access)
                if (user_id != 0) blob.i32(user_id);
            break;
        }
        case type::DISPLAY_BLOCK:
        {
            const auto display = std::ranges::find(world.displays, gamePacket.punch, &::display::pos);
            blob.u32(display == world.displays.end() ? 0u : display->id);
            break;
        }
        case type::SEED:
        {
            auto tree = std::ranges::find(world.trees, gamePacket.punch, &::tree::pos);
            if (tree != world.trees.end())
            {
                blob.push_back(tree->to_blob(true));
            }
            break;
        }
        case type::PROVIDER:
        {
            blob.u32(provider_elapsed_seconds(world, gamePacket.punch));
            break;
        }
        case type::VENDING_MACHINE:
        {
            const auto machine = std::ranges::find(world.vending_machines, gamePacket.punch, &::vending_machine_state::pos);
            blob.i32(machine == world.vending_machines.end() ? 0 : machine->item_id);
            blob.i32(machine == world.vending_machines.end() || machine->legacy_gem_currency ? 0 : machine->price);
            break;
        }
    }
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
    {
        send_data(p, blob);
    });
    world.save_blocks();
    if (item.type == type::LOCK)
        world.save_metadata();
}

void send_particle_effect(ENetEvent &event, const ::pos& pos, ::pos speed, int id, float offset)
{
    state_visuals(*event.peer, ::gamePacket{
        .type = 0x11, // @note PACKET_SEND_PARTICLE_EFFECT
        .netid = id, // @todo figure out if this is correct, i just assumed from firework visuals
        .id = id,
        .pos = pos,
        .speed = speed,
        .idk = offset
    });
}

void remove_fire(ENetEvent &event, gamePacket gamePacket, ::block &block, ::world &world)
{
    send_particle_effect(event, gamePacket.punch.by_32(), {0x00, 0x95});

    block.state[3] &= ~S_FIRE;
    send_tile_update(event, gamePacket, block, world);

    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    if (++pPeer->fires_removed % 100 == 0) 
    {
        on::ConsoleMessage(event.peer, "`oI'm so good at fighting fires, I rescused this `2Highly Combustible Box``!");
        modify_item_inventory(event, {3090/*Combustible Box*/, 1});
    }
    pPeer->add_xp(event, 1);
}

void fireworks(ENetEvent &event, const ::pos &pos)
{
    int type  [3]{ RandomRange(0x25, 0x28), RandomRange(0x25, 0x28), RandomRange(0x25, 0x28) };
    int offset[3]{ RandomRange(260, 2200), RandomRange(260, 2200), RandomRange(260, 2200) };

    send_particle_effect(event, pos, {0xb3, type[0]}, 0xc8*0, offset[0]);
    send_particle_effect(event, pos, {0xbe, type[1]}, 0xc8*1, offset[1]);
    send_particle_effect(event, pos, {0x7c, type[2]}, 0xc8*2, offset[2]);
}

void generate_world(::world &world)
{
    u_short main_door = RandomRange(2, cord(0, 60) / 100 - 4);
    std::vector<::block> blocks(cord(0, 60), ::block{0, 0});
    const int x = blocks.size() / 60;
    
    for (int i = 0ull; i < blocks.size(); ++i)
    {
        ::block &block = blocks[i];
        if (i >= cord(0, 37))
        {
            block.bg = 14; // @note cave background
            if (i >= cord(0, 38) && i < cord(0, 50) /* (above) lava level */ && RandomRange(0, 38) <= 1) block.fg = 10; // rock
            else if (i > cord(0, 50) && i < cord(0, 54) /* (above) bedrock level */ && RandomRange(0, 8) < 3) block.fg = 4; // lava
            else block.fg = (i >= cord(0, 54)) ? 8 : 2;
        }
        if (i == cord(main_door, 36))
        {
            block.fg = 6;
            world.doors.emplace_back(::door("EXIT","","", ::pos(i % x, i / x))); // @todo seems a bit hardcoded.
        }
        else if (i == cord(main_door, 37)) block.fg = 8; // @note bedrock (below main door)
    }
    world.blocks = std::move(blocks);
}

bool door_mover(::world &world, const ::pos &pos)
{
    std::vector<::block> &blocks = world.blocks;

    if (blocks[cord(pos.x, pos.y)].fg != 0 ||
        blocks[cord(pos.x, (pos.y + 1))].fg != 0) return false;

    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        if (blocks[i].fg == 6/*Main Door*/)
        {
            blocks[i].fg = 0; // @note remove main door
            blocks[cord(i % 100, (i / 100 + 1))].fg = 0; // @note remove bedrock below
            break;
        }
    }
    blocks[cord(pos.x, pos.y)].fg = 6;
    blocks[cord(pos.x, (pos.y + 1))].fg = 8;
    return true;
}

void blast::thermonuclear(::world &world)
{
    for (::block &block : world.blocks)
    {
        if (block.fg == 6/*main door*/ || block.fg == 8/*bedrock*/) continue;

        block.reset();
    }
    world.weather = ::pos{}; // @note every weather machine was just wiped
}
