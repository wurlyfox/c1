# C1 #

This is a port of the game *Crash Bandicoot* to C.

The code is designed to compile for both psx and pc. At the moment only pc-specific code is working. Furthermore the code will only compile for 32-bit targets; 64-bit is not yet supported.

## Status ##

- Code - 100% ported; mostly untested
- Functionality - 30%

At the moment the game is only partially playable. Current issues include:

- collision issues
- inaccurate object lighting/colors
- failure to render some objects
- camera lag
- incorrect display list ordering of objects
- unimplemented transparency
- no rejection of object geometry outside of viewing frustum
- crash upon occurrence of various events (death, collecting 3 doctor, etc.)
- various GOOL issues
- different behavior when compiled with optimization flags

Sound has been disabled as it is not yet working, however the code is present.

## Compiling ##

As of currently, compilation has only been tested on 32-bit Debian. However, the game should build on any 32-bit system with a working GNU toolchain and other dependencies listed below.

### Dependencies ###

#### pc ####
- gcc / binutils / glibc
- GNU make
- OpenGL 2.0 or higher
- SDL 2
- SDL_Mixer 2.0
- FluidSynth
- Freetype (for in-game UI)

Ubuntu/Debian:
`sudo apt install build-essential libgl1-mesa-dev libsdl2-dev libsdl2-mixer-dev libfluidsynth-dev libfreetype-dev`

#### psx ####
(note: psx build currently does not work)

- Psy-Q

## Running ##

To run c1, `cd` into the main project directory and run `./c1`.

Copies of the asset files from the Crash Bandicoot game disc are needed for the game to function. Asset files are the (.NSD/.NSF) files located in the `/S0`, `/S1`, `/S2`, and `/S3` directories, respectively. Each of these files must be copied from its respective `/S*` directory into the `/streams` directory. Additionally, after copying files to the `/streams` directory, they must renamed from the uppercase format ``S00000%%.NS[F,D]` to the lowercase format `s00000%%.ns[f,d]`.

The game is currently hardcoded to boot straight into level `09` (aka. N. Sanity Beach), bypassing the main menu screen(s). To choose a different boot level, redefine the `LID_BOOTLEVEL` constant in `common.h` and recompile.

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

## Structure ##
Project structure is listed below, including descriptions for each file.

Note that further documentation can be found in the `doc` directory.
```
.
├── doc                      # documentation
│   ├── coding.md            # project coding standards
│   ├── outline.md           # game outline
│   ├── funcs.md             # function descriptions
│   ├── files.md             # detailed file descriptions (incomplete)
│   └── memmap.xlsx          # memory map
├── src                      # source code
│   ├── formats              # entry [item] formats
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
│   │   ├── tree.c,h         # generic tree       
│   ├── ext                  # extensions to original game code
│   │   ├── lib              # libraries
│   │   │   ├── gui.c        # minimal GUI library
│   │   │   └── refl.c       # minimal type reflection library    
│   │   ├── gui.c,h          # in-game GUI
│   │   ├── refl.c,h         # game-specific type metadata    
│   │   └── disgool.c,h      # GOOL disassembler
│   └── patches              # patching of functions with code generated by c1c's discompiler
│       ├── emu.c,h          # modified version of c1c's emulator
│       ├── zz.c,h           # stubs and imports for decompiled code
│       └── zz_*.h           # decompiled code from c1c
├── streams                  # game assets (.NSD/.NSF files)
├── DejaVuSansMono.ttf       # font for in-game GUI
├── Makefile                 # project makefile
└── README.md                # project overview; this file
```
