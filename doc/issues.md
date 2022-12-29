# Issues

## General

- colors are slightly different from psx (map differently?)
- a few sfx are very loud and distorted (ex. warp sound, also when entering a bonus round)
- in some instances, game transitions occur too early (ex. game immediately jumps to win level screen when player lands on a warp platform, without playing either of crash's 'win level' animation sequences; some death sequences are cut short)
- main death sequence camera behavior is incorrect
- title sequence background images become too bright in some instances
- it is sometimes (but rarely) possible to jump in mid-air when beside walls
- demo mode does not function correctly
- attempting to enter any title state other than the map from the main menu ultimately results in the game entering an infinite loop and/or crashing
- compiling with optimization flags causes the game to behave differently (aside from performance improvements)
- tree change tracking implementation is broken

## Per-level

### Map

- camera is incorrect
- textures fail to load in some instances

### N. Sanity Beach

### Jungle Rollers

### The Great Gate

### Boulders

### Upstream

### Rolling Stones

- falling pegs seem to be positioned slightly higher than Crash when Crash stands on them

### Papu Papu

### Hog Wild

- player dies after respawning at a checkpoint
- crash skips the 'look back at camera and then jump on hog' animation sequence that typically begins hog levels

### Native Fortress

### Up The Creek

### Ripper Roo

- Ripper Roo only makes a single jump before they disappear and restart their path

### The Lost City

- game crashes when attempting to kill lizards

### Temple Ruins

- approaching bats causes game to crash

### Road To Nowhere

- objects do not detect solid floor; any wumpa fruit/lives released from boxes seem to fall past the floor without being stopped

### Boulder Dash

### Whole Hog

### Sunset Vista

- same crashes as The Lost City, when attempting to kill lizards (note: RWaOC; corrupted animation eid?)

### Koala Kong

- TNT boxes fall through the floor
- boulders launched by Koala Kong do not appear or are not thrown in the destined direction

### Heavy Machinery

### Cortex Power

- going near a MafiC causes the game to crash

### Generator Room

### Toxic Waste

- solid ceiling is not detected
- game crashes when dying in certain zones

### Pinstripe

- gun bullets do not do any damage to Crash

### The High Road

### Slippery Climb

### Lights Out

- lights go out too abruptly when time is up

### Fumbling in the Dark

### Jaws of Darkness

- spider or shadow rotation angle is incorrect
- rotating pegs rotate way too fast

### Castle Machinery

### N. Brio

- N. Brio never transitions from his initial state

### The Lab

- game crashes when dying in certain zones

### The Great Hall

### N. Cortex

- some particles shot by Cortex are not visible?
- only some of the particles shot by Cortex damage Crash

### Stormy Ascent

- potion vials thrown by lab asses are not visible
