#include "pch.hpp"
#include <filesystem>
#include <fstream>

#include "items.hpp"
#include "database/custom_content.hpp"

std::vector<::item> items;
namespace {
std::vector<std::vector<u_char>> item_records;
const std::string_view item_name_token{"PBG892FXX982ABC*"};
void write_u16(std::vector<u_char>& d, std::size_t p, u_short v) { std::memcpy(d.data()+p, &v, sizeof(v)); }
void write_u8(std::vector<u_char>& d, std::size_t p, u_char v) { d[p]=v; }
void write_i32(std::vector<u_char>& d, std::size_t p, int v) { std::memcpy(d.data()+p, &v, sizeof(v)); }
}

const ::item &id_to_item(u_short id) noexcept // @note std::out_of_range is handled
{
    if (id >= items.size()) { static const ::item dummy{}; return dummy; }
    
    return items[id];
}


std::vector<u_char> im_data(sizeof(::gamePacket)/*inital packet*/, 0x00);

template<typename T>
void shift_pos(const std::vector<u_char> &data, u_int &pos, T &value) noexcept // @note std::out_of_range is handled
{
    u_char *i8 = reinterpret_cast<u_char*>(&value);

    if (pos + sizeof(T) >= data.size()) puts("this items.dat is unsupported");
    for (std::size_t i = 0ull; i < sizeof(T); ++i) 
    {
        i8[i] = data[pos + i];
    }
    pos += sizeof(T);
}

/* have not tested modifying string values··· */
template<typename T>
void data_modify(std::vector<u_char> &data, const u_int &pos, const T &value) noexcept // @note std::out_of_range is handled
{
    const u_char *i8 = reinterpret_cast<const u_char*>(&value);

    if (pos + sizeof(T) >= data.size()) return; // @todo for custom items we would need resize data (im_data)
    for (std::size_t i = 0ull; i < sizeof(T); ++i) 
    {
        data[pos + i] = i8[i];
    }
}

bool decode_items()
{
    if (!std::filesystem::exists("items.dat")) return false;
    items.clear();
    const u_int size = std::filesystem::file_size("items.dat");
    im_data = compress_state(::gamePacket{ .type = 0x10,/*PACKET_SEND_ITEM_DATABASE_DATA*/ .state = state::S_EXTENDED, .size = size }).data();
    
    u_int pos = im_data.size(); // @note sizeof(::gamePacket)
    im_data.resize(pos + size); // @note resize to fit binary data
    
    std::ifstream file("items.dat", std::ios::binary);
    if (!file) return false;
    file.read((char*)&im_data[pos], size);
    if (!file) return false; // @note the binary data···

    u_short version{};
    shift_pos(im_data, pos, version);
    u_int count{};
    shift_pos(im_data, pos, count);

    const std::string_view token{"PBG892FXX982ABC*"};
    for (u_int i = 0; i < count; ++i)
    {
        ::item item;
        
        shift_pos(im_data, pos, item.id); pos += 2; // @note downside im.id to 2 bit (short)
        shift_pos(im_data, pos, item.property);

        shift_pos(im_data, pos, item.cat);

        shift_pos(im_data, pos, item.type);
        pos += sizeof(u_char);

        short len = *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);
        item.raw_name.resize(len);
        for (short i = 0; i < len; ++i) 
            item.raw_name[i] = im_data[pos] ^ token[(i + item.id) % token.length()], 
            ++pos;

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += sizeof(int);
        pos += sizeof(u_char);

        shift_pos(im_data, pos, item.ingredient);
        pos += sizeof(u_char);
        pos += sizeof(u_char);
        pos += sizeof(u_char);
        pos += sizeof(u_char);

        shift_pos(im_data, pos, item.collision);
        shift_pos(im_data, pos, item.hits);
        if (item.hits != 0) item.hits /= 6; // @note unknown reason behind why break hit is muliplied by 6 then having to divide by 6

        shift_pos(im_data, pos, item.hit_reset);

        if (item.type == type::CLOTHING) 
        {
            u_char cloth_type{};
            shift_pos(im_data, pos, item.cloth_type);
        }
        else pos += 1; // @note assign nothing
        if (item.type == type::AURA) item.cloth_type = clothing::ANCES;
        shift_pos(im_data, pos, item.rarity);

        pos += sizeof(u_char);
        {
            len = *reinterpret_cast<short*>(&im_data[pos]);
            pos += sizeof(short);
            std::string audio_directory{};
            audio_directory.assign(reinterpret_cast<char*>(&im_data[pos]), len);
            pos += len;

            // Preserve the original audio directory. Mutating the first byte of
            // an .mp3 path corrupts the client item database and can disable sounds.
            // Platform-specific audio handling must be negotiated with the client,
            // not applied to the shared items.dat payload.
        }
        pos += sizeof(int);

        pos += sizeof(std::array<u_char, 4ull>);

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += sizeof(std::array<u_char, 16ull>);

        shift_pos(im_data, pos, item.tick);

        pos += sizeof(short);
        pos += sizeof(short);

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += *(reinterpret_cast<short*>(&im_data[pos]));
        pos += sizeof(short);

        pos += sizeof(std::array<u_char, 80ull>);

        if (version >= 0x0b) // @date February 2019
        {
            pos += *(reinterpret_cast<short*>(&im_data[pos]));
            pos += sizeof(short);
        }
        if (version >= 0x0c) // @date October 2020
        {
            pos += sizeof(int);
            pos += sizeof(std::array<u_char, 9ull>);
        }
        if (version >= 0x0d) pos += sizeof(int); // @date May 2021
        if (version >= 0x0e) pos += sizeof(int); // @date October 2021
        if (version >= 0x0f)
        {
            pos += sizeof(std::array<u_char, 25ull>);
            pos += *(reinterpret_cast<short*>(&im_data[pos]));
            pos += sizeof(short);
        }
        if (version >= 0x10)
        {
            pos += *(reinterpret_cast<short*>(&im_data[pos]));
            pos += sizeof(short);
        }
        if (version >= 0x11) pos += sizeof(int); // @date April 2024
        if (version >= 0x12) pos += sizeof(int); // @date December 2024
        if (version >= 0x13) pos += sizeof(std::array<u_char, 9ull>);
        if (version >= 0x15) pos += sizeof(short); // @date September 2025
        if (version >= 0x16)
        {
            len = *reinterpret_cast<short*>(&im_data[pos]);
            pos += sizeof(short);
            item.info.assign(reinterpret_cast<char*>(&im_data[pos]), len);
            pos += len;
        }
        if (version >= 0x17) 
        {
            shift_pos(im_data, pos, item.splice[0]);
            shift_pos(im_data, pos, item.splice[1]);
        }
        if (version >= 0x18) pos += sizeof(u_char); // @date December 2025
        if (version >= 0x19)
        {
            len = *reinterpret_cast<short*>(&im_data[pos]);
            pos += sizeof(short);
            if (len > (sizeof(short) + sizeof(int))) // @note {size} 0x00 0x00 0x00 0x00
            {
                item.punch_fx.assign(reinterpret_cast<char*>(&im_data[pos]), len);
                pos += len;
            }
            pos += sizeof(int); // @note default: 0x00 0x00 0x00 0x00
        }
        if (version >= 0x1a) pos += sizeof(std::byte); // May 2026
        
        items.emplace_back(item);
    }
    printf("items.dat parsed successfully!\n");
    return true;
}


