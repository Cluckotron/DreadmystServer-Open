# Building the Dreadmyst Community Server

## Supported development environment

The maintained build path is Ubuntu Linux or Ubuntu under WSL2.

## Requirements

Install the compiler, CMake, SQLite development package, and Boost headers:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsqlite3-dev libboost-dev
```

The project uses C++17. Boost.Asio is used header-only for the current networking layer.

## 1. Verify the bundled game data

The repository is self-contained and already includes:

```text
game/game.db
game/maps/*.map
```

Verify the files and their SHA256 hashes with:

```bash
bash setup_game_data.sh
```

Despite its historical name, `setup_game_data.sh` no longer clones or downloads anything. It only checks the content stored in this repository.

You can also verify the manifest manually:

```bash
sha256sum -c GAME_DATA_SHA256.txt
```

## 2. Build

```bash
bash build_ubuntu.sh
```

The build helper verifies the included game data first, configures a Release build, then compiles the server.

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
bash run_ubuntu.sh
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

No external Dreadmyst GitHub repository is consulted during build or startup.

## 4. Reset development accounts/characters

Stop the server first, then:

```bash
bash reset_server_db.sh
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

### `game/game.db` or maps missing

A normal clone of this repository should already contain them. Check:

```bash
git status
git ls-files game/game.db game/maps
bash setup_game_data.sh
```

If the files were removed locally, restore them from Git:

```bash
git restore game/game.db game/maps
```

### Game-data checksum failure

If `bash setup_game_data.sh` reports a checksum mismatch, do not mix data revisions. Restore the preserved files from the same repository commit/release as the server source.

### Port already in use

Check what owns port 16383:

```bash
ss -ltnp | grep 16383
```

Or edit `Server/data/server.ini` and prepare the client with the same port.
