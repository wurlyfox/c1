# C1 #

This is a port of the game *Crash Bandicoot* to C.

The code is designed to compile for both psx and pc. At the moment only pc-specific code is working.

## Status ##

- Code - 100% ported; mostly untested
- Functionality - 55%

At the moment the game is only partially playable with a number of currently identified [issues](doc/issues.md).

Sound has been disabled as it is not working correctly, however the code is present. If you want to enable it (SFX only), add the `-DCFLAGS_SFX_FLUIDSYNTH` compilation flag to the appropriate section of the Makefile.

## Compiling ##

As of currently, compilation has only been tested on Ubuntu/Debian. However, the game should build on any system with a working 32-bit GNU toolchain and other dependencies listed below.

Note that only 32-bit target executables are currently supported. If you are not already running a 32-bit OS, you will need multilib versions of gcc and g++ and 32-bit versions of the dependencies.

To compile c1, install the dependencies listed below, `cd` into the main project directory, and run `make`.

### Dependencies ###

#### pc ####
- gcc / binutils / glibc
- GNU make
- OpenGL 2.0 or higher
- SDL 2
- SDL_Mixer 2.0
- FluidSynth
- Dear ImGui / cimgui (embedded in project)
- g++ / libstdc++ (for ImGui compilation)

Ubuntu/Debian:

- 64-bit: `sudo apt install build-essential gcc-multilib g++-multilib libstdc++6:i386 libgl1-mesa-dev:i386 libsdl2-dev:i386 libsdl2-mixer-dev:i386 libfluidsynth-dev:i386`
- 32-bit: `sudo apt install build-essential libstdc++6 libgl1-mesa-dev libsdl2-dev libsdl2-mixer-dev libfluidsynth-dev`

#### psx ####
(note: psx build currently does not work)

- Psy-Q

## Running ##

To run c1, `cd` into the main project directory and run `./c1`.

Copies of the asset files from the Crash Bandicoot game disc are needed for the game to function. Asset files are the (.NSD/.NSF) files located in the `/S0`, `/S1`, `/S2`, and `/S3` directories, respectively. Each of these files must be copied from its respective `/S*` directory into the `/streams` directory. Additionally, after copying files to the `/streams` directory, they must renamed from the uppercase format `S00000%%.NS[F,D]` to the lowercase format `s00000%%.ns[f,d]`.

The game is hardcoded to boot into level `25` (aka. title sequence/main menu screen(s)). To choose a different boot level, redefine the `LID_BOOTLEVEL` constant in `common.h` and recompile.

## Controls ##

- X - <kbd>Z</kbd>
- square - <kbd>X</kbd>
- circle - <kbd>C</kbd>
- triangle - <kbd>V</kbd>
- L1 - <kbd>A</kbd>
- R1 - <kbd>S</kbd>
- L2 - <kbd>Q</kbd>
- R2 - <kbd>W</kbd>
- d-pad up - <kbd>&#8593;</kbd>
- d-pad down - <kbd>&#8595;</kbd>
- d-pad left - <kbd>&#8592;</kbd>
- d-pad right - <kbd>&#8594;</kbd>
- start - <kbd>Enter</kbd>
- select - <kbd>Space</kbd>
- toggle in-game GUI - <kbd>Esc</kbd>
- toggle in-game GUI keyboard focus - <kbd>Tab</kbd>

## Structure ##
Project structure is listed below, including descriptions for each file.

