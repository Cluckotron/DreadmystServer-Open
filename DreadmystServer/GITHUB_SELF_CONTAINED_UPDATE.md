# Updating an Existing GitHub Server Repository

If you previously published the source-only v0.2.8 server, copy the files from this self-contained package over that checkout and commit the bundled game data.

The important additions/changes are:

```text
game/game.db
game/maps/*.map
GAME_DATA_SHA256.txt
setup_game_data.sh
build_ubuntu.sh
run_ubuntu.sh
README.md
BUILDING.md
.gitignore
THIRD_PARTY_NOTICE.md
PUBLIC_RELEASE_NOTES.md
```

From the root of your existing Git checkout:

```bash
git add game GAME_DATA_SHA256.txt setup_game_data.sh build_ubuntu.sh run_ubuntu.sh \
        README.md BUILDING.md .gitignore THIRD_PARTY_NOTICE.md \
        PUBLIC_RELEASE_NOTES.md GITHUB_SELF_CONTAINED_UPDATE.md

git status
git commit -m "Make Dreadmyst server self-contained"
git push
```

After pushing, verify Git actually tracks the preserved data:

```bash
git ls-files game/game.db 'game/maps/*.map'
```

A fresh clone should then be able to build without contacting any Dreadmyst upstream repository:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsqlite3-dev libboost-dev
bash setup_game_data.sh
bash build_ubuntu.sh
bash run_ubuntu.sh
```

`setup_game_data.sh` is now an integrity checker only. It does not download or clone anything.
