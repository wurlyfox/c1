# Issues

## General

- objects are not bright enough
- camera position offset oscillates at a few points in specific levels (ex. The Lost City)
- main death sequence camera behavior is incorrect
- title sequence background images become too bright in some instances
- transparency/blending behavior is incorrect for some objects
- aku aku sparkle fragments do not appear
- it is sometimes (but rarely) possible to jump in mid-air when beside walls
- various issues with invincibility
- wumpa fruit is collected in 'groups' of 3 (and then 1 for a total of 10) instead of 1 at a time when the [top of the] player hits the bottom face of a bouncy wumpa fruit box after being launched upwards from an bounceable object
- press X icon at level win is scaled incorrectly
- hot pipe shading intensity appears to be inverted
- returning from a bonus round brings you back to the map instead of a level
- level transitions cause the game to crash when the game is booted directly into a level (i.e. level has not been entered from the map)
- demo mode does not function correctly
- attempting to enter any title state other than the map from the main menu ultimately results in the game entering an infinite loop and/or crashing
- compiling with optimization flags causes the game to behave differently (aside from performance improvements)
- tree change tracking implementation is broken

## Per-level

### Map

- camera is incorrect
- some controls do not work
- textures fail to load in some instances

### N. Sanity Beach

### Jungle Rollers

- camera is not positioned correctly at the beginning of the level

### The Great Gate

- camera does not adjust quickly enough when moving upward
- bounceable fire pit never transitions to flame state

### Boulders

### Upstream

### Rolling Stones

- falling pegs seem to be positioned slightly higher than Crash when Crash stands on them

### Papu Papu

- Papu Papu's staff does no damage to Crash

### Hog Wild

- works fine up until a certain point in the level where it is not possible to veer away from the camera path

### Native Fortress

- camera does not adjust quickly enough when moving upward

### Up The Creek

### Ripper Roo

- Ripper Roo only makes a single jump before they disappear and restart their path

### The Lost City

- game crashes when attempting to kill lizards
- oscillating camera at a few points

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

- camera is unable to keep up with player during the falling descent at the beginning of the level, thus causing Crash to fall below bottom of the screen (which the games registers as death) and making the level unplayable

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

- potion viles thrown by lab asses are not visible
