#!/usr/bin/env bash
# Builds the browser (WebAssembly) version of c1 with Emscripten.
#
#   web/build.sh            # release build into build-web/dist
#   OPT=-O0 web/build.sh    # unoptimized build (see doc/web.md)
#   MIDI=0 web/build.sh     # build without FluidSynth (no music)
#   DEBUG=0 web/build.sh    # leaner build: no runtime checks or function names
#
# Requires the Emscripten SDK (emcc, emcmake) and cmake in PATH.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build-web}"
OPT="${OPT:--O2}"
MIDI="${MIDI:-1}"
# on by default while the port is young: errors then say where they happened
DEBUG="${DEBUG:-1}"
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
LINK_EH=()
if [ "$MIDI" = "1" ]; then
  SOURCES+=("$ROOT/web/stubs.c")
  FS_SRC="$DEPS/fluidsynth"
  # FluidSynth relies on C++ exceptions for ordinary failures (for example
  # the file lookup done when a soundfont is loaded from memory). Emscripten
  # turns a throw into abort() unless exceptions are enabled, both when
  # building the library and at link time. The build directory is versioned
  # so that a library built without them is not reused.
  FS_LIB="$DEPS/fluidsynth-build-eh/src/libfluidsynth.a"
  LINK_EH=(-fexceptions)
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
    emcmake cmake -S "$FS_SRC" -B "$DEPS/fluidsynth-build-eh" \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -Dosal=cpp11 \
      -Denable-floats=ON -DBUILD_UNIT_TESTS=OFF -DBUILD_DOC=OFF \
      -DCMAKE_C_FLAGS=-fexceptions -DCMAKE_CXX_FLAGS=-fexceptions \
      -DGCEM_INCLUDE_DIR="$FS_SRC/gcem/include" "${FS_OPTS[@]}"
    cmake --build "$DEPS/fluidsynth-build-eh" --target libfluidsynth -j"$JOBS"
  fi
  CFLAGS+=(-I"$FS_SRC/include" -I"$DEPS/fluidsynth-build-eh/include")
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

LINK_DEBUG=()
if [ "$DEBUG" = "1" ]; then
  LINK_DEBUG=(-sASSERTIONS=1 --profiling-funcs)
fi

echo "linking..."
em++ "$OPT" "${OBJECTS[@]}" "${LIBS[@]}" "${LINK_EH[@]}" "${LINK_DEBUG[@]}" -o "$DIST/c1.js" \
  -sUSE_SDL=2 \
  -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 \
  -sSTACK_SIZE=8MB -sINITIAL_MEMORY=256MB -sALLOW_MEMORY_GROWTH \
  -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2 \
  -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM -lidbfs.js \
  -sEXPORTED_FUNCTIONS=_main,_WebSetKey,_WebDebugInfo,_CardControl \
  -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,callMain,HEAP8,HEAP32

# Stamp the build so the page can show it in error reports and so browsers
# never mix files cached from an older build with new ones.
BUILD_ID="$(date -u +%Y%m%d-%H%M%S)"
COMMIT="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if [ -n "$(git -C "$ROOT" status --porcelain 2>/dev/null)" ]; then COMMIT="$COMMIT+local"; fi
FLAGS="$OPT MIDI=$MIDI ${LINK_DEBUG[*]:-}"
printf 'window.C1_BUILD = { id: "%s", commit: "%s", flags: "%s" };\n' "$BUILD_ID" "$COMMIT" "$FLAGS" > "$DIST/version.js"

cp "$ROOT/web/c1web.js" "$ROOT/web/c1web.css" "$DIST/"
sed "s/__C1_BUILD__/$BUILD_ID/g" "$ROOT/web/index.html" > "$DIST/index.html"
mkdir -p "$DIST/js"
cp "$ROOT/web/js/"*.js "$DIST/js/"

# The disc image -> game ZIP converter, as a download for people who use the
# page without the repository.
if command -v python3 >/dev/null 2>&1; then
  python3 "$ROOT/tools/pack/build_tool_zip.py" "$DIST/c1-pack-tool.zip"
else
  echo "python3 not found: skipping c1-pack-tool.zip (the converter download)"
fi
echo "build $BUILD_ID ($COMMIT)"
echo "done: $DIST (serve it over http, e.g. python3 -m http.server -d \"$DIST\")"
