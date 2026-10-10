#pragma once
#include <string>

/*
* @brief wrench menu features that need no extra packets: notebook, bio, wardrobe, growmojis, world lock bank,
*        "view worn clothes" and "send message" on other players.
*/
namespace profile_system
{
    /* @brief handle a button of the wrench ("popup" dialog). @return true if the button belonged to this system */
    bool handle_popup(ENetEvent &event, const ::hPipe &pipe);

    /* @brief the "profile_menu" dialog (notebook / bio / wardrobe / pm / back buttons) */
    void handle_dialog_return(ENetEvent &event, const ::hPipe &pipe);

    /* @brief "3h 20m" style play time of a peer, for the wrench menu */
    std::string playtime_text(const ::peer &p);
    /* @brief "12 days" style account age */
    std::string account_age_text(const ::peer &p);
    /* @return active effects as readable lines (double jump, punch effect, events...) */
    std::string effects_text(const ::peer &p);
}
