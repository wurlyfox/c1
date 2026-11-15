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
| `DEBUG` | `1` | Runtime checks and function names in error call stacks. `DEBUG=0` builds without them. |
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
- **Presentation.** Frames are drawn into an offscreen target and copied to
  the canvas when complete. The engine paints the background, waits for the
  frame time and only then draws the primitives; a browser shows the canvas
  at every yield and then discards it, so drawing directly flickered between
  the background alone and the primitives on black.
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
- **Input.** The page reads the keyboard, gamepads and the on-screen touch
  controller and presses the engine's pad keys through `WebSetKey`. Keyboard
  keys can be reassigned on the start page (saved in `localStorage`); SDL's
  own keyboard handling is bypassed so the built-in layout does not interfere.
- **Optimization.** The desktop build is `-O0` because optimized builds are
  known to behave differently. The web build defaults to `-O2` with
  `-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
  -ftrivial-auto-var-init=zero`.

## Debug panel

Tick "Show the debug panel" on the start page, press F2 while playing, or use
the ☰ menu. The panel sits beside the game (over the picture on narrow
screens) and shows:

- live state, read four times a second through `WebDebugInfo` in
  `src/pc/gfx/gl.c`: frame rate, work time per frame, level id, triangles
  drawn, lives / fruit / boxes, the player's position, held pad buttons,
  canvas size, memory and audio state;
- a log of what the engine prints, what the page does (resizes, audio,
  gamepads, tab visibility) and state changes it notices (level changes,
  pause, slow frames, WebGL context loss). "Copy" puts state and log on the
  clipboard.

The Debug section of the start page can also boot straight into any stored
level instead of the title sequence ("Start at", or `?level=<hex id>` in the
address), and save a stored level file to disk.

The log is recorded from the moment Play is pressed, whether or not the panel
is open. When the game stops, a full error report opens by itself.

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
