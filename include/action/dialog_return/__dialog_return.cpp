#include "pch.hpp"

#include "popup.hpp"
#include "drop_item.hpp"
#include "trash_item.hpp"
#include "find_item.hpp"
#include "gateway_edit.hpp"
#include "display_edit.hpp"
#include "vending.hpp"
#include "trade.hpp"
#include "billboard_edit.hpp"
#include "lock_edit.hpp"
#include "create_blast.hpp"
#include "socialportal.hpp"
#include "megaphone.hpp"
#include "commands/admin.hpp"
#include "gameplay/content_commands.hpp"

#include "__dialog_return.hpp"

std::unordered_map<std::string, std::function<void(ENetEvent &, const ::hPipe &)>> dialog_return_pool
{
    {"popup", std::bind(&popup, std::placeholders::_1, std::placeholders::_2)},

    {"drop_item", std::bind(&drop_item, std::placeholders::_1, std::placeholders::_2)},
    {"trash_item", std::bind(&trash_item, std::placeholders::_1, std::placeholders::_2)},
    {"find_item", std::bind(&find_item, std::placeholders::_1, std::placeholders::_2)},

    {"gateway_edit", std::bind(&gateway_edit, std::placeholders::_1, std::placeholders::_2)},
    {"door_edit", std::bind(&gateway_edit, std::placeholders::_1, std::placeholders::_2)},
    {"sign_edit", std::bind(&gateway_edit, std::placeholders::_1, std::placeholders::_2)},
    {"display_edit", std::bind(&display_edit, std::placeholders::_1, std::placeholders::_2)},
    {"vending", std::bind(&vending, std::placeholders::_1, std::placeholders::_2)},
    {"trade", std::bind(&trade::handle, std::placeholders::_1, std::placeholders::_2)},
    {"trade_closed", [](ENetEvent&, const ::hPipe&) {}},

    {"billboard_edit", std::bind(&billboard_edit, std::placeholders::_1, std::placeholders::_2)},
    {"lock_edit", std::bind(&lock_edit, std::placeholders::_1, std::placeholders::_2)},

    {"create_blast", std::bind(&create_blast, std::placeholders::_1, std::placeholders::_2)},
    {"socialportal", std::bind(&socialportal, std::placeholders::_1, std::placeholders::_2)},
    {"megaphone", std::bind(&megaphone, std::placeholders::_1, std::placeholders::_2)},
    {"admin_panel", std::bind(&admin_panel_return, std::placeholders::_1, std::placeholders::_2)},
    {"gurotopia_hub", std::bind(&handle_content_dialog_return, std::placeholders::_1, std::placeholders::_2)},
    {"gurotopia_quests", std::bind(&handle_content_dialog_return, std::placeholders::_1, std::placeholders::_2)},
    {"gurotopia_achievements", std::bind(&handle_content_dialog_return, std::placeholders::_1, std::placeholders::_2)},
    {"gurotopia_craft", std::bind(&handle_content_dialog_return, std::placeholders::_1, std::placeholders::_2)},
    {"gurotopia_content", std::bind(&handle_content_dialog_return, std::placeholders::_1, std::placeholders::_2)},
};
