## Custom content

Custom item and recipe definitions are loaded from `resources/custom_items.txt`.
The server validates the complete file before replacing the active custom-content
registry. If validation fails, the previous registry remains active.

### Custom item format

```text
item|id|name|base_item|type|rarity|tradeable|texture_path|info
```

- `id`: custom item ID (1000–65535), outside the vanilla item database.
- `name`: display name.
- `base_item`: an existing vanilla item whose item data is cloned.
- `type`: item type value (0–255).
- `rarity`: rarity value (0–32767).
- `tradeable`: `1` to allow trading, `0` to mark the item untradeable.
- `texture_path`: optional client texture path; the server looks for its basename in `resources/custom_assets/`.
- `info`: optional item information. The `|` character is the field separator.

### Recipe format

```text
recipe|result_id|result_amount|ingredient_id:amount,ingredient_id:amount
```

Recipe IDs must refer to known vanilla or custom items. Amounts must be positive;
the output amount is limited to 200. Duplicate item IDs, duplicate recipe outputs,
unknown definition types, invalid fields, and missing texture files are validation
errors.

### Runtime commands

- `/content` — show currently loaded content counts.
- `/content validate` — validate custom items and recipes and print actionable errors.
- `/reload content` — reload custom content, quests, and achievements after validation.
- `/reload all` — reload the supported runtime data sets.

Run `/content validate` before reloading after editing the file.


## Titles, profile and UI features

### Titles
Titles are defined in `resources/titles.txt` (`id|name|color|requirement|description`). Players unlock them by
playing (level, achievements, quests, daily streak, play time, account age, fires put out, role) or when a developer
uses `/givetitle <player|me> <id>`. Open them with the wrench menu **Title** button, or `/titles`.

- Tap an unlocked title to wear it; it shows in front of the name tag for everyone in the world (`[Builder] name`).
- **Disable Title / Enable Title** turns the display off and on without losing the selected title.
- The *Locked* tab shows the requirement and a progress bar for every title you have not unlocked yet.
- Unlocked titles, the equipped title, and the on/off switch are saved per account (new `peer` columns are added automatically on startup).
- Edit the file and run `/reload content` to add or change titles. Unlocked titles are never taken away.

### Wrench menu buttons now connected
Title, Notebook, Personalize Player Profile (about-me line), Wardrobe (worn and owned clothing, take everything off),
Growmojis, World Lock Bank, Marvelous Missions (opens the Gurotopia Hub), plus *View worn clothes* and *Send Message* on other players.
The profile also shows real play time, account age, active effects and the player's title.

### New commands
`/titles`, `/givetitle` (developer), `/leaderboard` (`/top`, `/lb`: levels and play time), `/playtime`, `/notebook`.
`/help` is now a categorised dialog that only lists commands your role can use.

### Shared dialog theme
`include/tools/ui.hpp` holds the colours and helpers (`header`, `section`, `bar`, `footer`, `sanitize`) used by the new dialogs.

# Deployment (Docker, recommended)

```bash
cp .env.example .env          # set DB_PASSWORD and PUBLIC_ADDRESS
docker compose up -d --build
docker compose logs -f gurotopia
```

- Open **17091/udp** (game) and **443/tcp** (login / server list) on your firewall.
- `docker compose stop` sends SIGTERM; the server saves every player and world before exiting (30 s grace period).
- Staff actions are recorded in `logs/audit.log` (who ran which moderator / developer command, UTC).
- The database starts empty on first boot. Make a backup routine for the `db_data` volume, e.g.
  `docker compose exec db mariadb-dump -uroot -p"$DB_PASSWORD" --databases gurotopia > backup.sql`

### Configuration without files

| Variable | Meaning | Default |
|---|---|---|
| `GURO_DB_HOST` / `GURO_DB_PORT` | MariaDB address | `127.0.0.1` / `3306` |
| `GURO_DB_USER` / `GURO_DB_PASSWORD` | MariaDB login | `root` / empty |
| `GURO_SERVER_ADDR` | address players connect to | from `server_data.php` |
| `GURO_PORT` | game port (udp) | `17091` |
| `GURO_MAX_PEERS` | maximum connected players (1-1024) | `50` |

Environment variables win over `mysql_login.txt` and `server_data.php`. If the database is not ready yet the
server retries for about a minute before giving up with a clear error.

### Before you go public

- Never run with an empty database password. Create a dedicated MariaDB user instead of root if the database is shared.
- `resources/ctx/server.key` is a private key stored in the repository. Generate your own certificate pair if the
  server is exposed to the internet and your client build allows it.
- Run `/reload content` after editing anything in `resources/`.

## New in this update

- **Titles:** 12 titles instead of 37. The title keeps its own colour (Developer is black) and is no longer
  recoloured by the client's level 125 name colour. Rebuilt `/titles` window: live name preview, hide / show,
  nearest goals first with progress bars.
- **Player commands:** `/status`, `/flip`, `/ping`, `/uptime`, `/count`.
- **Staff commands:** `/mute <player> [minutes]`, `/unmute`, `/pinfo <player>`, `/nick <name>`, `/default`.
- **Audit log** for every moderator and developer command.

### Client-visible polish (this update)

- **Toasts:** a Growtopia-style notification slides in for level ups, quest / achievement completion, new titles and the daily reward.
- **Message of the day:** `resources/motd.txt`, shown in the console at login. New accounts also get a welcome window (`/welcome` reopens it).
- **World menu:** Top Worlds are sorted by player count (top 10) and Recently Visited shows the newest world first.
- **Fix:** quest / achievement completion messages printed a stray "w" in front of the name.
