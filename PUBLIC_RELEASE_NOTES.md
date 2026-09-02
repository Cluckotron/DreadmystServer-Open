# Public-source packaging notes

Prepared from the v0.2.8 community server development snapshot for use as a clean GitHub source repository.

Public-package changes only (no gameplay behavior intentionally changed):

- removed bundled original `game.db` and map data;
- added `setup_game_data.sh` so users can fetch/copy the required runtime data locally;
- removed generated build outputs and local account/character databases;
- added `.gitignore`, build instructions, security guidance, and provenance notices;
- changed public-facing private-development branding to community-server wording;
- no local account database, API token, private key, machine-specific home path, or known personal username is included.

Validation performed while preparing the bundle:

- server gameplay/source validator: **19/19 passed**;
- clean Ubuntu Release build: **passed** after supplying matching external `game.db` and maps;
- final public ZIP intentionally omits the fetched/copied `game/` folder and `Server/build/` output.
