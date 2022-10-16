# `/src/pc/gfx/tex.c` #

Texture cache, for paletted textures.

## terms ##

- **texture page** - refers to the actual source texture page data in nsf memory
- **texture atlas** - refers to the destination pixel buffer in the cache to where converted subregions of a particular texture page are copied
- **texture** - refers to a converted subregion of a particular texture atlas
- **texinfo** - a descriptor for a single texture; there is a one-to-one correspondence between texinfos and textures. Parts of the texinfo are used as a key for looking up the corresponding texture. Should the same texinfo be looked up twice, both lookups will return the same texture, but only the first lookup will cause a new texture to be created (i.e. converted and copied from source texture page).

## description ##

The psx gpu supports *paletted textures* (enabled for graphics primitives of color modes 0 and 1). Many of the textures used in the crash games are paletted textures.

Texture data in the crash games is a combination of raw 15 bit direct colors and 8 or 4 bit indices. Each index indirectly references a particular one of the 15 bit colors in the non-indexed portion of the data. The raw 15 bit direct colors portions of the data can be divided into two parts:
- raw direct colors (non-indexed pixels), used by color mode 2
- raw 'palette' colors referenced by indexed pixels, used by color mode 0 and 1 (a row of these is called a 'clut'/'color lookup table')

Modern gpus do not have paletted texturing as a standard/built-in feature. To implement the functionality either shaders can be written or pixel lookups can be done with software before uploading textures to the gpu.

This file implements the software-based solution.

The following must be done in converting the region of pixel data referred to by a polygons uvs:
- convert (if necessary) via palette lookup using the polygon's palette
- convert from 15 bit rgb (5 bpc) to 32 bit (8 bpc)
- copy 32 bit pixels to the corresponding region in a destination texture, as a subtexture

The destination texture can then be uploaded to the gpu as necessary and the uvs/texid recalculated accordingly. The same destination texture can be reused for all regions of the source texture which use the same color mode. In the case of crash/psx gpu, all textures are regions of 1 of 8 texture pages which occupy the gpu's vram in its entirety; so there can be 3 destination textures per texture page, 1 per color mode.

Furthermore, since multiple references to any given texture region should be expected throughout the game's lifetime (as there can be many polygons amongst one or more models that use the same texture region) it is necessary to have a cache to prevent unnecessarily converting and copying the same region of pixel data into the destination texture for each reference.

The `TextureLoad` function is used to create and/or lookup existing uv coordinates for a texinfo; upon creation, the region of texture page pixels described by the texinfo are converted and copied to a rectangular region of a destination texture in the cache; the uvs are simply the corresponding quad.

# `/src/pc/sound/audio.c` #

Fluidsynth pc audio backend.

Configuration is fairly straightforward.
- There are 24 voices. each channel is dedicated to a separate voice. This way volumes can be controlled individually
- [2 channel] volume is controlled by calculating pan amount and base volume from the left and right values and issuing cc 7 (which is mapped to initial attenuation, by default) with the base volume and setting the pan generator with the pan value.
- Sample pitch can only be affected via modulation. This is because the original (psx) impl does all manipulation of pitch via the 'pitch' voice attribute in the `VoiceAttrs`. Pitch control is achieved by setting the pitch wheel value.
- There is an array of samples, one for each voice/channel. Loading data into a sample simply involves calling the corresponding fluidsynth function and storing the result loaded sample at the index for the corresponding voice; the global sfloader is configured to, upon note on events in a particular channel, allocate and start an sf voice with that channel/voice's corresponding sample.

# `/src/ext/lib/refl.c` #

Minimal type reflection lib (c1 extension).

## usage ##
Define a `refl_type` for each type needing reflection support and call `ReflInit` with an array of pointers to these types. Code following the `ReflInit` will then be able to use library functions for any of the defined types.

See `/src/ext/lib/refl.md` for overview and reference.

# `/src/ext/refl.c` #

Type reflection metadata.

## description ##
In this file we explicitly specify a list of the field names and typenames for each structure type. Macros convert these into initializer lists with the necessary field metadata, including offset of field in the existing structure definition along with name and typename. This type information is provided to the reflection library so that reflection can be used on structures of these types. (In particular, the type reflection functions are used along with the gui functionality to map runtime game data to custom views or trees of 'gui items', for realtime analysis/modification/debugging/etc.)

