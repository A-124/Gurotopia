#pragma once
#include <string_view>
namespace donation_system {
void command(ENetEvent& event, std::string_view text);
}
