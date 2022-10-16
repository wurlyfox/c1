# Function descriptions #

These are here for now but will likely moved into code as function comments in the future.


## cam.c ##
- `CamAdjustProgress` - update current progress according to the given progress speed and camera struct and update the level
- `CamGetProgress` - approximate the projected progress of the player (or a point positioned behind the player) along a particular camera path (incl possible new path, distance error of player to nearest path point, etc.), given player trans and camera path
- `CamGetProgress2` - alternate version of `CamGetProgress`
- `CamFollow` - adjust current progress according to a delta calculated based on the projected progress of the player (or more correctly, a point slightly behind or left/right of the player which dynamically zooms and/or pans) along the nearest camera path and update the level
- `CamUpdate` - update progress and level according to current path's camera mode (where mode 5 and 6 use `CamFollow`)
- `CamDeath` - update camera during the spinning death sequence

## gfx.c ##
- `TgeoOnLoad` - probe for tpages referenced by texinfos in a tgeo entry upon load
- `GfxScreenProj` - return the corresponding H value for a given fov (field of view)
- `GfxInitMatrices` - initialize all global matrices used for transformations
- `GfxUpdateMatrices` - recompute global matrices used for transformations and load world models
- `GfxResetCam` - reset camera trans, rot, and scale vectors to default values
- `GfxCalcObjectZoneMatrices` - recalculate color and light matrices for player in zones with an `unknown_a` mode value
- `GfxCalcObjectMatrices` - [re]calculate matrices for an object and replace the current modelview matrix
- `GfxTransformSvtx` - transform an svtx frame and place resulting primitives into the ot
- `GfxTransformCvtx` - transform a cvtx frame and place resulting primitives into the ot
- `GfxTransformFragment` - transform a `gool_frag` and place resulting primitives into the ot
- `GfxTransformFontChar` - transform a `gool_glyph` and place resulting primitives into the ot
- `GfxTransform` - multiply a 4:12 fractional fixed point matrix by a 20:12 fixed point vector to produce a 20:12 fixed point vector
- `GfxLoadWorlds` - load info for the current zone's worlds into the reserved area of the zone header
- `GfxTransformWorlds` - transform wgeo models for the current zone's worlds and place resulting primitives into the ot (wraps call to the actual hardware or software world transform function)
- `GfxGetFar` - compute far and shamt values based on values in the current zone header; used as shader params
- `GfxTransformWorldsFog` - `GfxTransformWorlds` variant with fog shader
- `GfxTransformWorldsRipple` - `GfxTransformWorlds` variant with ripple [vertex] shader
- `GfxTransformWorldsLightning` - `GfxTransformWorlds` variant with lightning shader
- `GfxTransformWorldsDark` - `GfxTransformWorlds` variant with darkness shader
- `GfxTransformWorldsDark2` - `GfxTransformWorlds` variant with alternate darkness shader
- `sub_8001A460` - used at world map: freezes animated textures (patching over wgeo polygon texture animation info) based on bitfields

