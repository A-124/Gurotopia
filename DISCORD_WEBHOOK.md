# Discord webhook: lifecycle + multiplier event

This branch adds one-way Discord notifications for:
- Server startup (after the server has initialized and is ready).
- Server stopping (on a handled SIGINT/SIGTERM/SIGHUP). A restart appears as a stopping message followed by a startup message from the next process.
- Successful `/startmultiplier <gem> <xp> <seconds>` command execution.

## Configure

Set `GURO_DISCORD_WEBHOOK_URL` in the server's environment to the Discord channel's **Incoming Webhook URL**. Keep it secret; do not commit it to source control. If it is unset, the integration is disabled and the server works as before.

For Docker, pass it at runtime, for example:

```sh
docker run -e GURO_DISCORD_WEBHOOK_URL='https://discord.com/api/webhooks/REDACTED/REDACTED' ...
```

Do not paste the real webhook URL into logs, source files, or public issue reports.

## Message behavior

The sender runs on a background thread so the game loop does not wait for Discord's HTTP response. It verifies HTTPS and leaves libcurl's TLS certificate/hostname verification enabled. Messages are JSON-escaped, mentions are disabled, requests have timeouts, and a small queue/rate delay is used.

This sends a stop notice only when the process receives a handled termination signal. A hard crash, forced kill, or power loss cannot reliably send a final webhook message.
