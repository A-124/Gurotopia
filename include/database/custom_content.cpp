#include "pch.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include "custom_content.hpp"

namespace custom_content {
namespace {
using item_map = std::unordered_map<int, custom_item>;
using recipe_map = std::unordered_map<int, recipe>;
item_map custom_items;
recipe_map recipes;
struct parsed_content { item_map items; recipe_map recipes; std::vector<std::string> errors; };

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, delimiter)) parts.push_back(part);
    return parts;
}
std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) value.remove_suffix(1);
    return value;
}
bool integer(const std::string& value, int& output) {
    try { std::size_t consumed{}; output = std::stoi(value, &consumed); return consumed == value.size(); }
    catch (...) { return false; }
}
bool has_vanilla_item(int id) {
    return std::any_of(::items.begin(), ::items.end(), [id](const ::item& value) {
        return value.id == static_cast<u_short>(id);
    });
}
void add_error(parsed_content& parsed, std::size_t line, std::string message) {
    parsed.errors.emplace_back(std::format("resources/custom_items.txt:{}: {}", line, std::move(message)));
}
bool parse_file(parsed_content& parsed) {
    std::ifstream file("resources/custom_items.txt");
    if (!file) { parsed.errors.emplace_back("resources/custom_items.txt: file not found or could not be opened"); return false; }
    std::string line;
    std::size_t line_number{};
    while (std::getline(file, line)) {
        ++line_number;
        const auto content = trim(line);
        if (content.empty() || content.front() == '#') continue;
        const auto fields = split(std::string(content), '|');
        if (fields.empty()) { add_error(parsed, line_number, "empty definition"); continue; }

        if (fields[0] == "item") {
            if (fields.size() < 6 || fields.size() > 9) {
                add_error(parsed, line_number, "expected item|id|name|base_item|type|rarity|tradeable(0/1)|texture|info"); continue;
            }
            custom_item def;
            if (!integer(fields[1], def.id) || !integer(fields[3], def.base_item) ||
                !integer(fields[4], def.type) || !integer(fields[5], def.rarity)) {
                add_error(parsed, line_number, "item id, base_item, type, and rarity must be integers"); continue;
            }
            def.name = fields[2];
            if (fields.size() > 6) {
                if (fields[6] != "0" && fields[6] != "1") {
                    add_error(parsed, line_number, "tradeable must be 0 or 1"); continue;
                }
                def.tradeable = fields[6] == "1";
            }
            if (fields.size() > 7) def.texture = fields[7];
            if (fields.size() > 8) def.info = fields[8];
            if (def.id < 1000 || def.id > 65535) {
                add_error(parsed, line_number, "custom item id must be between 1000 and 65535"); continue;
            }
            if (def.id < static_cast<int>(::items.size()) || has_vanilla_item(def.id)) {
                add_error(parsed, line_number, "custom item id overlaps the vanilla item database"); continue;
            }
            if (def.base_item < 0 || def.base_item > 65535 || !has_vanilla_item(def.base_item)) {
                add_error(parsed, line_number, "base_item must reference an existing vanilla item"); continue;
            }
            if (def.type < 0 || def.type > 255) {
                add_error(parsed, line_number, "type must be between 0 and 255"); continue;
            }
            if (def.rarity < 0 || def.rarity > 32767) {
                add_error(parsed, line_number, "rarity must be between 0 and 32767"); continue;
            }
            if (trim(def.name).empty()) {
                add_error(parsed, line_number, "item name cannot be empty"); continue;
            }
            if (def.name.size() > 32767 || def.texture.size() > 32767 || def.info.size() > 32767) {
                add_error(parsed, line_number, "name, texture, and info must each be at most 32767 bytes"); continue;
            }
            if (parsed.items.contains(def.id)) {
                add_error(parsed, line_number, std::format("duplicate custom item id {}", def.id)); continue;
            }
            if (!def.texture.empty()) {
                const auto path = std::filesystem::path("resources/custom_assets") / std::filesystem::path(def.texture).filename();
                std::error_code ec;
                if (!std::filesystem::is_regular_file(path, ec) || ec) {
                    add_error(parsed, line_number, std::format("texture file does not exist: {}", path.string())); continue;
                }
                if (std::filesystem::file_size(path, ec) == 0 || ec) {
                    add_error(parsed, line_number, std::format("texture file is empty or unreadable: {}", path.string())); continue;
                }
            }
            parsed.items.emplace(def.id, std::move(def));
            continue;
        }

        if (fields[0] == "recipe") {
            if (fields.size() != 4) {
                add_error(parsed, line_number, "expected recipe|result_id|result_amount|ingredient_id:amount,..."); continue;
            }
            recipe def;
            if (!integer(fields[1], def.result) || !integer(fields[2], def.amount)) {
                add_error(parsed, line_number, "recipe result_id and result_amount must be integers"); continue;
            }
            if (def.result < 0 || def.result > 65535 || def.amount < 1 || def.amount > 200) {
                add_error(parsed, line_number, "recipe result_id must be 0..65535 and result_amount must be 1..200"); continue;
            }
            bool valid = true;
            std::unordered_set<int> seen;
            for (const auto& token : split(fields[3], ',')) {
                const auto pair = split(token, ':');
                int id{}, amount{};
                if (pair.size() != 2 || !integer(pair[0], id) || !integer(pair[1], amount) || id < 0 || id > 65535 || amount < 1) {
                    add_error(parsed, line_number, std::format("invalid ingredient '{}'; expected item_id:positive_amount", token));
                    valid = false; break;
                }
                if (!seen.insert(id).second) {
                    add_error(parsed, line_number, std::format("ingredient id {} appears more than once", id));
                    valid = false; break;
                }
                def.ingredients.emplace_back(id, amount);
            }
            if (!valid) continue;
            if (def.ingredients.empty()) { add_error(parsed, line_number, "recipe must contain at least one ingredient"); continue; }
            if (parsed.recipes.contains(def.result)) {
                add_error(parsed, line_number, std::format("duplicate recipe result id {}", def.result)); continue;
            }
            parsed.recipes.emplace(def.result, std::move(def));
            continue;
        }
        add_error(parsed, line_number, std::format("unknown definition '{}'; expected 'item' or 'recipe'", fields[0]));
    }
    if (file.bad()) parsed.errors.emplace_back("resources/custom_items.txt: read error");
    if (::items.empty()) parsed.errors.emplace_back("items.dat must be loaded before custom content can be validated");
    for (const auto& [result_id, def] : parsed.recipes) {
        if (!parsed.items.contains(result_id) && !has_vanilla_item(result_id))
            parsed.errors.emplace_back(std::format("recipe result id {} does not reference an existing vanilla or custom item", result_id));
        for (const auto& [ingredient_id, amount] : def.ingredients) {
            (void)amount;
            if (!parsed.items.contains(ingredient_id) && !has_vanilla_item(ingredient_id))
                parsed.errors.emplace_back(std::format("recipe for item {} references unknown ingredient id {}", result_id, ingredient_id));
        }
    }
    return parsed.errors.empty();
}
}
std::vector<std::string> validate() {
    parsed_content parsed;
    parse_file(parsed);
    return std::move(parsed.errors);
}
bool reload() {
    parsed_content parsed;
    if (!parse_file(parsed)) return false;
    custom_items.swap(parsed.items);
    recipes.swap(parsed.recipes);
    return true;
}
const custom_item* find_item(int id) noexcept {
    const auto found = custom_items.find(id);
    return found == custom_items.end() ? nullptr : &found->second;
}
bool is_custom_item(int id) noexcept { return custom_items.contains(id); }
const recipe* find_recipe(int result) noexcept {
    const auto found = recipes.find(result);
    return found == recipes.end() ? nullptr : &found->second;
}
const std::unordered_map<int, custom_item>& items() noexcept { return custom_items; }
std::size_t recipe_count() noexcept { return recipes.size(); }
}