## gool.c ##
- `GoolInitAllocTable` - allocate memory for player object and all other objects, establish linked list of free objects, and initialize object handles
- `GoolKillAllocTable` - free memory for player object and all other objects
- `GoolInitLid` - copy current lid from nsd into the read-only current lid gool global
- `GoolOrientObjectOnPath` - orient an object on its path based on current path progress and current location
- `GoolInitLevelSpawns` - initialize level spawns for the current level
- `GoolObjectTraverseTreePreorder` - run a callback for each object in the object tree rooted at the given object, in pre order
- `GoolObjectTraverseTreePostorder` - run a callback for each object in the object tree rooted at the given object, in post order
- `GoolObjectSearchTree` - find the first object in the object tree rooted at the given object that returns true for the given callback, checking objects in pre order
- `GoolObjectHandleTraverseTreePreorder` - run a callback for each object in the object tree rooted at the given object handle, in pre order
- `GoolObjectHandleTraverseTreePostorder` - run a callback for each object in the object tree rooted at the given object handle, in post order
- `GoolForEachObjectHandle` - run a callback for each object handle
- `GoolObjectHasPidFlags` - callback; returns the given object if it has the given pid flags
- `GoolFindNearestObject` - query for the object nearest to the given object
- `GoolObjectTestStateFlags` - callback; returns the given object if it has the given state flags
- `GoolObjectAllocate` - allocate an object from the free object pool
- `GoolObjectSpawn` - spawn a new object for the given entity (i.e. zone and entity index)
- `GoolObjectCreate` - create a new object with the given parent, executable id, subtype, and arguments
- `GoolObjectInit` - initialize a newly allocated object
- `GoolObjectKill` - send TERM event (if given arg `sig` is 1), kill all child objects of, free audio voices for, clear spawn bits for, and return to the free list, a given object
- `GoolZoneObjectTerminate` - send TERM event and kill the given object if it is in the given zone
- `GoolObjectTerminate` - `GoolObjectKill` with `sig=1`
- `GoolZoneObjectsTerminate` - terminate all objects in the given zone
- `GoolObjectLocalBound` - recompute the local bound box for given object with given scale
- `GoolUpdateObjects` - update all objects
- `GoolObjectChangeState` - change an object's state
- `GoolObjectPushFrame` - push a frame to an object's stack
- `GoolObjectPopFrame` - pop the topmost frame from an object's stack
- `GoolObjectUpdate` - update an object; recalculate bound, run trans block, run code block if wait time has elapsed, compute object colors, do object physics, and transform object animation frame model
- `GoolObjectTransform` - transform an object's current animation frame according to animation type, and put result primitives in the ot
- `GoolTextStringTransform` - generate primitives for and transform an object's current text string (when the animation type is text), and put result primitives in the ot
- `GoolTextObjectTransform` - wrapper for `GoolTextStringTransform` which includes formatting the current string and calculating matrices for the object before transforming its formatted text string
- `GoolObjectColorByZone` - make an object inherit colors from the current zone
- `GoolObjectColors` - color an object according to invincibility state
- `GoolObjectBound` - compute an object's absolute bounding volume
- `GoolObjectControlDir` - move, accelerate/decelerate, and orient the player object according to state of directional buttons and current move state
- `GoolObjectPhysics` - handle physics for an object; includes controlling direction and rotation, stopping at solid matter, orienting on path, and falling due to gravity
- `GoolTranslateGop` - convert a GOOL operand to an absolute pointer to data
- `GoolTranslateInGop` - convert an input GOOL operand to an absolute pointer to data
- `GoolTranslateOutGop` - convert an output GOOL operand to an absolute pointer to data
- `GoolTestControls` - logic for the 'test controls' GOOL instruction, for reading joypad input
- `GoolObjectInterpret` - run the next burst of instructions for a given object
- `GoolSendEvent` - send an event from sender object to recipient object
- `GoolObjectRotate` - seek from source angle to destination angle at a given speed
- `GoolObjectRotate2` - alternative variant of `GoolObjectRotate`
- `GoolAngDiff` - gets the difference between 2 angles; result angle in the range of -180 to 180 degrees
- `GoolSeek` - seek source value towards destination value at a given delta
- `GoolTransform` - transform a vector with corresponding model matrix for the given trans, rot, and scale, for rot a ZXY euler angle
- `GoolTransform2` - transform a vector with the corresponding view matrix for the camera
- `GoolProject` - transform a vector with the corresponding view matrix for the camera and project to 2d screen coordinates
- `GoolCollide` - test for collision of source and target object bounds and set the appropriate status flags in either object when there is a collision
- `GoolSendIfColliding` - callback to use for testing and handling collision for all objects in a given object tree; implements several test types and each is handled by sending an event from sender to recipient
- `GoolSendToColliders` - tests and handles collision for all objects in a given tree (using `GoolSendIfColliding`)
- `GoolSendToColliders2` - alternative variant of `GoolSendToColliders`
- `GoolObjectInterrupt` - interrupt an object


