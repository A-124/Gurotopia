#include "pch.hpp"

#include "commands/sb.hpp"
#include "tools/bubble.hpp"

#include "megaphone.hpp"

void megaphone(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (pPeer->curse_until > std::time(nullptr))
    {
        tell(event.peer, "`4You can't do that while cursed.``");
        return;
    }

    const std::string message = hPipe["message"];
    if (message.empty()) return;
    sb(event, "sb " + message); // @note sb() skips the first 3 chars ("sb ") @todo handle this when /sb requires gems @todo handle trim
}