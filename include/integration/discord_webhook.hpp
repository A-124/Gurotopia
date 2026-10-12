#pragma once

#include <string_view>

/* Discord webhook notifications (libcurl, background queue).
 *
 * Enable with either:
 *   export GURO_DISCORD_WEBHOOK_URL="https://discord.com/api/webhooks/..."
 * or put that URL alone on the first line of:
 *   resources/discord_webhook.url
 *
 * Never commit a real webhook URL — treat it like a password.
 */
namespace discord_webhook
{
    void start();
    void stop();

    /* Queue a plain-text message (max ~2000 chars). Safe from the game thread. */
    void notify(std::string_view message);

    /* Queue a rich embed. color is 0xRRGGBB (e.g. 0x57F287 green). */
    void notify_embed(std::string_view title, std::string_view description, int color = 0x5865F2);

    bool enabled() noexcept;
}