## level.c ##
- `LdatInit` - initialize the level; set current zone and path to initial zone and path as specified in the current nsd
- `ZdatOnLoad` - onload callback for zdat entries
- `MdatOnLoad` - onload callback for mdat entries
- `LevelSpawnObjects` - spawn all unspawned objects in the current and neighbor zones (with an entity group of 3)
- `ZoneTerminateDifference` - terminate all objects spawned from neighbor zones of the current zone that do not exist as neighbors of the given next zone
- `LevelUpdate` - update current zone, path, progress, poly id list, and camera
- `LevelUpdateMisc` - perform various miscellaneous updates for level
- `LevelInitGlobals` - initialize level-engine-specific GOOL globals
- `LevelResetGlobals` - reset level-engine-specific GOOL globals
- `LevelInitMisc` - create initial objects, specific to level
- `LevelSaveState` - save current level state; used for checkpoints
- `LevelRestart` - restart level after dying or return from bonus round
- `TestPointInRect` - test if a point/vector is within a rect
- `TestPointInBound` - test if a point/vector is within a bound
- `TestRectIntersectsBound`- test if a rect intersects a bound
- `TestBoundIntersection` - test if a pair of bounds intersect
- `TestBoundInBound` - test if one bound is completely contained by another
- `ZoneFindNeighbor` - get the neighbor zone of the given zone which contains a given point
- `ZoneFindNode` - find the point of intersection between a zone octree and a ray and return the node which contains the point of intersection
- `ZoneReboundVector` - get the rebound direction of a given vector within the current zone or neighbor using `ZoneFindNode`
- `ZoneColorsScale` - scale input `gool_colors` by the given red, green, and blue percentages
- `ZoneColorsScaleByNode` - scale input `gool_colors` per the given node subtype
- `ZoneColorSeek` - seek input int16 color component by step amount
- `ZoneColorsScaleSeek` - scale input `gool_colors` per the given node subtype and optionally seek input obj colors towards the scaled input colors
- `ZoneFindNearestNode` - find the octree node within the current zone or a neighbor which is nearest to a given point
- `ZoneFindNearestObjectNode` - find the octree node or object [node] within the current zone or neighbor which is nearest to a given point
- `ZoneFindNearestObjectNode2` - alternative variant of `ZoneFindNearestObjectNode`
- `ZoneFindNearestObjectNode3` - alternative variant of `ZoneFindNearestObjectNode` which colors the object according to node type
- `ZoneQueryOctrees` - query the octrees of the current zone and its neighbors
- `ZoneQueryOctrees2` - `ZoneQueryOctrees` using object trans for trans
- `ZonePathProgressToLoc` - converts a progress to a path point location given a zone path
- `ZoneGetNeighborPath` - get the neighbor path at the given index in the given zone path

## main.c ##
- `main` - main function
- `CoreObjectsCreate` - create main game objects (HUDs) and preload common code entries (box code, crash code, etc.)
- `CoreLoop` - main game loop

## math.c ##
- `ApxDist` - compute the approximate (manhattan distance) between 2 3d points
- `OutOfRange` - test if 2 points are out of range of each other
- `EucDist` - compute the (euclidean) distance between two [3d] points
- `EucDistXZ` - compute the xz plane distance between two [3d] points
- `AngDistXZ` - compute the xz plane angle between two points
- `AngDistXY` - compute the xy plane angle between two points
- `sub_80029E80` - compute the angular distance between two points and limit to the given range
- `sub_80029F04` - unknown
- `_rand` - calculate a pseudo-random number for given seed and max value
- `sranda` - seed pseudo-random number generator a
- `sranda2` - seed pseudo-random number generator a, default value
- `randa` - get pseudo-random number from generator a
- `srandb` - seed pseudo-random number generator b
- `srandb2` - seed pseudo-random number generator b, default value
- `randb` - get pseudo-random number from generator b
- `absdiff` - get the absolute difference of 2 values

