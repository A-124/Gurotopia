#include "pch.hpp"

#include <cstdlib>
#include <ctime>
#include <format>
#include <sstream>
#include <string>
#include <vector>

#include "database/peer.hpp"
#include "proton/Variant.hpp"
#include "__command.hpp"
#include "event_manager.hpp"

/* Global state */
event_state g_event_state;

/* ==================== HELPERS ==================== */

static void broadcast_message(const std::string& msg)
{
    peers("", peer_condition::PEER_ALL, [&](ENetPeer& p)
    {
        send_varlist(&p, { "OnConsoleMessage", msg });
    });
}

static std::string format_duration(long seconds)
{
    long m = seconds / 60;
    long s = seconds % 60;
    if (m > 0)
        return std::format("{} minute(s) {} second(s)", m, s);
    return std::format("{} second(s)", s);
}

static void send_talk(ENetPeer* peer, int netid, const std::string& msg)
{
    send_varlist(peer, { "OnTalkBubble", netid, msg, 0 });
}

/* ==================== PUBLIC API ==================== */

bool is_event_active()
{
    if (!g_event_state.active) return false;
    if (std::time(nullptr) >= g_event_state.end_time) return false;
    return true;
}

float get_gem_multiplier()
{
    return is_event_active() ? g_event_state.gem_mult : 1.0f;
}

float get_xp_multiplier()
{
    return is_event_active() ? g_event_state.xp_mult : 1.0f;
}

/* ==================== COMMANDS ==================== */

void event_start_command(ENetEvent& event, const std::string_view text)
{
    ::peer* pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    /* role check: DEVELOPER only */
    if (pPeer->role < DEVELOPER)
    {
        send_talk(event.peer, pPeer->netid, "`4You don't have permission to use this command.``");
        return;
    }

    /* parse: /startmultiplier <gemMult> <xpMult> <durationSeconds> */
    std::vector<std::string> parts;
    std::istringstream iss{ std::string(text) };
    std::string tok;
    while (iss >> tok) parts.push_back(tok);

    if (parts.size() < 4)
    {
        send_talk(event.peer, pPeer->netid, "Usage: /startmultiplier <gem> <xp> <seconds>");
        return;
    }

    float gem_mult = std::strtof(parts[1].c_str(), nullptr);
    float xp_mult  = std::strtof(parts[2].c_str(), nullptr);
    long  duration = std::strtol(parts[3].c_str(), nullptr, 10);

    if (gem_mult < 1.0f)
    {
        send_talk(event.peer, pPeer->netid, "Invalid gem multiplier (minimum 1).");
        return;
    }
    if (xp_mult < 1.0f)
    {
        send_talk(event.peer, pPeer->netid, "Invalid XP multiplier (minimum 1).");
        return;
    }
    if (duration <= 0)
    {
        send_talk(event.peer, pPeer->netid, "Duration must be a positive number (in seconds).");
        return;
    }

    g_event_state.active   = true;
    g_event_state.gem_mult = gem_mult;
    g_event_state.xp_mult  = xp_mult;
    g_event_state.end_time = std::time(nullptr) + duration;

    broadcast_message(std::format(
        "`2[EVENT]`` Event started! Gem x{:.1f}, XP x{:.1f} for {}.",
        gem_mult, xp_mult, format_duration(duration)));

    send_talk(event.peer, pPeer->netid, "Multiplier event started!");
}

void event_stop_command(ENetEvent& event, const std::string_view)
{
    ::peer* pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    if (pPeer->role < DEVELOPER)
    {
        send_talk(event.peer, pPeer->netid, "`4You don't have permission to use this command.``");
        return;
    }

    if (!g_event_state.active)
    {
        send_talk(event.peer, pPeer->netid, "No active event.");
        return;
    }

    g_event_state.active   = false;
    g_event_state.gem_mult = 1.0f;
    g_event_state.xp_mult  = 1.0f;
    g_event_state.end_time = 0;

    broadcast_message("`4[EVENT]`` The multiplier event has ended.");
    send_talk(event.peer, pPeer->netid, "Multiplier event stopped.");
}

void event_show_command(ENetEvent& event, const std::string_view)
{
    ::peer* pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer) return;

    if (!is_event_active())
    {
        send_varlist(event.peer, { "OnConsoleMessage", "`4[EVENT]`` No active event right now." });
        return;
    }

    long remaining = static_cast<long>(g_event_state.end_time - std::time(nullptr));
    send_varlist(event.peer, { "OnConsoleMessage", std::format(
        "`2[EVENT]`` Gem drops: x{:.1f} | XP drops: x{:.1f} | Time left: {}",
        g_event_state.gem_mult, g_event_state.xp_mult, format_duration(remaining)) });
}

/* ==================== TIMER ==================== */

void event_manager_tick()
{
    if (g_event_state.active && std::time(nullptr) >= g_event_state.end_time)
    {
        g_event_state.active   = false;
        g_event_state.gem_mult = 1.0f;
        g_event_state.xp_mult  = 1.0f;
        g_event_state.end_time = 0;

        broadcast_message("`4[EVENT]`` The multiplier event has ended.");
    }
}