# Changelog

## 2022-12-28

### Fixed

- object shading inaccuracy
- press X icon at level win is scaled incorrectly
- hot pipe shading intensity appears to be inverted
- sound dies after collecting 3 aku aku and any level transitions thereafter cause the game to crash 
- map controls do not work
- (partial) attempting to enter any title state other than the map from the main menu ultimately results in the game entering an infinite loop and/or crashing
- issue with singular/plural text
- Papu Papu: staff does no damage to Crash
- Hog Wild: works fine up until a certain point in the level where it is not possible to veer away from the camera path

## 2022-12-27

### Fixed

- aku aku sparkle fragments do not appear
- aku aku mask does not appear/invincibility is not completely effective
- wumpa fruit is collected in 'groups' of 3 (and then 1 for a total of 10) instead of 1 at a time when the [top of the] player hits the bottom face of a bouncy wumpa fruit box after being launched upwards from an bounceable object
- returning from a bonus round brings you back to the map instead of a level
- Jungle Rollers: camera is not positioned correctly at the beginning of the level
- The Great Gate: camera does not adjust quickly enough when moving upward
- The Great Gate: bounceable fire pit never transitions to flame state

## 2022-12-26

### Fixed

- broken sfx code
- broken music/midi player code
- myriad of issues related to audio
- no initialization of gool globals at game init
- transparency/blending behavior is incorrect for some objects

### Added

- custom audio sampler and mixer implementation

### Removed

- SDL mixer dependency

### Other

- Sound effects and music are now working. Volume levels may not be completely accurate to the original game, however. Expect a fix for this in future updates.

## 2022-12-19

### Fixed

- flickering colors during intro sequence
- rot matrix calc errors
- some text does not render
- issues preventing level transitions
- more ripple shader inaccuracies
- fog shader inaccuracies
- lightning shader inaccuracies
- dark shader inaccuracies
- background color not visible in levels that have one
- animated textures do not animate
- some objects randomly respawn
- some object collisions do not register (namely collisions with wumpa fruit)
- some objects appear to not complete their paths
- pausing the game does not work correctly
- beach does not display in N. Sanity Beach
- boot level is N. Sanity Beach instead of title

### Added

- top and bottom screen borders present in original
- toggleable render of object bounds (press <key>q</key> to toggle)
- issue tracking document

### Removed

- patches code (no longer used)

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