## midi.c ##
- `MidiResetSeqVol` - reset volume of the playback sequence
- `MidiInit` - initialize the midi playback engine
- `MidiResetFadeStep` - reset step value for volume fade counters
- `MidiTogglePlayback` - toggle playback of the currently playing sequence
- `MidiGetVab` - get .VAB data from a midi entry
- `MidiOpenAndPlay` - open and begin playback of a midi entry
- `MidiSetStateStopped` - stop midi playback
- `MidiUpdate` - per-frame update function for midi engine; handles changes in state between frames
- `MidiKill` - kill the midi playback engine
- `MidiSeqInit` - set initial sequence number and playback volume
- `MidiClose` - close SEP and VAB
- `MidiControl` - control midi playback (play, stop, pause, resume, or fade)

## misc.c ##
- `ShaderParamsUpdate` - update level-specific world shader parameters
- `ShaderParamsUpdateRipple` - update level-specific world shader parameters; for ripple shader only


## ns.c ##
- `NSEIDToString` - convert an eid to a name/string
- `NSEIDValToString` - convert an eid or entry pointer to a name/string
- `NSStringToEID` - convert a name/string to an eid
- `NSEIDType` - get the entry type from an eid
- `NSAlphaNumChar` - get the alpha-numeric index of a character (A-Za-z0-9)
- `NSGetFilename` - get the (.NSD/.NSF) filename for a given file type and level id
- `NSAtoi` - convert a string to cid or eid or parse from a string an lid (alphanumeric index of first char)
- `NSGetFilenameStr` - get the (.NSD/.NSF) filename for the lid corresponding to the alphanumeric index of the input char
- `NSMallocPage` - allocate [in memory] a page object
- `NSEntryItemSize` - get the size of an entry item
- `NSCountEntries` - get the number of entries of a particular type [in the page table]
- `NSPageTranslateOffsets` - convert relative offsets of entries in a page to absolute pointers, and convert relative offsets of items in each entry to absolute pointers
- `NSPageUpdateEntries` - resolve entry pointer in page table and run `on_load` callback, for each entry in the [physical] page loaded at a given index
- `NSTexturePageAllocate` - reserve a texture page and return the index
- `NSTexturePageFree` - free a previously reserved texture page
- `NSPageOpen` - page the physical or virtual page for a given page id and increment its reference counter
- `NSPageClose` - decrement the reference counter for the physical/virtual page with the given page id
- `NSEntryPageStruct` - search all paged-in pages for the given entry and return the corresponding page struct for the page that contains the entry
- `NSEntryPage` - get the page which contains the given entry
- `NSPageStruct` - get the page struct for the page that contains the given entry (uses `NSEntryPageStruct` for the initial call, storing page id in the page table, and looks up in the page table for future calls)
- `NSOpen` - page the physical or virtual page for a given eid or page table entry and return a pointer to the corresponding entry within the page data
- `NSClose` - close the physical or virtual page for a given eid or page table entry
- `NSCountAvailablePages` - get the number of  virtual and physical pages with a reference count of zero (and which are not in state 1)
- `NSProbe` - get the page table entry for a given eid
- `NSProbeSafe` - get the page table entry for a given eid; avoids continuing search past end of the table if an entry is not found
- `NSResolve` - get a pointer to the entry data for the given pte (for initial call: create page struct and page in page with page id [if needed], and get pointer to entry in that page, for the page id and eid in the given pte, and store the pointer in the page table for future lookup; for future calls: lookup the stored pointer in the page table)
- `NSLookup` - get a pointer to the entry data for the given eid (probe for the pte for the given eid and resolve the entry data for the pte)
- `NSInitTexturePages` - initialize texture page structures
- `NSInitAudioPages` - initialize audio page structures
- `NSInit` - initialize paging subsystem for paging in pages/entries from the corresponding NSF file for the given level id
- `NSPagePhysical` - create anew or get the existing physical page struct for a given page id (and eid), and page in the page with that id

