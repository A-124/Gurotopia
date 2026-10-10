#include "pch.hpp"
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include "audit.hpp"

void audit::log(const ::peer &actor, std::string_view text)
{
    static std::mutex lock;
    std::lock_guard guard{ lock };

    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    std::ofstream file("logs/audit.log", std::ios::app);
    if (!file) return; // @note a read-only disk must never stop the server

    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char stamp[32]{};
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &utc);

    std::string clean{ text.substr(0, 300) };
    std::erase_if(clean, [](unsigned char c) { return c < 0x20; });
    file << stamp << " UTC | uid " << actor.user_id << " " << actor.growid << " (role " << static_cast<int>(actor.role) << ") | /" << clean << '\n';
}
