#pragma once
#include <string_view>
namespace gacha_system {
void command(ENetEvent& event, std::string_view text);
void handle_dialog_return(ENetEvent& event, const ::hPipe& pipe);
}
