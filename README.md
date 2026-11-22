# RE4 Proto — PSP

A small homebrew 3D prototype targeting the PlayStation Portable (PSP) and PPSSPP.

## Included project structure

- `.devcontainer/devcontainer.json` — PSPDEV development container.
- `main.c` — game loop, controls, simple arena, targets, and GU rendering.
- `obj_loader.c/.h` — Wavefront OBJ vertex/UV loader.
- `texture.c/.h` — PNG loading through `stb_image.h` and PSP GU texture binding.
- `audio.c/.h` — generated fallback sound effects.
- `savedata.c/.h` — basic save/load support.
- `assets/` — optional custom models, textures, and audio files.
- `Makefile` — PSPSDK build configuration.
- `requirements.txt` — toolchain and external dependency notes.

## Build

The recommended environment is GitHub Codespaces or VS Code Dev Containers using
`pspdev/pspdev:latest`. The container setup downloads `stb_image.h` automatically.

Then run:

```sh
make check-deps
make
```

The build should produce `EBOOT.PBP` in the project root.

## Install

Copy `EBOOT.PBP` to:

```
PSP/GAME/RE4Proto/EBOOT.PBP
```

Run it from the PSP Game menu or open it in PPSSPP.

## Controls

- Analog stick / D-pad: move and turn.
- X: fire at a target in front of the player.
- Square: reload.
- Select: quick save.
- Start: pause / resume.
- Circle: return from pause or exit the title screen.
- Home: exit.

## Optional assets

Place custom assets in `assets/`:

- `player.obj`, `enemy.obj`
- `player.png`, `enemy.png`
- `gunshot.raw`, `hit.raw`

The current prototype uses generated sound effects and primitive geometry when optional
assets are absent. Raw audio files are listed for future integration; the current audio
module generates its fallback effects internally.

## Scope and limitations

This repository contains a starter prototype, not a complete remake of Resident Evil 4.
The current gameplay, visuals, collision, and audio are intentionally minimal. Test on
PPSSPP first, then validate on real PSP hardware. A successful compile is not proof of
correct performance on every PSP model.
