#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace custom_content {
enum class custom_block_kind { none, lucky_box, pot_gold };
struct custom_item {
    int id{};
    std::string name{};
    int base_item{};
    // Optional client-record template. When unset, base_item supplies both behavior and render metadata.
    int render_base_item{-1};
    int type{};
    int rarity{};
    bool tradeable{true};
    std::string texture{};
    std::string info{};
    custom_block_kind block_kind{custom_block_kind::none};
    // Lucky Box uses positional weights: first ID has the highest chance, last the lowest.
    std::vector<int> lucky_box_drops{};
    int pot_gold_gem_multiplier{1};
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
