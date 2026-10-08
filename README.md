# Gurotopia

**Gurotopia** — Lightweight & Maintained GTPS written in C/C++.

A Growtopia Private Server (GTPS) project based on the original Gurotopia repository, with custom commands and features added.

---

## Requirements

- MSYS2
- Visual Studio Code — install the C/C++ extension
- MariaDB Server

---

## Setup

### Windows (MSYS2)

1. Open your MSYS2 folder at C:\msys64, run ucrt64.exe, and execute:

   pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,openssl} make

2. Open the project in Visual Studio Code.
3. Press Ctrl + Shift + B to build.
4. Press F5 to run the server.

### Linux

Install dependencies:

   # Arch
   sudo pacman -S base-devel openssl mariadb-libs

   # Debian / Ubuntu
   sudo apt-get update && sudo apt-get install build-essential libssl-dev libmariadb-dev

Compile and run:

   make -j$(nproc)
   ./main.out

### macOS

Install dependencies via Homebrew:

   brew install gcc make openssl@3 mariadb-connector-c

Compile:

   make -j$(sysctl -n hw.ncpu) includes="-Iinclude -Ibuild/include -I$(brew --prefix openssl@3)/include" libraries="-L$(brew --prefix openssl@3)/lib -L$(brew --prefix mariadb-connector-c)/lib -L./include/enet/lib -L./include/mysql/lib -lssl -lcrypto -lmariadb -lenet_macos"

Run:

   ./main.out

---

## Local Server Configuration

Modify your hosts file to connect to your local server.

- Windows: C:\Windows\System32\drivers\etc\hosts
- Linux / macOS: /etc/hosts

Add:

   127.0.0.1 www.growtopia1.com
   127.0.0.1 www.growtopia2.com

---

## Custom Commands

| Command | Description |
|---------|-------------|
| /online or /on | Shows server statistics (online players, countries, uptime) |
| /help or /? | Shows the list of available commands |

---

## Credits

Based on the original Gurotopia project by the gurotopia team.

- Original Repository: https://github.com/gurotopia/Gurotopia
- Original Authors: https://github.com/gurotopia
- License: Apache-2.0

### Modifications in This Fork

- Custom /online command — Displays server statistics.
  - Original Lua concept by Albin
  - C++ port by A-124 (https://github.com/A-124)

---

## License

Licensed under the Apache-2.0 License — see LICENSE for details.

---

## Support

If you find this useful, give it a star on GitHub!

For questions or bug reports, open an Issue at https://github.com/A-124/Gurotopia/issues

---

**Enjoy your Gurotopia private server!**