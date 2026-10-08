#include "pch.hpp"
#include <filesystem>
#include <fstream>

#include "items.hpp"
#include "database/custom_content.hpp"

std::vector<::item> items;
const ::item* find_custom_runtime_item(u_short id) noexcept;
namespace {
std::vector<std::vector<u_char>> item_records;
std::vector<::item> custom_runtime_items;
const std::string_view item_name_token{"PBG892FXX982ABC*"};
void write_u16(std::vector<u_char>& d, std::size_t p, u_short v) { std::memcpy(d.data()+p, &v, sizeof(v)); }
void write_u32(std::vector<u_char>& d, std::size_t p, u_int v) { std::memcpy(d.data()+p, &v, sizeof(v)); }
void write_u8(std::vector<u_char>& d, std::size_t p, u_char v) { d[p]=v; }
void write_i32(std::vector<u_char>& d, std::size_t p, int v) { std::memcpy(d.data()+p, &v, sizeof(v)); }
u_int hash_bytes(const u_char* data, std::size_t size) noexcept { u_int acc = 0x55555555u; for (std::size_t i = 0; i < size; ++i) acc = ((acc << 5) | (acc >> 27)) + data[i]; return acc; }

}

const ::item &id_to_item(u_short id) noexcept // @note std::out_of_range is handled
{
    if (id < items.size()) return items[id];
    if (const auto* custom = find_custom_runtime_item(id)) return *custom;
    static const ::item dummy{};
    return dummy;
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
    item_records.clear();
    item_records.reserve(count);
    for (u_int i = 0; i < count; ++i)
    {
        const std::size_t record_start = pos;
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
        item_records.emplace_back(im_data.begin() + record_start, im_data.begin() + pos);
    }
    printf("items.dat parsed successfully!\n");
    if (!custom_content::items().empty() && !rebuild_custom_items()) return false;
    return true;
}