## pad.c ##
- `PadInit` - initialize structures for holding current joypad state and detecting changes in joypad state
- `PadUpdate` - update joypad state and detect changes in joypad state

## pbak.c ##
- `PbakInit` - initialize demo playback
- `PbakStart` - begin demo playback
- `PbakPlay` - load playback entry and begin demo playback
- `PbakChoose` - choose a random demo playback entry

## slst.c ##
- `SlstInit` - allocate double poly id buffers
- `SlstFree` - free poly id buffers allocated by `SlstInit`

### pc ###
- `SlstDecodeForward` - delta decode a poly id list using slst delta item, for a forward change in progress
- `SlstDecodeBackward` - delta decode a poly id list using slst delta item, for a backward change in progress

## solid.c ##
- `TransSmoothStopAtSolid` - wrapper for `TransPullStopAtSolid`
- `TransPullStopAtSolid` - calculate the limited next trans for a given object, zone query, and current trans, by iteratively taking away from velocity, adding to trans, and limiting with `TransStopAtSolid`
- `TransStopAtSolid` - calculate the actual [limited] next trans for a given object, current trans, and projected change in trans [velocity]; add velocity to current trans and limit the result at the faces/edges of solid floors, ceilings, walls, and zone boundaries
- `StopAtHighestObjectBelow` - limit the projected next trans for a given object to the top face of the nearest object bounding box with which 'it' (i.e. the collision box with center at projected next trans) would collide, if any at all
- `FindFloorY` - get the average y location(s) of the top faces of floor nodes in the given zone query which are nearest to the bottom face of the given collision box
- `FindCeilY` - get the average y location(s) of the bottom faces of ceiling nodes in the given zone query which are nearest to the top face of the given collision box
- `StopAtFloor` - limit the projected next trans for a given object to the average of top faces of the nearest solid floor(s) [node bounds] or the top face of a nearer object bounding box
- `StopAtCeil` - limit the projected next trans for a given object to the average of bottom faces of the nearest solid ceiling(s) [node bounds] or the bottom face of a nearer object bounding box
- `StopAtZone` - limit the projected next trans for a given object to the bottom face of the current zone bound or a neighbor zone bound, if the face is solid
- `BinfInit` - set up pointers to (or allocate) the wall bitmap, wall cache, and circle bitmap
- `BinfKill` - free memory allocated by `BinfInit`
- `PlotWall` - set a 16 unit radius circular region of bits in the wall bitmap
- `PlotWallB` - set or clear a rectangular region of bits in the wall bitmap
- `PlotObjWalls` - plot walls in the wall bitmap at the scaled relative locations (in the xz plane) of the nearest object(s) [bound boxes]
- `PlotQueryWalls` - plot walls in the wall bitmap at the scaled relative locations of the nearest solid wall(s) [node bounds]
- `PlotWalls` - plot walls in the wall bitmap at the scaled relative locations [in the xz plane] of the nearest solid wall(s) [node bounds] and object(s) [bound boxes]
- `StopAtWalls` - adjust input bit index (x, z) to the index of the nearest non-solid bit in the wall bitmap

## title.c ##
- `TitleInit` - allocate title screen struct
- `TitleLoadNextState` - load the next title screen state
- `TitleLoadScreen`- load a title screen
- `TitleLoadState` - load a title screen state
- `TitleKill` - free memory allocated by `TitleInit`
- `TitleLoading` - display loading splash screen
- `TitleUpdate` - per-frame update function for the title screen; handles state transitions and calls to the other lower-level title screen Functions
- `TitleCalcUvs` - calculate uv coordinate indices for each imag tile
- `TitleCalcTiles` - calculate infos for each tile, set draw modes, and get tpage idx
- `TitleLoadImages` - load color mode 1 textures into the appropriate locations of vram for each splash screen image tile, based on computed uv indices
- `TitleLoadEntries` - load all entries for a given mdat state
