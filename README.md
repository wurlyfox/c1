# C1 #

This is a decompilation and native PC port of the game *Crash Bandicoot* in C.

The code is designed to compile for both PS1 and PC. At the moment only PC-specific code is working.

## Status ##

- **Code:** 100% ported
- **Functionality:** 95%

The game is mostly playable with only a small number of remaining [issues](doc/issues.md).

## Compiling ##

C1 uses **CMake (3.20+)** for its build system. The game engine strictly relies on 32-bit pointer layouts and arithmetic; **only 32-bit (x86 / i686 / Win32) target binaries are currently supported.**

### Linux (Ubuntu / Debian) ###

A 32-bit multilib toolchain is required on 64-bit hosts. Ensure a multilib compiler toolchain is installed:
```bash
sudo apt update
sudo apt install cmake build-essential gcc-multilib g++-multilib
```

Then simply build the project:
```bash
make
```

A wrapper `Makefile` is provided in the project root that automatically invokes CMake with the correct multiarch arguments and target configuration. By default, CMake will automatically query `dpkg` / `apt-get` during configuration to install all required packages.

### Windows (Visual Studio 2019 / 2022) ###

> C1 requires the **LLVM Clang-CL** toolchain (`clang-cl.exe`). **Building with standard MSVC (`cl.exe`) is currently unsupported.**

Ensure the **C++ Clang Compiler for Windows** component is installed via the Visual Studio Installer (under *Desktop development with C++*).

1. Open Visual Studio.
2. Select **Open a local folder** and choose the `c1` repository directory.
3. Visual Studio will automatically detect `CMakeLists.txt` and generate the project cache.
4. Select **Build All** (`Ctrl+Shift+B`).

When building with pre-compiled packages (`BUILD_DEPS=OFF`), CMake automatically downloads pre-compiled 32-bit binaries for SDL2 and FluidSynth into `deps/`. On post-build, all runtime DLLs (SDL2, FluidSynth, and its companion GLib/Intl libraries) are staged directly alongside `c1.exe` in the root project folder.

### Dependencies Summary ###
#### PC ####
- C11 and C++17 compilers (GCC, Clang, or Clang-CL; `cl.exe` unsupported)
- CMake 3.20 or newer
- OpenGL 2.0+
- SDL 2 (2.30.x)
- FluidSynth (2.3.x)
- Dear ImGui / cimgui (fetched and built automatically from source)

On Ubuntu/Debian, all dependencies can be pre-retrieved (as an optional step before running `make`):

- **64-bit:**
  ```bash
  sudo dpkg --add-architecture i386
  sudo apt update
  sudo apt install build-essential gcc-multilib g++-multilib libstdc++6:i386 libgl1-mesa-dev:i386 libsdl2-dev:i386 libfluidsynth-dev:i386
  ```
- **32-bit:**
  ```bash
  sudo apt install cmake build-essential libstdc++6 libgl1-mesa-dev libsdl2-dev libfluidsynth-dev
  ```

#### PS1 ####
*(Note: PS1 target build is currently non-functional)*
- Psy-Q SDK

### Build Options ###

The build system provides the following configuration toggles via CMake:

| Option | Default | Description |
| :--- | :--- | :--- |
| `BUILD_DEPS` | `OFF` | When `OFF`, searches locally or automatically downloads/installs pre-compiled libraries (APT multiarch on Linux, pre-compiled packages on Windows). When `ON`, fetches and compiles SDL2 and FluidSynth from source via FetchContent. |
| `BUILD_SHARED_LIBS` | `OFF` | When building dependencies from source (`BUILD_DEPS=ON`), controls whether dependencies are built as shared DLLs/SOs or static libraries. |

## Running ##

To run C1, ensure the asset files are present, then execute the binary from the root project directory:

- **Linux:** `./c1`
- **Windows:** `c1.exe`

Asset files (`.NSD` and `.NSF`) from a *Crash Bandicoot* PS1 game disc are required:
1. Locate the files in the `/S0`, `/S1`, `/S2`, and `/S3` directories on the game disc.
2. Copy all `.NSD` and `.NSF` files into the local `streams/` directory.
3. Rename all copied asset filenames from uppercase (`S00000%%.NS[F,D]`) to lowercase (`s00000%%.ns[f,d]`).

The game is hardcoded to boot into level `25` (title sequence and main menu screen(s)) by default. To select a different boot level, adjust `LID_BOOTLEVEL` in `src/common.h` and recompile.

## Controls ##

### Joystick

| Control         | Key                | Control         | Key                |
| :---            | :---               | :---            | :---               |
| **X**           | <kbd>Z</kbd>       | **L1**          | <kbd>A</kbd>       | 
| **Square**      | <kbd>X</kbd>       | **R1**          | <kbd>S</kbd>       | 
| **Circle**      | <kbd>C</kbd>       | **L2**          | <kbd>Q</kbd>       | 
| **Triangle**    | <kbd>V</kbd>       | **R2**          | <kbd>W</kbd>       |
| **D-Pad Up**    | <kbd>&#8593;</kbd> | **Start**       | <kbd>Enter</kbd>   |
| **D-Pad Down**  | <kbd>&#8595;</kbd> | **Select**      | <kbd>Space</kbd>   |
| **D-Pad Left**  | <kbd>&#8592;</kbd> |
| **D-Pad Right** | <kbd>&#8594;</kbd> |


### Other

| Control                               | Key            |
| :---                                  | :---           |
| **Toggle in-game GUI**                | <kbd>Esc</kbd> |
| **Toggle in-game GUI keyboard focus** | <kbd>Tab</kbd> |

## Structure ##
The project structure is listed below, including descriptions for each file.

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
│   ├── Windows                   # pc-specific code (non-inline)
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
├── CMakeLists.txt           # CMake build script
├── CMakeSettings.json       # Visual Studio CMake presets
├── Makefile                 # project makefile
└── README.md                # project overview; this file
```
