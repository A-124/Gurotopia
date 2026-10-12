#pragma once

#include <string_view>

namespace discord_webhook
{
    // Starts the background sender. No-op when GURO_DISCORD_WEBHOOK_URL is unset.
    void start();

    // Queue a plain-text Discord notification. Safe to call from the game thread.
    void notify(std::string_view message);

    // Drains queued notifications and joins the sender thread.
    void stop();
}
