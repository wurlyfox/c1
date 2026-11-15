#!/usr/bin/env bash
# Builds the browser (WebAssembly) version of c1 with Emscripten.
#
#   web/build.sh            # release build into build-web/dist
#   OPT=-O0 web/build.sh    # unoptimized build (see doc/web.md)
#   MIDI=0 web/build.sh     # build without FluidSynth (no music)
#
# Requires the Emscripten SDK (emcc, emcmake) and cmake in PATH.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build-web}"
OPT="${OPT:--O2}"
MIDI="${MIDI:-1}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
FLUIDSYNTH_TAG="v2.5.0"
GCEM_REV="012ae73c6d0a2cb09ffe86475f5c6fba3926e200"

command -v emcc >/dev/null || { echo "emcc not found: install and activate the Emscripten SDK first" >&2; exit 1; }

OBJ="$OUT/obj"
DEPS="$OUT/deps"
DIST="$OUT/dist"
mkdir -p "$OBJ" "$DEPS" "$DIST"

# The game code relies on behavior that optimizing compilers are allowed to
# break (see doc/issues.md); these flags define the usual suspects.
CFLAGS=(
  "$OPT" -std=gnu11 -fms-extensions
  -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
  -ftrivial-auto-var-init=zero
  -w -Wno-error=incompatible-pointer-types -Wno-error=int-conversion
  -sUSE_SDL=2
  -I"$ROOT/src"
  -DCFLAGS_GFX_SW_PERSP -DCFLAGS_DRAW_EXTENSIONS
)

SOURCES=()
while IFS= read -r f; do SOURCES+=("$f"); done < <(
  find "$ROOT/src" -maxdepth 1 -name '*.c'
  find "$ROOT/src/util" "$ROOT/src/pc" -name '*.c'
)

LIBS=()
if [ "$MIDI" = "1" ]; then
  SOURCES+=("$ROOT/web/stubs.c")
  FS_SRC="$DEPS/fluidsynth"
  FS_LIB="$DEPS/fluidsynth-build/src/libfluidsynth.a"
  if [ ! -f "$FS_LIB" ]; then
    [ -d "$FS_SRC" ] || git clone --depth 1 -b "$FLUIDSYNTH_TAG" https://github.com/FluidSynth/fluidsynth "$FS_SRC"
    if [ ! -f "$FS_SRC/gcem/include/gcem.hpp" ]; then
      rm -rf "$FS_SRC/gcem"
      git clone https://github.com/kthohr/gcem "$FS_SRC/gcem"
      git -C "$FS_SRC/gcem" checkout -q "$GCEM_REV"
    fi
    FS_OPTS=()
    for o in alsa pulseaudio pipewire jack sndfile libsndfile dbus network readline \
             openmp midishare coreaudio wasapi dsound winmidi waveout oss sdl2 sdl3 \
             libinstpatch ladspa systemd opensles oboe portaudio aufile ipv6 lash; do
      FS_OPTS+=("-Denable-$o=OFF")
    done
    emcmake cmake -S "$FS_SRC" -B "$DEPS/fluidsynth-build" \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -Dosal=cpp11 \
      -Denable-floats=ON -DBUILD_UNIT_TESTS=OFF -DBUILD_DOC=OFF \
      -DGCEM_INCLUDE_DIR="$FS_SRC/gcem/include" "${FS_OPTS[@]}"
    cmake --build "$DEPS/fluidsynth-build" --target libfluidsynth -j"$JOBS"
  fi
  CFLAGS+=(-I"$FS_SRC/include" -I"$DEPS/fluidsynth-build/include")
  LIBS+=("$FS_LIB")
else
  CFLAGS+=(-DC1_NO_MIDI)
fi

echo "compiling ${#SOURCES[@]} files ($OPT)..."
OBJECTS=()
pids=()
for src in "${SOURCES[@]}"; do
  rel="${src#"$ROOT/"}"
  obj="$OBJ/${rel//\//_}.o"
  OBJECTS+=("$obj")
  emcc -c "${CFLAGS[@]}" "$src" -o "$obj" &
  pids+=($!)
  if [ "${#pids[@]}" -ge "$JOBS" ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
done
for pid in "${pids[@]}"; do wait "$pid"; done

echo "linking..."
em++ "$OPT" "${OBJECTS[@]}" "${LIBS[@]}" -o "$DIST/c1.js" \
  -sUSE_SDL=2 \
  -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 \
  -sSTACK_SIZE=8MB -sINITIAL_MEMORY=256MB -sALLOW_MEMORY_GROWTH \
  -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2 \
  -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM -lidbfs.js \
  -sEXPORTED_FUNCTIONS=_main,_WebSetKey \
  -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,callMain

cp "$ROOT/web/index.html" "$ROOT/web/c1web.js" "$ROOT/web/c1web.css" "$DIST/"
mkdir -p "$DIST/js"
cp "$ROOT/web/js/"*.js "$DIST/js/"
echo "done: $DIST (serve it over http, e.g. python3 -m http.server -d \"$DIST\")"
