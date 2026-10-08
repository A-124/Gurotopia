#pragma once

extern u_char get_punch_id(u_int item_id);

extern void punch(ENetEvent& event, const std::string_view text);
