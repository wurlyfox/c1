# Web build

`web/` contains a browser (WebAssembly) build of the PC port, made with
[Emscripten](https://emscripten.org). It reuses the PC code in `src/pc`; the
web-specific parts are guarded by `__EMSCRIPTEN__`.

## Building

Requirements: the Emscripten SDK (`emcc` in `PATH`), CMake and git.

```bash
web/build.sh
python3 -m http.server -d build-web/dist
```

Then open `http://localhost:8000`. The page has to be served over HTTP; opening
`index.html` from disk does not work.

| Variable | Default | Description |
| :--- | :--- | :--- |
| `OPT` | `-O2` | Optimization level. Use `OPT=-O0` if the game misbehaves (see below). |
| `MIDI` | `1` | `MIDI=0` builds without FluidSynth: no sequenced music, faster build. |
| `OUT` | `build-web` | Output directory. The site is written to `$OUT/dist`. |

SDL2 comes from the Emscripten ports; FluidSynth is cloned and built into
`build-web/deps` on the first run.

## Game files

No game data is included. The page asks for either

- a disc image of the NTSC-U release (`.bin` with 2352-byte sectors, or a plain
  `.iso`); the `.NSF`/`.NSD` files are extracted from it in the browser, or
- the `.NSF`/`.NSD` files themselves.

The files are kept in the browser's IndexedDB storage (mounted at `/streams`),
so they only have to be chosen once. Nothing is uploaded.

## What differs from the desktop build

- **Renderer.** WebGL has no fixed-function pipeline, so `src/pc/gfx/gl.c` has a
  second path (`C1_GLES2`) that batches the primitives and draws them with one
  small shader. Blending and the 2x texture color scale match the desktop path.
  Title card images are drawn at 1x; on desktop they inherit the 2x scale left
  behind by the primitives, which is the likely cause of the "too bright"
  entry in [issues.md](issues.md).
- **Main loop.** The game loop is unchanged. The link uses Asyncify, and the
  frame limiter in `GLUpdate` sleeps instead of spinning.
- **In-game GUI.** Not built (`CFLAGS_GUI` and `CFLAGS_GOOL_DEBUG` are off).
- **Input.** Keyboard as on desktop. The page also maps gamepads and on-screen
  touch buttons to the same keys through `WebSetKey`.
- **Optimization.** The desktop build is `-O0` because optimized builds are
  known to behave differently. The web build defaults to `-O2` with
  `-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
  -ftrivial-auto-var-init=zero`.

## Status

Verified in headless Chromium, without game data: the page, importing files
from raw and plain disc images, storage across reloads, WebGL output of every
primitive type and blend mode against expected pixel values, frame pacing
(30 fps), and audio/FluidSynth start-up.

**Not yet verified: running the actual game.** That needs the level files,
which were not available when this was written.
