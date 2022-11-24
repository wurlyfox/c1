# Changelog

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
