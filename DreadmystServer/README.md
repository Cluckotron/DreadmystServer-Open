# Dreadmyst Community Server

Experimental headless community server compatible with the accompanying r1189-derived Dreadmyst community client work.

This package corresponds to the v0.2.8 development snapshot. It includes Linux/WSL2 C++17 build support, Boost.Asio networking, SQLite persistence, world/entity simulation, movement, NPCs, inventory/combat foundations, quests/social systems, and the gameplay compatibility repairs developed for the community client.

## Self-contained game data

This repository now includes the matching preserved server runtime data directly:

```text
game/
├── game.db
└── maps/
```

A fresh clone therefore does **not** need to download `game.db` or maps from `DreadmystRPG/steam` or any other external Dreadmyst repository. `setup_game_data.sh` is retained as a local integrity/presence check only; it performs no network download.

The server does not need the client-side sprite/audio packs to boot. Those are preserved separately in the community runtime-assets project.

## Quick start on Ubuntu / WSL2

```bash
sudo apt update
sudo apt install -y build-essential cmake libsqlite3-dev libboost-dev

bash build_ubuntu.sh
bash run_ubuntu.sh
```

`build_ubuntu.sh` and `run_ubuntu.sh` both verify the bundled game data automatically. You can also run `bash setup_game_data.sh` by itself when you only want to check integrity.

The default server listens on TCP port `16383`.

See [BUILDING.md](BUILDING.md) for detailed setup and troubleshooting.

If you already published the earlier source-only package, see [GITHUB_SELF_CONTAINED_UPDATE.md](GITHUB_SELF_CONTAINED_UPDATE.md) for the exact files and Git commands needed to convert that repository to the self-contained layout.

## What is generated locally

Player/account state is **not** part of the repository. The server creates its local account/character database at runtime under the build tree and Git ignores it.

To wipe development accounts/characters:

```bash
bash reset_server_db.sh
```

## Preserved content revision

`GAME_DATA_SHA256.txt` contains SHA256 checksums for the bundled `game/game.db` and every bundled `.map` file. This lets future users verify that the preserved data has not silently changed.

## Security warning

This is still a development/LAN server. The current local authentication implementation is not suitable for direct public-Internet hosting. See [SECURITY.md](SECURITY.md).

## License / provenance

The server package retains the license file that was already present in the community server snapshot. The bundled `game.db` and map data are preserved Dreadmyst runtime material and are not claimed as newly authored or relicensed by this community project. Some shared protocol/data definitions and dependencies also have their own provenance; read [THIRD_PARTY_NOTICE.md](THIRD_PARTY_NOTICE.md) before redistribution.
