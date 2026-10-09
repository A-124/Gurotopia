#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace custom_content {
struct custom_item {
    int id{};
    std::string name{};
    int base_item{};
    int type{};
    int rarity{};
    bool tradeable{true};
    std::string texture{};
    std::string info{};
};
struct recipe { int result{}; int amount{1}; std::vector<std::pair<int, int>> ingredients; };
// Validate without mutating the active registry. Empty means valid.
std::vector<std::string> validate();
// Replace active content only if every definition and reference is valid.
bool reload();
const custom_item* find_item(int id) noexcept;
bool is_custom_item(int id) noexcept;
const recipe* find_recipe(int result) noexcept;
std::size_t recipe_count() noexcept;
const std::unordered_map<int, custom_item>& items() noexcept;
}
