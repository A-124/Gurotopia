#include "pch.hpp"
#include "find.hpp"
#include "find_item.hpp"

void find_item(ENetEvent& event, const ::hPipe& pipe)
{
    handle_find_dialog(event, pipe);
}