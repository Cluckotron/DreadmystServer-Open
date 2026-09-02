# Building the Dreadmyst Community Server

## Supported development environment

The maintained build path is Ubuntu Linux or Ubuntu under WSL2.

## Requirements

Install the compiler, CMake, Git, SQLite development package, and Boost headers:

```bash
sudo apt update
sudo apt install -y build-essential cmake git libsqlite3-dev libboost-dev
```

The project uses C++17. Boost.Asio is used header-only for the current networking layer.

## 1. Obtain game data

The public source package does not redistribute original Dreadmyst runtime data.

Automatic fetch:

```bash
./setup_game_data.sh
```

This clones the public Dreadmyst runtime into the gitignored `.runtime_source/` cache and copies only `game.db` and `maps/` into the gitignored `game/` directory required by the server.

If you already have a runtime:

```bash
DREADMYST_RUNTIME_DIR=/path/to/dreadmyst ./setup_game_data.sh
```

Required source-runtime files:

```text
game.db
maps/
```

## 2. Build

```bash
./build_ubuntu.sh
```

`build_ubuntu.sh` automatically calls `setup_game_data.sh` if `game/game.db` or `game/maps/` is missing.

Manual equivalent:

```bash
cmake -S Server -B Server/build -DCMAKE_BUILD_TYPE=Release
cmake --build Server/build -j"$(nproc)"
```

Expected executable:

```text
Server/build/DreadmystServer
```

## 3. Run

```bash
./run_ubuntu.sh
```

The server starts from `Server/build/` so the relative data paths in `Server/data/server.ini` resolve correctly.

Default configuration:

```ini
[Server]
Port=16383
MaxConnections=100

[Database]
GameDbPath=../../game/game.db
MapsPath=../../game/maps
ServerDbPath=data/server.db
```

## 4. Reset development accounts/characters

Stop the server first, then:

```bash
./reset_server_db.sh
```

The account/character database is recreated automatically the next time the server starts.

## Running with the Windows client under WSL2

On many Windows installations `127.0.0.1:16383` forwards to WSL automatically. If it does not, get the current WSL IPv4 address:

```bash
hostname -I
```

Use the first IPv4 address when preparing the client runtime.

## Troubleshooting

### `SQLite3 not found`

```bash
sudo apt install -y libsqlite3-dev
```

### Boost headers missing

```bash
sudo apt install -y libboost-dev
```

### `game/game.db` missing

```bash
./setup_game_data.sh
```

### Port already in use

Check what owns port 16383:

```bash
ss -ltnp | grep 16383
```

Or edit `Server/data/server.ini` and prepare the client with the same port.
