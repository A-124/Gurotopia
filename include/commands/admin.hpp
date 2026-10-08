#pragma once

extern void admin(ENetEvent& event, const std::string_view text);
extern void show_admin_panel(ENetEvent& event, int selected_target_uid = 0);
extern void admin_panel_return(ENetEvent& event, const ::hPipe &hPipe);
