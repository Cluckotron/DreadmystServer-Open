# Third-party and upstream notice

This community server interfaces with Dreadmyst protocol/runtime data and uses third-party components including SQLite3 and Boost.Asio.

For preservation and reproducibility, this self-contained server snapshot includes the matching Dreadmyst runtime `game.db` and map files under `game/`. The server build/startup path does not fetch those files from an upstream repository.

These preserved runtime files are original Dreadmyst material. Their inclusion here is not a claim of authorship, trademark ownership, endorsement, or a new license grant. The package retains the license file that was already present in the community server source snapshot, but that license does not automatically relicense third-party libraries, original game data/assets, trademarks, or upstream material owned by others.

Client-side visual/audio assets (player/NPC sprites, terrain textures, item/spell icons, UI art, sounds, etc.) are not required by the headless server and are preserved separately in the community runtime-assets repository.

This repository is community work and is not represented as an official Dreadmyst release or endorsement.
