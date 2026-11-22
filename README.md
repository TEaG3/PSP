# RE4 Proto — PSP

A small homebrew 3D action prototype for PSP and PPSSPP. The game menus and HUD are Arabic-first, with an embedded bitmap font; no runtime font download is required.

## Project structure

- \`main.c\` — PSP game loop, arena, targets, controls, HUD and menus.
- \`arabic_font.c/.h\` — embedded bitmap font renderer, Arabic contextual forms, right-to-left line layout and mixed Arabic/Latin text.
- \`arabic_font_data.h\` — Arabic glyph bitmap data.
- \`arabic_font_ascii_data.h\` — printable ASCII glyph bitmap data.
- \`obj_loader.c/.h\` — Wavefront OBJ position/UV reader with polygon triangulation.
- \`texture.c/.h\` — PNG loader via \`stb_image.h\` and PSP GU texture binding.
- \`audio.c/.h\` — generated fallback sound effects sent through non-blocking audio output.
- \`savedata.c/.h\` — validated save/load data in the PSP savedata folder.
- \`Makefile\` — PSPSDK build configuration.
- \`assets/\` — optional player/enemy OBJ and PNG files.

## Arabic rendering

The bitmap font is embedded in the repository. It supports Arabic base letters with their common contextual presentation forms, printable ASCII, Arabic-Indic digits, and a limited punctuation set. The renderer places Arabic runs right-to-left and keeps basic Latin/digit runs in their expected order.

This is a lightweight game UI renderer, not a full Unicode bidirectional implementation. Combining Arabic vowel marks are currently skipped, and complex punctuation/paragraph-direction cases should be visually checked on PPSSPP.

## Build

Use GitHub Codespaces or VS Code Dev Containers with \`pspdev/pspdev:latest\`. The GitHub Actions workflow also builds the EBOOT on pushes and pull requests for \`main\` and \`feature/arabic-localization\`.

In the PSPDEV environment:

\`\`\`sh
curl -fsSL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o stb_image.h
make check-deps
make
\`\`\`

The build output is \`EBOOT.PBP\`. GitHub Actions uploads it as the \`RE4Proto-EBOOT\` artifact when the build succeeds.

## Install and controls

Copy \`EBOOT.PBP\` to \`PSP/GAME/RE4Proto/EBOOT.PBP\`, then launch it from the PSP Game menu or PPSSPP.

- Analog stick or D-pad: move and turn.
- X or R trigger: fire.
- L trigger: aim, show the laser line and increase damage.
- Square: reload.
- Select: quick save.
- Start: pause or resume.
- Circle: return to the title menu; on the title screen it exits.
- Triangle: save while paused.
- Home: exit the game.

## Optional assets

Place these files in \`assets/\` to replace the fallback geometry:

- \`player.obj\`, \`enemy.obj\`
- \`player.png\`, \`enemy.png\`

If these files are absent, the game falls back to primitive shapes. Sound effects are generated internally; external raw sound files are not required.

## Scope and testing

This repository is a compact prototype, not a complete remake of Resident Evil 4. Its gameplay, visuals, collision and sound remain intentionally limited. A successful compile confirms only that the program builds; test the generated EBOOT in PPSSPP and then on PSP hardware before treating the release as device-verified.
