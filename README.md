<div align="center">

**グロートピア** *(Gurotopia)* : Lightweight & Maintained GTPS written in C/C++

[![](https://github.com/GT-api/GT.api/actions/workflows/make.yml/badge.svg)](https://github.com/GT-api/GT.api/actions/workflows/make.yml)
[![Dockerfile](https://github.com/gurotopia/Gurotopia/actions/workflows/docker.yml/badge.svg)](https://github.com/gurotopia/Gurotopia/actions/workflows/docker.yml)
[![](https://app.codacy.com/project/badge/Grade/fa8603d6ec2b4485b8e24817ef23ca21)](https://app.codacy.com/gh/gurotopia/Gurotopia/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![](https://dcbadge.limes.pink/api/server/zzWHgzaF7J?style=flat)](https://discord.gg/zzWHgzaF7J)

</div>

---
# <img width="250" height="53" alt="image" src="https://github.com/user-attachments/assets/0a3bee67-ad6c-4e4c-bd0e-aed89d9b5c09" />

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/archive.svg) 1. Requirements
   - [**MSYS2**](https://www.msys2.org/)
   - [**Visual Studio Code**](https://code.visualstudio.com/): install [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) for VSCode
   - [**MariaDB Server**](https://mariadb.org/download/?t=mariadb&p=mariadb&r=12.2.2&os=windows&cpu=x86_64&pkg=msi&mirror=accretive)

### 2. Setup MSYS2
   - Locate your MSYS2 folder at `C:\msys64`, open `ucrt64.exe`, and run the following command:
   
     ```bash
     pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,openssl} make
     ```

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/build.svg) 3. Compile
   - Open the project folder in Visual Studio Code.
   - Press **`Ctrl + Shift + B`** to start the build process.

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/debug-alt-small.svg) 4. Run
   - After compiling, press **`F5`** to run the server!

# <img src="https://github.com/user-attachments/assets/fecde323-04c5-4b82-a08d-badcb184be6a" width="30" /> Linux

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/archive.svg) 1. Install Dependencies

- Enter command associated with your distribution into the terminal to install nessesary tools.
   <details><summary><img width="22" height="22" src="https://github.com/user-attachments/assets/8359ba6e-a9b2-4500-893f-61eaf40e2478" /> Arch</summary>
   <p>
      
   ```bash
   sudo pacman -S base-devel openssl mariadb-libs
   ```
   </p>
   </details> 
   <details><summary><img width="22" height="22" src="https://github.com/user-attachments/assets/742f35c4-3e69-450e-8095-9fabe9ecd0d8" /> Debian <img width="18" height="18" src="https://github.com/user-attachments/assets/46f0770e-f4ed-480b-851d-c90b05fae52f" /> Ubuntu</summary>
   <p>
      
   ```bash
   sudo apt-get update && sudo apt-get install build-essential libssl-dev libmariadb-dev
   ```
        
   </p>
   </details> 
### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/build.svg) 2. Compile
   - Navigate to the project's root directory in your terminal and run the `make` command:
   
     ```bash
     make -j$(nproc)
     ```
### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/debug-alt-small.svg) 3. Run
   - Execute the compiled binary located in the `main` directory:
   
     ```bash
     ./main.out
     ```
# macOS

> [!NOTE]
> macOS builds are only tested on Intel (`x86_64`). Apple Silicon is not tested yet.
> `include/enet/lib/libenet_macos.a` is a universal (`x86_64` + `arm64`) archive, so it should still link on Apple Silicon.

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/archive.svg) 1. Install Dependencies

- Install [Homebrew](https://brew.sh/) or [MacPorts](https://www.macports.org/install.php), then run the matching command. Mixing is fine: MacPorts GCC with Homebrew OpenSSL and MariaDB uses the Homebrew compile command.
   <details><summary>Homebrew</summary>
   <p>
      
   ```bash
   brew install gcc make openssl@3 mariadb-connector-c
   ```
   </p>
   </details>
   <details><summary>MacPorts</summary>
   <p>
      
   ```bash
   sudo port install gcc15 gmake openssl3 mariadb-11.4
   ```
   </p>
   </details>

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/build.svg) 2. Compile

- Apple's SDK does not include OpenSSL headers. Pass Homebrew or MacPorts paths on the `make` line. A plain `make` fails with `openssl/ssl.h: No such file or directory`. Use `-lenet_macos`; the default `-lenet` is a Linux library.
   <details><summary>Homebrew</summary>
   <p>
      
   ```bash
   make -j$(sysctl -n hw.ncpu) \
     includes="-Iinclude -Ibuild/include -I$(brew --prefix openssl@3)/include" \
     libraries="-L$(brew --prefix openssl@3)/lib -L$(brew --prefix mariadb-connector-c)/lib -L./include/enet/lib -L./include/mysql/lib -lssl -lcrypto -lmariadb -lenet_macos"
   ```
   </p>
   </details>
   <details><summary>MacPorts</summary>
   <p>
      
   ```bash
   gmake -j$(sysctl -n hw.ncpu) \
     includes="-Iinclude -Ibuild/include -I/opt/local/libexec/openssl3/include" \
     libraries="-L/opt/local/libexec/openssl3/lib -L/opt/local/lib/mariadb-11.4/mysql -L./include/enet/lib -L./include/mysql/lib -lssl -lcrypto -lmariadb -lenet_macos"
   ```
   </p>
   </details>

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/debug-alt-small.svg) 3. Run
   - Execute the compiled binary:
   
     ```bash
     ./main.out
     ```
# ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/settings.svg) Local Server Configuration

> [!NOTE]
> To connect to your local server, you must modify your system's **hosts** file.
> - **Windows**: `C:\Windows\System32\drivers\etc\hosts`
> - **Linux/macOS**: `/etc/hosts`
> ```
> 127.0.0.1 www.growtopia1.com
> 127.0.0.1 www.growtopia2.com




# ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/heart.svg) Credits

- Original Repository: [https://github.com/gurotopia/Gurotopia](https://github.com/gurotopia/Gurotopia)
- Original Authors: [gurotopia](https://github.com/gurotopia)

# ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/law.svg) License

Licensed under the **Apache-2.0 License** — see [LICENSE](LICENSE) for details.


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
