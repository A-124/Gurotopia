#pragma once
#include <string_view>
namespace bonus_drop_system {
void command(ENetEvent& event, std::string_view text);
void on_block_broken(ENetEvent& event, int tile_id, const ::pos& position, ::world& world);
}
