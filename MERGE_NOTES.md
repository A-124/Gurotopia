# Merge notes: main + other

Base: **main** (your server). Features from **other** were added where main lacked them.
Where both had the same feature, main's version was kept, except titles (other's, as requested).

## Titles
- Main's title system removed: `gameplay/title_system.*`, `commands/title_commands.*`, `resources/titles.txt`.
- Other's titles are in: `commands/legendary.*` ("of Legend" from Quest For Honor, "Dr.", etc.).
- `/title` (player: wrench > Title, or `/title` with arguments for staff) is other's command.
- `/playtime` and `/notebook` moved to `commands/profile_commands.*` (they were in main's title file).
- Achievements, quests and daily no longer award titles.

## Commands added from other (registered in `commands/__command.cpp`)
access, accounts, announce, bot, breakall, clear, cleardrops, curse, deleteallworlds, deleteworld,
ducttape, dumpitems, fill, fillworld, firework, flag, freeze, gems, give, god, heal, invis, list, nuke,
private, removeblock, renameworld, reset, save, search, setowner, showplayer, shutdown, spawnall,
sphere, summon, swap, tile, uncurse, undo, undress, unnuke, warpto, wear, whois, worlds, xp.

## Overlapping features (main kept)
help, ?, find, ghost, me, news, punch, sb, skin, time, uptime, warp, weather, who, kick, unban, pull,
msg / r / w, maintenance (main's gServer_data.maintenance), nick (main's staff /nick), event (main's event manager),
role changes (main's /setrole). Other's /mod, /nick, /msg, /reply, /dev, /devmenu, /events, /motd,
/maintenance and /level were not registered because main already covers them.
Other's /trade command stays unregistered; main's trade dialog flow is kept.

## Core state and behaviour ported from other
- `peer`: god_mode, frozen, instant_break, curse_until, freeze_until, ban_until, item_states, buffs,
  god_rejoin, renamed_at, tint, move, last_msg_from, block_history, nickname; pstate S_FROZEN and S_INVISIBLE.
- `world`: nuked, base_weather, treasure_spots, save(). New DB columns via ensure_column.
- Join: arrivals (summon/warpto land where you stand), curse / HELL rules, nuked-world check, invisible spawns.
- Leave: delayed OnRemove queue (leave_pump), leave_flush on re-entry.
- Break/place: block history for /undo, /breakall instant break, god-mode lava immunity.
- Visuals: invisibility, ghost, frozen, curse and item effects in SetClothing; bot SetClothing overload.
- CountryStateOf, /god item data (harmful_type_pos, im_file_offset in items.cpp).
- Login: ban_until check (banned players are disconnected with the remaining time).

## Dropped as duplicates
commands/dev, devmenu, mod, nick, msg, motd, maintenance (from other).

## Not verified here
- Built with g++ 13 against the vendored enet and the vendored headers.
- MariaDB and libcurl headers/libs are not installed in the build environment. Link was checked with
  stub libraries for those two only (undefined-symbol check). Real libraries are needed for `make`.
- No database or game client was available, so runtime behaviour is not tested.