Note that further documentation can be found in the `doc` directory.
```
.
├── doc                      # documentation
│   ├── changelog.md         # project changelog
│   ├── issues.md            # project issues
│   ├── coding.md            # project coding standards
│   ├── outline.md           # game outline
│   ├── funcs.md             # function descriptions
│   ├── files.md             # detailed file descriptions (incomplete)
│   └── memmap.xlsx          # memory map
├── src                      # source code
│   ├── formats              # entry [item] formats
│   │   ├── cvtx.h           # cvtx type entry item formats
│   │   ├── gool.h           # gool type entry item formats
│   │   ├── imag.h           # imag type entry item formats
│   │   ├── inst.h           # inst type entry item formats
│   │   ├── ipal.h           # ipal type entry item formats
│   │   ├── mdat.h           # mdat type entry item formats
│   │   ├── pbak.h           # pbak type entry item formats
│   │   ├── slst.h           # slst type entry item formats
│   │   ├── svtx.h           # svtx type entry item formats
│   │   ├── tgeo.h           # tgeo type entry item formats
│   │   ├── wgeo.h           # wgeo type entry item formats
│   │   └── zdat.h           # zdat type entry item formats
│   ├── pc                   # pc-specific code (non-inline)
│   │   ├── gfx              # pc graphics
│   │   │   ├── gl.c,h       # OpenGL backend (2D primitive rasterization)
│   │   │   ├── tex.c,h      # texture cache
│   │   │   └── pcgfx.h      # graphics primitives
│   │   │── sound            # pc sound
│   │   │   ├── audio.c,h    # audio/sfx backend
│   │   │   ├── midi.c,h     # midi backend
│   │   │   ├── util.c,h     # utility functions for SEQ/VAB conversion
│   │   │   └── formats      # audio and midi formats
│   │   │       ├── psx.h    # psx SEP, SEQ, VAB, and VAG formats
│   │   │       ├── sf2.h    # soundfont (sf2) format
│   │   │       └── smf.h    # standard midi format
│   │   │── init.c,h         # pc-specific initialization (incl. SDL)
│   │   │── math.c,h         # fixed point implementations of math functions
│   │   │── pad.c,h          # joypad emulation
│   │   └── time.c,h         # tick counter
│   ├── psx                  # psx-specific code (non-inline)
│   │   ├── card.c,h         # memory card
│   │   ├── cdr.c,h          # cd-rom
│   │   ├── gpu.c,h          # gpu interface (2D primitive rasterization)
│   │   ├── init.c,h         # psx-specific initialization
│   │   ├── r3000a.s         # r3000a assembly code; includes all functions from the game that were originally written in assembly
│   │   ├── r3000a.c         # c equivalents of the code in r3000a.s; here for sake of readability
│   │   └── r3000a.h         # header for r3000a.s and r3000a.c
│   ├── common.h             # common includes, macros, and codes
│   ├── geom.h               # geometric primitives (point, vector, angle, rect, matrix etc.)
│   ├── globals.h            # GOOL globals
│   ├── audio.c,h            # audio engine
│   ├── cam.c,h              # camera engine
│   ├── gfx.c,h              # upper-level graphics code
│   ├── gool.c,h             # GOOL object engine
│   ├── level.c,h            # level state and zone functions
│   ├── main.c,h             # main function and game loop
│   ├── math.c,h             # misc math functions
│   ├── midi.c,h             # midi/seq engine
│   ├── misc.c,h             # per-frame updates for world shader parameters
│   ├── ns.c,h               # paging/entry subsystem
│   ├── pad.c,h              # joypad interface
│   ├── pbak.c,h             # demo playback
│   ├── slst.c,h             # polygon id sort lists
│   ├── solid.c,h            # solidity and collision detection and movement limiting
│   ├── title.c,h            # handles title screen loading, transition between title states, and drawing of title card backdrops
│   ├── util                 # utilities
│   │   ├── list.c,h         # generic linked list
│   │   └── tree.c,h         # generic tree
│   └── ext                  # extensions to original game code
│       ├── lib              # libraries
│       │   ├── gui.c        # minimal GUI library ([c]imgui thin wrapper)
│       │   ├── refl.c       # minimal type reflection library
│       │   └── cimgui       # cimgui and imgui
│       ├── gui.c,h          # in-game GUI
│       ├── refl.c,h         # game-specific type metadata
│       └── disgool.c,h      # GOOL disassembler
├── streams                  # game assets (.NSD/.NSF files)
├── Makefile                 # project makefile
└── README.md                # project overview; this file
```
