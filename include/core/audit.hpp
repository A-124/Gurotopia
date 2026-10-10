#pragma once
#include <string_view>

/*
* @brief append-only record of what staff did, written to logs/audit.log (one line per action).
*        Needed on a public server: "who banned / muted / changed this?" must always be answerable.
*/
namespace audit
{
    /* @param text the command as typed, without the leading '/' */
    void log(const ::peer &actor, std::string_view text);
}
