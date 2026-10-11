#pragma once
#include <string_view>
namespace donation_system {
void command(ENetEvent& event, std::string_view text);
void open_box(ENetEvent& event);
void handle_dialog_return(ENetEvent& event, const ::hPipe& pipe);
}