bool rebuild_custom_items()
{
    if (items.empty() || item_records.size() != items.size()) return false;
    const std::size_t header_size = sizeof(::gamePacket);
    if (im_data.size() < header_size + sizeof(u_short) + sizeof(u_int)) return false;

    // Remove a previous custom suffix by rebuilding the packet from the original
    // vanilla records. This makes repeated /reload content deterministic.
    std::vector<u_char> rebuilt(im_data.begin(), im_data.begin() + header_size);
    u_short version{}; std::memcpy(&version, im_data.data() + header_size, sizeof(version));
    u_int vanilla_count{}; std::memcpy(&vanilla_count, im_data.data() + header_size + sizeof(version), sizeof(vanilla_count));
    const std::size_t payload_start = header_size;
    rebuilt.insert(rebuilt.end(), im_data.begin() + payload_start,
                   im_data.begin() + payload_start + sizeof(version) + sizeof(vanilla_count));
    for (const auto& record : item_records) rebuilt.insert(rebuilt.end(), record.begin(), record.end());

    u_int total_count = vanilla_count;
    for (const auto& [id, def] : custom_content::items()) {
        if (id < 0 || id > 65535 || static_cast<std::size_t>(def.base_item) >= item_records.size()) continue;
        if (id < static_cast<int>(items.size())) continue;
        const auto& base = item_records[def.base_item];
        if (base.size() < 10) continue;

        std::vector<u_char> record = base;
        u_short custom_id = static_cast<u_short>(id);
        write_u16(record, 0, custom_id);
        write_u8(record, 6, static_cast<u_char>(std::clamp(def.type, 0, 255)));
        // Name length starts at byte 6 in a record: id(2), padding(2), property(1), cat(1).
        // Keep the record shape compatible while replacing the encoded name.
        u_short name_len{}; std::memcpy(&name_len, record.data() + 6, sizeof(name_len));
        const std::size_t name_start = 10;
        if (name_start + name_len > record.size() || def.name.size() > 32767) continue;
        std::vector<u_char> encoded(def.name.size());
        for (std::size_t n=0; n<def.name.size(); ++n)
            encoded[n] = static_cast<u_char>(def.name[n]) ^ static_cast<u_char>(item_name_token[(n + custom_id) % item_name_token.size()]);
        record.erase(record.begin() + name_start, record.begin() + name_start + name_len);
        record.insert(record.begin() + name_start, encoded.begin(), encoded.end());
        write_u16(record, 8, static_cast<u_short>(encoded.size()));

        // Rarity is after the clothing byte and several fixed fields, so retain the
        // base value for now; the server-side definition still owns the canonical rarity.
        (void)def.rarity;
        if (!def.tradeable) record[5] |= CAT_UNTRADEABLE;
        rebuilt.insert(rebuilt.end(), record.begin(), record.end());
        ++total_count;
    }
    write_u16(rebuilt, header_size, version);
    std::memcpy(rebuilt.data() + header_size + sizeof(version), &total_count, sizeof(total_count));
    const u_int packet_size = static_cast<u_int>(rebuilt.size() - header_size);
    std::memcpy(rebuilt.data() + offsetof(::gamePacket, size), &packet_size, sizeof(packet_size));
    im_data.swap(rebuilt);
    return true;
}
