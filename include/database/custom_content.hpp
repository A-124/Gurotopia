#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace custom_content {
struct custom_item {
    int id{};
    std::string name{};
    int base_item{};
    int type{};
    int rarity{};
    bool tradeable{true};
};
struct recipe {
    int result{};
    int amount{1};
    std::vector<std::pair<int,int>> ingredients;
};
bool reload();
const custom_item* find_item(int id) noexcept;
bool is_custom_item(int id) noexcept;
const recipe* find_recipe(int result) noexcept;
const std::unordered_map<int, custom_item>& items() noexcept;
}
