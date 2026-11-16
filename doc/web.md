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

No game data is included. The start page offers up to three ways to load
the game:

1. **Download the game**, when the build was given a split pack (see
   below). The parts are fetched one by one, checked and kept in the browser.
2. **Use your disc image**: the `.bin` (2352-byte sectors) or `.iso` of the
   NTSC-U release; the level files are extracted from it in the browser.
3. **Use a game ZIP** made with the converter described below.

Loose `.NSF`/`.NSD` files can still be added from the Advanced section, which
also holds the other technical options (debug panel, start level, saving a
stored level file).

The files are kept in the browser's IndexedDB storage (mounted at `/streams`),
so the game only has to be loaded once. Nothing is uploaded.

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

### Download in parts

For local use, the page can install the game by itself from parts placed
next to it, so that a phone on the same network does not need the disc image
or the ZIP:

```bash
python3 tools/pack/make_c1_pack.py "Crash Bandicoot (USA).bin"
python3 tools/pack/split_pack.py c1-pack.zip
web/build.sh
```

`split_pack.py` (adapted from ReChan Web) cuts the pack into 19 MB parts and
writes them with a `pack.json` (sizes and SHA-256 of each part) into
`web/data/`; the build copies that folder into `dist/data/`. With it present
the start page shows "Download the game". `web/js/netpack.js` downloads the
parts, verifies each one, and keeps them in their own IndexedDB database
(`c1-download`) so an interrupted download resumes with the parts it already
has; once the game is installed the parts are deleted.

`web/data/` is ignored by git. The parts are the game: do not commit or
publish them.

### Robust installs

Installing a large game in a browser fails in ways that are easy to make
permanent. The page guards against them:

- **Parts kept from an interrupted download are read back and hashed again**
  before they are reused; a part written as the tab closed can be truncated.
  A part that does not match is downloaded again. Each part is tried three
  times, with a growing wait, and its size and SHA-256 are checked before it
  is stored.
- **SHA-256 works without `crypto.subtle`** (`web/js/sha256.js`). Browsers
  only offer it on https and localhost, so a phone opening the page by the
  computer's address on the local network would otherwise be unable to verify
  anything.
- **Errors say what happened**: the real error name and text, with the part
  or phase ("c1-pack.part003: TypeError – Failed to fetch", "IndexedDB (saving
  the game): QuotaExceededError"). Only running out of space is reworded.
- **Free space is checked before downloading** (parts plus the unpacked game,
  about 2.6 times the pack) and the browser is asked to keep the data
  (`navigator.storage.persist`).
- **An install manifest** (`/streams/install.json`) is written last and only
  on success: the number of files really stored, `complete`, an
  `importVersion`, the source and the pack's SHA-256. A full install removes
  the previous manifest before its first write, and a failed one deletes what
  it wrote, so nothing half installed is ever taken for a game.
- **The stored game is validated when the page starts**: manifest present and
  complete, at least as many files as it lists, the title level there, and
  the start of the title `.NSF` and of the first and last `.NSF` checked for
  the page magic (`34 12` or `35 12`). Otherwise it is deleted and the page
  says why. Installs older than the manifest are accepted when every file has
  its partner and every `.NSF` starts correctly.
- **The error panel has Retry** (an interrupted download carries on from the
  parts it has) **and "Delete everything and download again"**, which is also
  in Advanced. The latter empties the game store, deletes the game and
  download databases even if they are broken, clears every cache, and reloads
  with a mark that starts the download by itself. Saved games (another
  database) and options (`localStorage`) are kept.
- **The download stays on offer with a game installed**, as an update, unless
  the installed game came from that very pack.
- **The ZIP is read through a 4 MB window** in file order, so headers and
  small entries do not cost a read each, and every entry is copied into its
  own bytes: nothing stored is a slice of the pack. Game files are stored as
  bytes by Emscripten's IDBFS, not as Blobs, so WebKit's one-file-per-Blob
  cost and its bug with slices of disk-backed Blobs do not come into play.

