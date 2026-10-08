#include "pch.hpp"
#include <sstream>
#include "database/peer.hpp"
#include "core/runtime_reload.hpp"
#include "reload.hpp"
void reload(ENetEvent& event, const std::string_view text) {
 ::peer* pPeer = event.peer ? static_cast<::peer*>(event.peer->data) : nullptr;
 if (!pPeer) return;
 if (pPeer->role < DEVELOPER) { send_varlist(event.peer, {"OnConsoleMessage", "You don't have permission to use this command."}); return; }
 std::istringstream input{std::string(text)}; std::string command, target; input >> command >> target;
 if (target.empty()) { send_varlist(event.peer, {"OnConsoleMessage", "Usage: /reload <items|content|store|holiday|all>"}); return; }
 const auto result = runtime_reload::reload(target);
 send_varlist(event.peer, {"OnConsoleMessage", std::format("[RELOAD] {}: {}", target, runtime_reload::result_message(result))});
}
