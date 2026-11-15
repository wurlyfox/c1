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
| `DEBUG` | `0` | `DEBUG=1` adds runtime checks and function names to error call stacks. |
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
  frame limiter in `GLUpdate` waits on `requestAnimationFrame` instead of
  spinning, so frames are presented in step with the display.
- **Display.** The page sizes the canvas to its on-screen size times the device
  pixel ratio (capped at 2) and the engine sets the GL viewport from it every
  frame. In portrait with the touch controller shown, the game sits in a 4:3
  band at the top with the controls below.
- **Audio.** The audio buffer is 2048 frames (web audio runs its callback on
  the main thread between frames), and the page resumes the audio context on
  input and when the tab becomes visible again.
- **In-game GUI.** Not built (`CFLAGS_GUI` and `CFLAGS_GOOL_DEBUG` are off).
- **Input.** Keyboard as on desktop. The page also maps gamepads and the
  on-screen touch controller to the same keys through `WebSetKey`.
- **Optimization.** The desktop build is `-O0` because optimized builds are
  known to behave differently. The web build defaults to `-O2` with
  `-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
  -ftrivial-auto-var-init=zero`.

## Touch controller

`web/js/touch.js` and `web/js/dpad-gemuboy.js` come from
[ReChan Web](https://github.com/gabom88/ReChan) (`web/shell/js`), adapted to the
PlayStation pad: d-pad or stick, the four face buttons, Start and Select, plus
L1/R1/L2/R2 hidden by default. From the ☰ menu each control can be moved,
resized or hidden, separately for landscape and portrait; the layout is saved
in `localStorage`. The overlay hides itself when a keyboard or gamepad is used
and comes back on the next touch.

The display, frame pacing and audio changes above follow the same project.
Its AAC music and H.264 movie playback were not carried over: c1 has no movies,
and its music is sequenced (SEQ/VAB) and synthesized by FluidSynth at run time.

## Status

Verified in headless Chromium, without game data: the page, importing files
from raw and plain disc images, storage across reloads, WebGL output of every
primitive type and blend mode against expected pixel values, frame pacing
(30 fps), audio/FluidSynth start-up, and, on an emulated phone, the touch
controller (every button, d-pad diagonals, multi-touch), both orientations,
canvas resizing and the layout editor.

**Not yet verified: running the actual game.** That needs the level files,
which were not available when this was written.
