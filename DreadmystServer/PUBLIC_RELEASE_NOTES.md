# Public-source packaging notes

Prepared from the v0.2.8 community server development snapshot for use as a clean, self-contained GitHub source repository.

Public-package changes only (no gameplay behavior intentionally changed):

- bundled the matching preserved `game/game.db` directly in the repository;
- bundled all 28 matching `.map` files under `game/maps/`;
- removed the runtime dependency on `https://github.com/DreadmystRPG/steam`;
- converted `setup_game_data.sh` from a network fetcher into a local integrity checker;
- added `GAME_DATA_SHA256.txt` so the preserved database/map revision can be verified later;
- changed `.gitignore` so `game/` is intentionally tracked;
- removed generated build outputs and local account/character databases;
- retained build instructions, security guidance, and provenance notices;
- no local account database, API token, private key, machine-specific home path, or known personal username is included.

Validation performed while preparing the self-contained bundle:

- server gameplay/source validator: **19/19 passed**;
- bundled game data: `game.db` + **28 maps** present;
- game-data SHA256 verification: **passed**;
- clean Ubuntu Release build: **passed** using only the files contained in this repository;
- final public ZIP intentionally omits `Server/build/` and all generated `server.db` files.
