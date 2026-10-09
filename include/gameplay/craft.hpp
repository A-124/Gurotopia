#pragma once
#include <string_view>
#include <enet/enet.h>
void craft(ENetEvent& event, const std::string_view text);
