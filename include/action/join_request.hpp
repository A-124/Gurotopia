#pragma once

namespace action
{ 
    extern void join_request(ENetEvent& event, const std::string& header, const std::string_view world_name);
}
