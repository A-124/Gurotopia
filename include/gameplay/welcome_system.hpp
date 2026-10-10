#pragma once
#include <string_view>

/*
* @brief what a player sees when they log in: the message of the day (resources/motd.txt) and, for brand new
*        accounts, a welcome window that points to the commands, the daily reward and the titles.
*/
namespace welcome_system
{
    /* @brief call once after login. */
    void on_login(ENetEvent &event);

    /* @brief /welcome: open the welcome window again */
    void command(ENetEvent &event, std::string_view text);

    void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe);
}
