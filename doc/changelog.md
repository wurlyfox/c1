# Changelog

## 2022-12-13

### Fixed

- (most) transparency has not been implemented
- (most) semi-transparency/blending behavior is inaccurate
- incorrect starting location for pickups when moving to hud

## 2022-12-12

### Fixed

- textures should not use anti-aliasing
- (partial) object phong shading is inaccurate
- (partial) transparency has not been implemented
- screen does not fade out/in after death/before respawn
- player does not inherit zone brightness levels
- (some) gool issues
- timing issue with gool animations
- no deceleration of player when jumping
- (partial) it is sometimes possible to jump in mid-air when beside walls
- auto-cam transition takes multiple button presses to skip
- (many) solidity check/collision issues
- ripple shader inaccuracies
- text is not rendered at the correct locations

### Removed

- vertex buffer based draw code
- gl extensions dependency

### Added

- immediate mode based draw code
- toggleable render of wall bitmap image (press <key>d</key> to toggle)

## 2022-12-7

### Fixed

- path traversing objects are not oriented correctly
- display list ordering of objects is incorrect
- object geometry outside of viewing frustum is not rejected as it should be

## 2022-11-24

### Fixed

- (some) solidity check/collision issues
- wumpa fruit and other icon-based sprites are at incorrect scale
- (partial) HUD fails to render
- camera lags behind player
- camera path selection at divergences is incorrect
- infinite loop in `GoolObjectSearchTree`
- segfault on exit

### Added

- changelog (this file)
- in-game GUI re-write using [c]imgui as backend (press <key>esc</key> to toggle)
  - GOOL object code debugger
- toggleable render of zone octree query results (press <key>a</key> to toggle)

### Removed

- old in-game GUI implementation
- freetype dependency

### Other

Audio SFX are still disabled by default as they are not working 100% correctly, however they can now be enabled with compilation flag `-DCFLAGS_SFX_FLUIDSYNTH`.
