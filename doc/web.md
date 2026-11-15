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
- the `.NSF`/`.NSD` files themselves, or
- a **game ZIP** made with the converter described below.

The files are kept in the browser's IndexedDB storage (mounted at `/streams`),
so they only have to be chosen once. Nothing is uploaded.

### Game ZIP

`tools/pack/make_c1_pack.py` reads the disc image and writes `c1-pack.zip`
with only what the game needs: the level files and a small `manifest.json`.
It is roughly a third of the size of the image, which matters when the game
is loaded on a phone.

```bash
python3 tools/pack/make_c1_pack.py "Crash Bandicoot (USA).bin"
```

Without arguments it looks for the image next to the script and in Downloads,
Desktop and Documents. `-o` chooses the output file, `--store` skips
compression, `--force` accepts a disc that is not SCUS-94900. Only the Python
standard library is used. `make_c1_pack.command` (macOS) and
`make_c1_pack.bat` (Windows) run it with a double click.

The web build publishes the converter as `c1-pack-tool.zip`, linked from the
start page, for people who use the page without the repository. The page loads
a pack with "Choose game ZIP" (`web/js/zippack.js`); a hand-made ZIP of the
level files works too.

The pack contains game data. It is for personal use and must not be shared or
committed.

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
- **Picture modes** (☰ menu). *Fill window (wider view)*, the default: the
  canvas takes the whole window and `GLUpdateScreen` in `gl.c` widens the
  visible area from 512 screen units to match, so more of the scene shows at
  the sides and nothing is stretched. The game only draws what the original
  4:3 camera could see, so inside levels the extra area is partly empty and
  ends in ragged edges; 2D screens (menus, title cards) stay 4:3 in the
  middle. *Fill window (stretched)* stretches the 4:3 picture instead.
  *Original 4:3* keeps the picture as it was. All three follow the window
  size as it changes.
- **Resolution and antialiasing** (☰ menu). The frame is drawn into the
  offscreen target at an internal resolution: the window's (default), 1.5 or
  2 times that, or the 240 rows of the original console. Antialiasing (on by
  default) draws at twice the internal resolution in each direction and
  averages it down when the frame is copied to the canvas, which smooths
  polygon edges. The target is capped at 4096 pixels a side. Both cost GPU
  time; the debug panel shows the target size as "Render".
- **Menu link.** The last entry of the ☰ menu opens a web page in a new tab.
  Its label and address are `MENU_LINK` at the top of `web/c1web.js`; an
  empty address hides it.
- **Display.** The page sizes the canvas to its on-screen size times the device
  pixel ratio (capped at 2) and the engine sets the GL viewport from it every
  frame. In portrait with the touch controller shown, the game sits in a 4:3
  band at the top with the controls below.
- **Volume.** The ☰ menu has a master volume slider (0 to 500%; the game's mix is quiet), applied to
  the final mix by `WebSetVolume` in `src/pc/sound/audio.c` and remembered
  between visits. It is separate from the game's own effects and music
  volumes in Options.
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

## Saved games

The PC port had no memory card: `CardControl` was a stub, so the Load Game and
save screens could not work. `src/pc/card.c` now provides a virtual card that
is always inserted and formatted. It reproduces the states, flags and part
information that the game's own save screens (the `CardC` GOOL code) expect
from `src/psx/card.c`, and keeps up to 15 saves, each holding the same block
of globals the original writes, in one small file.

On the web that file is `/save/card.dat`, stored in IndexedDB. The start page
shows how many saves there are and can download a backup, restore one or
delete them. The desktop build writes `c1card.dat` in the working directory.

Used slots show the game's own save icon (Crash's face, animated), copied
into the slot's texture region as the original does. Slot names are not
shown: the text buffer for them is never set up in this port. Slots still
show their progress.

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