bool rebuild_custom_items()
{
    if (items.empty() || item_records.size() != items.size()) return false;
    custom_runtime_items.clear();
    const std::size_t header_size = sizeof(::gamePacket);
    if (im_data.size() < header_size + sizeof(u_short) + sizeof(u_int)) return false;

    // Preserve the original items.dat byte-for-byte. Re-serializing parsed
    // records is dangerous: a parser/format mismatch can corrupt vanilla
    // texture metadata (especially hand-item sprites). Custom records are
    // appended after the untouched vanilla database.
    std::vector<u_char> rebuilt = im_data;
    u_short version{};
    std::memcpy(&version, im_data.data() + header_size, sizeof(version));
    u_int vanilla_count{};
    std::memcpy(&vanilla_count, im_data.data() + header_size + sizeof(version), sizeof(vanilla_count));

    std::vector<std::pair<u_int, std::vector<u_char>>> custom_records;
    custom_records.reserve(custom_content::items().size());

    for (const auto& [id, def] : custom_content::items()) {
        if (id < 1000 || id > 65535) continue;

        std::size_t base_index = item_records.size();
        for (std::size_t n = 0; n < items.size(); ++n)
            if (items[n].id == static_cast<u_short>(def.base_item)) { base_index = n; break; }
        if (base_index == item_records.size()) continue;

        bool vanilla_id = false;
        for (const auto& vanilla : items)
            if (vanilla.id == static_cast<u_short>(id)) { vanilla_id = true; break; }
        if (vanilla_id || id < static_cast<int>(vanilla_count)) continue;

        const auto& base = item_records[base_index];
        if (base.size() < 10) continue;

        std::vector<u_char> record = base;
        const u_short custom_id = static_cast<u_short>(id);
        write_u32(record, 0, static_cast<u_int>(custom_id));

        // Preserve base flags and change only the tradeability bit.
        u_short flags{};
        std::memcpy(&flags, record.data() + 4, sizeof(flags));
        if (def.tradeable) flags &= static_cast<u_short>(~CAT_UNTRADEABLE);
        else flags |= CAT_UNTRADEABLE;
        write_u16(record, 4, flags);

        write_u8(record, 6, static_cast<u_char>(std::clamp(def.type, 0, 255)));

        u_short name_len{};
        std::memcpy(&name_len, record.data() + 8, sizeof(name_len));
        const std::size_t name_start = 10;
        if (name_start + name_len > record.size() || def.name.size() > 32767) continue;

        std::vector<u_char> encoded(def.name.size());
        for (std::size_t n = 0; n < def.name.size(); ++n)
            encoded[n] = static_cast<u_char>(def.name[n]) ^
                         static_cast<u_char>(item_name_token[(n + custom_id) % item_name_token.size()]);
        record.erase(record.begin() + name_start, record.begin() + name_start + name_len);
        record.insert(record.begin() + name_start, encoded.begin(), encoded.end());
        write_u16(record, 8, static_cast<u_short>(encoded.size()));

        if (!def.texture.empty()) {
            const std::size_t texture_len_pos = name_start + encoded.size();
            if (texture_len_pos + sizeof(u_short) <= record.size()) {
                u_short old_len{};
                std::memcpy(&old_len, record.data() + texture_len_pos, sizeof(old_len));
                const std::size_t texture_start = texture_len_pos + sizeof(u_short);
                if (texture_start + old_len <= record.size()) {
                    record.erase(record.begin() + texture_start, record.begin() + texture_start + old_len);
                    record.insert(record.begin() + texture_start, def.texture.begin(), def.texture.end());
                    const u_short new_len = static_cast<u_short>(def.texture.size());
                    write_u16(record, texture_len_pos, new_len);

                    // Custom RTTEX files are standalone assets, not the original
                    // sprite sheets. Start their item sprite at cell (0, 0).
                    // Texture X/Y follow texture hash, visual effect, and cook time.
                    // Do not overwrite the cook-time/ingredient field.
                    const std::size_t texture_x_pos =
                        texture_start + new_len + sizeof(u_int) + sizeof(u_char) + sizeof(u_int);
                    const std::size_t texture_y_pos = texture_x_pos + 1;
                    if (texture_y_pos < record.size()) {
                        write_u8(record, texture_x_pos, 0);
                        write_u8(record, texture_y_pos, 0);
                    }

                    const std::filesystem::path texture_path =
                        std::filesystem::path("resources/custom_assets") /
                        std::filesystem::path(def.texture).filename();
                    std::ifstream texture_file(texture_path, std::ios::binary);
                    if (texture_file) {
                        std::vector<u_char> texture_data(
                            (std::istreambuf_iterator<char>(texture_file)),
                            std::istreambuf_iterator<char>());
                        const u_int texture_hash =
                            hash_bytes(texture_data.data(), texture_data.size());
                        const std::size_t hash_pos = texture_start + new_len;
                        if (hash_pos + sizeof(texture_hash) <= record.size())
                            std::memcpy(record.data() + hash_pos, &texture_hash, sizeof(texture_hash));
                    }
                }
            }
        }

        // Clone server-side wearable metadata from the base item.
        ::item runtime = items[base_index];
        runtime.id = custom_id;
        runtime.raw_name = def.name;
        runtime.type = static_cast<u_char>(std::clamp(def.type, 0, 255));
        runtime.rarity = static_cast<short>(std::clamp(def.rarity, -32768, 32767));
        if (def.tradeable) runtime.cat = static_cast<u_char>(runtime.cat & ~CAT_UNTRADEABLE);
        else runtime.cat = static_cast<u_char>(runtime.cat | CAT_UNTRADEABLE);
        runtime.ingredient = static_cast<int>(def.base_item);
        custom_runtime_items.emplace_back(std::move(runtime));

        custom_records.emplace_back(static_cast<u_int>(id), std::move(record));
    }

    std::sort(custom_records.begin(), custom_records.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Grow the on-wire database to the highest custom ID. The client indexes
    // items.dat records by item ID, so simply appending ID 20000 after 16434
    // vanilla records would leave the custom item at the wrong record index.
    if (!custom_records.empty()) {
        const auto placeholder = item_records.front();
        u_int next_id = vanilla_count;

        for (const auto& [custom_id, record] : custom_records) {
            while (next_id < custom_id) {
                std::vector<u_char> filler = placeholder;
                write_u16(filler, 0, static_cast<u_short>(next_id));
                rebuilt.insert(rebuilt.end(), filler.begin(), filler.end());
                ++next_id;
            }
            rebuilt.insert(rebuilt.end(), record.begin(), record.end());
            next_id = custom_id + 1;
        }
    }

    const u_int total_count = custom_records.empty()
        ? vanilla_count
        : std::max<u_int>(vanilla_count, custom_records.back().first + 1); 
    write_u16(rebuilt, header_size, version);
    std::memcpy(rebuilt.data() + header_size + sizeof(version), &total_count, sizeof(total_count));
    const u_int packet_size = static_cast<u_int>(rebuilt.size() - header_size);
    std::memcpy(rebuilt.data() + offsetof(::gamePacket, size), &packet_size, sizeof(packet_size));
    im_data.swap(rebuilt);
    return true;
}


u_int item_data_hash() noexcept
{
    constexpr std::size_t header_size = sizeof(::gamePacket);
    if (im_data.size() <= header_size) return 0;
    return hash_bytes(im_data.data() + header_size, im_data.size() - header_size);
}

const ::item* find_custom_runtime_item(u_short id) noexcept
{
    for (const auto& value : custom_runtime_items)
        if (value.id == id) return &value;
    return nullptr;
}