The remaining information is computed at runtime, including typename lookup, sizes of variable size fields, and offsets of fields packed after variable sized ones. For the latter we have to provide a bit of extra information to the structure parser, including callbacks to calculate sizes of elements of 2-dimensional primitive arrays (i.e. entry item sizes), and to calculate counts of variable length arrays when the length is some function of another field value (ex. count + 1).

See `/ext/lib/refl.md` for an overview of the type reflection functionality.

# `/src/ext/gui.c` #

## terminology ##
- *item* - an item is the basic building block for all other gui entities
- *base item* - an item with no child items and therefore which has minimal complexity; the 4 types of base item include: static item (no type), label, button, and slider
- *control* - an item consisting of one or more child items and thus which is considered higher in complexity than base items; a control, as a whole, is still considered 'single-purpose' (as is a base item) and therefore is not too much higher in complexity.
  examples of 'built-in' controls include: *editable text, scrollbar, and list (*excluding the child cursor, editable text can be considered a base item)
- *view* - an item with one or more child items/controls, which is generally considered higher in complexity than both controls and base items; almost always a list
- *specific view*  - a view which is not wholistically bound to any particular data and thus can have a user-defined or application-specific form; examples are not limited to entire hierarchies of controls, any of which can be dependent on one another; usually complex enough to be considered 'multi-purpose'; a specfic view IS AN item, and HAS one or more controls/items
- *generic view* - a view which is bound to particular data and thus takes a form which is synchronized with the data; from the inner-scope pov the view could be considered multi-purpose but from the outer-scope it is said to be 'single-purpose' due to the wholistic nature of the bound data; therefore such views can be referred to as controls (ex. list[view], tree[view]); a generic view IS A control and IS AN item, and HAS one or more controls/items
- *custom control* - a user-defined (non-built-in) control, possibly a generic view
- *custom view* - a user-defined specific view; does not include generic views

## contents ##
the /src/ext/gui.c file consists of:

- global map and filter callbacks
- item hierarchy search and/or replace helper functions
- custom controls (generic or specific)
  - constructors
  - mappers for mapping any and all parts of the input data to [parts of] a control
  - specific 'deconstructors' or extractors to break apart input data into mappable sub-parts (to assist with unfold-type maps)
  - default handlers for root items or items which map from any parts of the input data
  - parsers and unparsers for text items which map from any parts of input data
- custom views (not incl generic views)
  - constructors for views
  - specific handlers for view items (particularly those that are dependent on multiple items in the view)
- main gui endpoint wrappers
  - init, update, draw

`GuiItemReplace` - item replace helper function

### Extractors ###
`GuiObjChildren` - specific extractor of children for gool objects; used to tree-map a gool object and its hierarchy
(unparsers)
`GuiUnparseObjPtr` - unparser for text item rep of gool objects

### Generic mappers ###
(for structure fields mapped via reflection)

`GuiReflText` - default mapper for primitive type fields
`GuiReflList` - default mapper for unimplemented structure (non-primitive) type fields
`GuiReflVec`  - default mapper for `vec` type fields
`GuiReflVec16` - default mapper for `vec16` type fields
`GuiReflAng`  - default mapper for `ang` type fields
`GuiBoxColor` - callback function for background color of static controls resulting from `GuiReflRgb8`
`GuiBoxColor16` - callback function for background color of static controls resulting from `GuiReflRgb16`
`GuiReflRgb8` - default mapper for `rgb8` type fields
`GuiReflRgb16` - default mapper for `rgb16` type fields
`GuiReflMat16` - default mapper for `mat16` type fields
`GuiReflBound` - default mapper for `bound` type fields
`GuiReflObjPtr` - default mapper for `gool_object` pointer type fields

### Mappers ###
`GuiMapObjField` - main mapper for `gool_object` structure fields

### Custom control constructors ###
`GuiObjView` - constructor for generic view of gool objects; main mapper for `gool_object`
`GuiObjTree` - constructor for `gool_object` tree specialization of `GuiTextTree`

### Custom views ###
`GuiObjSelect` - custom handler for selecting an object in the object tree in the debug view
`GuiDebugView` - constructor for the debug view

### Main lower level gui wrappers ###
`GuiInit` - initializer to register callbacks with the lower level gui system and create the root item
`GuiUpdate` - function to update the item hierarchy (via lower level gui call)
`GuiDraw` - function to draw the item hierarchy (via lower level gui call)
