# Dreadmyst Community Server

Experimental headless community server compatible with the accompanying r1189-derived Dreadmyst community client work.

This package corresponds to the v0.2.8 development snapshot. It includes Linux/WSL2 C++17 build support, Boost.Asio networking, SQLite persistence, world/entity simulation, movement, NPCs, inventory/combat foundations, quests/social systems, and the gameplay compatibility repairs developed for the community client.

Original Dreadmyst `game.db` and map data are **not bundled** in this public-source package. `setup_game_data.sh` obtains them from the public Dreadmyst runtime or copies them from a local runtime supplied by the user.

## Quick start on Ubuntu / WSL2

```bash
sudo apt update
sudo apt install -y build-essential cmake git libsqlite3-dev libboost-dev

./setup_game_data.sh
./build_ubuntu.sh
./run_ubuntu.sh
```

The default server listens on TCP port `16383`.

See [BUILDING.md](BUILDING.md) for detailed setup and troubleshooting.

## Existing runtime instead of downloading

If you already have the Dreadmyst runtime locally:

```bash
DREADMYST_RUNTIME_DIR=/path/to/dreadmyst ./setup_game_data.sh
```

The runtime directory must contain `game.db` and `maps/`.

## Local database

Player/account state is generated at runtime under the build tree and is ignored by Git. To wipe development accounts/characters:

```bash
./reset_server_db.sh
```

## Security warning

This is still a development/LAN server. The current local authentication implementation is not suitable for direct public-Internet hosting. See [SECURITY.md](SECURITY.md).

## License / provenance

The server package retains the license file that was already present in the community server snapshot. Some shared protocol/data definitions interface with Dreadmyst material and dependencies that have their own provenance; read [THIRD_PARTY_NOTICE.md](THIRD_PARTY_NOTICE.md) before redistribution.
