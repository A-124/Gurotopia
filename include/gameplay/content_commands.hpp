#pragma once
#include <string_view>
#include <enet/enet.h>
void content_status(ENetEvent& event, const std::string_view text);
