#pragma once
#include <string_view>
namespace item_exchange_system {
void command(ENetEvent& event, std::string_view text);
void manage_command(ENetEvent& event, std::string_view text);
void handle_dialog_return(ENetEvent& event, const ::hPipe& pipe);
}
