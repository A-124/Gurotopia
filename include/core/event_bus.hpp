#pragma once
#include <functional>
#include <string_view>
#include <vector>
namespace event_bus {
enum class type : unsigned char { tick, player_connected, player_disconnected, packet_received, command_executed, game_packet_received, player_entered_world, item_changed, block_changed, item_dropped };
struct event { type kind; void* context{}; std::string_view name{}; int item_id{-1}; int amount{}; int x{}; int y{}; };
using listener = std::function<void(const event&)>;
void subscribe(type kind, listener callback);
void emit(const event& value);
void clear();
}