The service worker (see App below) never caches the data parts.

### App (PWA)

The page can be installed as an app (home screen icon, full screen, works
offline once opened):

- `web/manifest.webmanifest`: name, colors and icons.
- `web/sw.js`, the service worker. `web/build.sh` writes the build id and the
  list of files of the build into it. It stores every file of the build with
  `cache: 'reload'` (no copy from the HTTP cache can mix builds) in a cache
  named after the build, serves the page and its files from there, and never
  caches the game data parts in `data/`, which go to the page's own storage.
  A new build installs in the background and waits: the page shows "A new
  version is ready" and switches only when the player presses Update
  (`SKIP_WAITING`). While developing, a hard reload (Shift + reload) bypasses
  it.
- Browsers only run service workers on https and on localhost. A phone that
  opens the computer's local address (`http://192.168.x.x:8000`) gets the page
  without one: it still works, online, and the home screen entry is a
  shortcut; the start page says so.
- The bottom of the start page explains how to install it on iPhone/iPad
  (Safari, Share, Add to Home Screen) and Android (Chrome, menu, Install
  app). Chrome and Edge also get an "Install the app" button from their own
  install prompt.

**Icon.** The app icon is the game's memory card icon (Crash's face), taken
from the title level of your disc by `tools/pack/make_icons.py`, which
`make_c1_pack.py` also runs when it makes a pack. It is written to
`web/img/icons/`, which git ignores: it is game art and stays on your
machine. Without it the build uses a plain "C1" placeholder.

```bash
python3 tools/pack/make_icons.py "Crash Bandicoot (USA).bin"
```

### Error reports with source lines

Debug builds (`DEBUG=1`, the default) are compiled with line tables and come
with a source map, `c1.wasm.map`. When the game stops, the report resolves
each frame of the call stack to a file and line ("TransSmoothStopAtSolid ->
src/solid.c:340"), including code the compiler inlined into another
function. Line tables do not change the generated code. The debug panel also
shows the most objects with a collision bound in one frame and the most
zone query results, against their limits.

### Hidden diagnostics

Five taps on the build line at the bottom of the start page, or `?diag` in
the address, open a report meant for phones, where there is no console: what
the browser supports (secure context, SHA-256 method, `DecompressionStream`,
storage used and whether it is persisted), the pack next to the page and the
parts kept in the browser (with a button that re-hashes them), the install
manifest, file counts, unpaired or wrongly starting files, the first 16 bytes
of key files, saved games, sound (audio support, an offline render with its
peak amplitude to detect silence, and two buttons that play a beep through
Web Audio and through `<audio>`), graphics, the last install error and the
page's recent events. "Copy" puts it on the clipboard.

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
  offscreen target at an internal resolution: 1x (the 240 rows of the
  original console), 2x, 3x or 4x, or the window's size (Auto, the default).
  1x with antialiasing off gives the console's hard pixels.
  Antialiasing (on by default) draws at twice the internal resolution in
  each direction and averages it down when the frame is copied to the
  canvas, which smooths polygon edges. The target is capped at 8192 pixels
  a side (or the GPU's limit). Both cost GPU time and memory; the debug
  panel shows the target size as "Render".
- **Title menu credit.** While the title menu is on screen the page draws a
  credit line over the bottom right corner of the picture (`CREDIT` in
  `web/c1web.js`). The engine reports the title state through `WebDebugInfo`.
- **Title menu link.** The game's title menu gets a fifth entry below
  OPTIONS ("MORE GAMES") that opens a web page in a new tab. Its text and address are
  `MENU_LINK` at the top of `web/c1web.js`; an empty address removes it.
  `web/menulink.c` enables the hidden fifth entry the menu already has
  ("TEST SAVE SYSTEM"), replaces its text in memory and takes over its
  action. No game file is changed.
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
