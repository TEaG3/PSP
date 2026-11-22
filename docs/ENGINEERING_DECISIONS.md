# Engineering Decisions: RE4-Inspired PSP Prototype

## Developer lessons applied

This project uses broad design lessons from published interviews with the Resident Evil 4 developers. It does not copy source code, characters, level layouts, models, audio, or other protected game assets.

1. **Camera placement is a core gameplay decision.** Producer Hiroyuki Kobayashi described the long refinement needed to find the right over-the-shoulder camera position. We therefore tune follow distance, shoulder offset, height, easing, and aiming transition as one connected system rather than treating the camera as decoration.
2. **Controls, enemy behavior, and spaces must be designed together.** Producer Yoshiaki Hirabayashi explained that changes to movement flexibility required matching changes to enemy behavior and stage design to preserve the game's pacing. For this PSP prototype, movement speed, camera response, corridor width, arena size, and encounter timing must be tested together.
3. **Pacing matters more than constant intensity.** Hirabayashi described preserving the balance between tense action and short breaks. Our stage flow should alternate navigation, observation, short challenge sections, and quieter transition spaces instead of making every room equally busy.
4. **Readable spaces and player orientation matter.** The original developer interview discusses exploration, collecting resources, and maintaining awareness of the environment. Our stages should use landmarks, clear paths, visible exits, and optional side spaces so players can understand where they are going.

## Language and toolchain decisions

### C — PSP runtime

C remains the primary runtime language. The project already uses PSPSDK interfaces for the PSP operating system, GU/GUM graphics, controller input, file I/O, and PCM audio. Keeping the game loop, rendering, input, gameplay state, and save system in C avoids adding a second runtime or a foreign-function boundary to this small homebrew.

### Python — offline development tools

Python is used on the development/CI machine only. It is suitable for asset validation, model/texture metadata checks, content-manifest generation, and repeatable build checks. Python is not required on the PSP and is not shipped in EBOOT.PBP.

### C++ — optional, not a migration target

PSPDEV provides a C/C++ toolchain, but there is no current need to rewrite this project in C++. A migration would add risk without solving the present rendering and content gaps. Introduce C++ only if a well-defined subsystem requires it and can be isolated behind a small C-compatible API.

### Asset creation

Original low-poly art can be created in a 3D editor and exported to the OBJ/PNG formats supported by the current loader. The asset validator checks the formats the current C loader can consume; it does not create or invent art assets.

## Runtime engineering practice

- Use elapsed time for movement and camera interpolation so behavior does not depend on the current frame rate.
- Keep PSP rendering and game-state code in C; run heavyweight content checks offline in Python.
- Profile on PPSSPP and, when available, PSP hardware. Never infer device performance from a successful compile.
- Treat CI as a compile/test gate, not proof that the scene is visible or the game is playable.
- Preserve an English-first interface while the rendering and gameplay foundations are being stabilized.

## Source notes

- Hiroyuki Kobayashi, producer, interview about the original Resident Evil 4 development (Nintendo Dream interview, English translation): https://shmuplations.com/residentevil4/
- GamesRadar+, “The making of Resident Evil 4”, including Kobayashi's description of refining the camera position: https://www.gamesradar.com/making-of-resident-evil-4/
- Xbox Wire, interview with Resident Evil 4 producer Yoshiaki Hirabayashi, March 10, 2023: https://news.xbox.com/en-us/2023/03/10/talking-resident-evil-4-updated-combat/
- PSPDEV project homepage and SDK documentation: https://pspdev.github.io/
- PSPSDK source repository: https://github.com/pspdev/pspsdk
