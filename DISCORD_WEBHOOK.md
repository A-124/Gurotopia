# Discord webhook: lifecycle + multiplier event

This branch adds one-way Discord notifications for:
- One persistent server status embed, created after initialization and edited every 10 seconds to refresh uptime (resets when the process restarts).
- Server stopping (on a handled SIGINT/SIGTERM/SIGHUP). A restart appears as a stopping message followed by a startup message from the next process.
- Successful `/startmultiplier <gem> <xp> <seconds>` command execution.

## Configure

Open `include/integration/discord_webhook.cpp` and replace `PASTE_YOUR_DISCORD_WEBHOOK_URL_HERE` in `configured_webhook_url` with your Discord channel's **Incoming Webhook URL**.

Treat the webhook URL like a password. Anyone who gets it can post to that channel. Do not commit a real webhook URL to a public repository; add it locally after pulling the code, or keep your configured copy private. If the placeholder is unchanged, the integration stays disabled and the server works as before.

## Message behavior

The persistent status embed is created with Discord webhook `?wait=true` so the API returns its message ID; the integration then edits that same message every 10 seconds. Multiplier and shutdown announcements remain separate messages. The sender runs on background threads so the game loop does not wait for Discord HTTP responses. It verifies HTTPS and leaves libcurl TLS certificate/hostname verification enabled. Messages are JSON-escaped, mentions are disabled, requests have timeouts, and a small queue/rate delay is used.

This sends a stop notice only when the process receives a handled termination signal. A hard crash, forced kill, or power loss cannot reliably send a final webhook message.
