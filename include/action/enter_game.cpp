#include "pch.hpp"
#include "onVariant/RequestWorldSelectMenu.hpp"
#include "onVariant/RequestGazette.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "onVariant/SetBux.hpp"
#include "tools/create_dialog.hpp"
#include "automate/holiday.hpp"
#include "gameplay/title_system.hpp"
#include "gameplay/welcome_system.hpp"

#include "enter_game.hpp"

void action::enter_game(ENetEvent& event, const std::string& header) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    pPeer->update_display_growid();
    on::ConsoleMessage(event.peer, 
        std::format("Welcome back, {}. No friends are online.", 
            pPeer->display_growid
        )
    );
    title_system::refresh(event.peer); // @note unlock anything earned while offline (account age, saved progress)
    if (const std::string tag = pPeer->title_enabled ? title_system::tag(*pPeer) : std::string{}; !tag.empty())
        on::ConsoleMessage(event.peer, std::format("`oYour title: ``{}`o- change it from the wrench menu > Title.``", tag));
    on::ConsoleMessage(event.peer, holiday_greeting().second);
    on::ConsoleMessage(event.peer, "`5Personal Settings active:`` `#Can customize profile``");
    
    send_inventory_state(event);
    on::SetBux(event);
    send_varlist(event.peer, { "SetHasGrowID", 1, pPeer->growid.c_str(), "" });
    {
        std::tm time = localtime();

        send_varlist(event.peer, {
            "OnTodaysDate",
            time.tm_mon + 1,
            time.tm_mday,
            0u, // @todo
            0u // @todo
        });
    } // @note delete time

    on::RequestWorldSelectMenu(event);
    on::RequestGazette(event);
    welcome_system::on_login(event); // @note message of the day, welcome window for new accounts

    send_data(*event.peer, compress_state(::gamePacket{ .type = 0x16 /*PACKET_PING_REQUEST*/ }));
    /* for v5.47+ client */
    send_varlist(event.peer, {
        "OnSetFeatureEnableFlags",
        "EA8DEAcGAgEOBQgKCQ0MEQQ=" // @todo Dw0JEQQMEAMPAgYBDgUICg==
    });
}
