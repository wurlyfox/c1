# `/src/pc/gfx/tex.c` #

Texture cache, for paletted textures.

## terms ##

- **texture page** - refers to the actual source texture page data in nsf memory
- **texture atlas** - refers to the destination pixel buffer in the cache to where converted subregions of a particular texture page are copied
- **texture** - refers to a converted subregion of a particular texture atlas
- **texinfo** - a descriptor for a single texture; there is a one-to-one correspondence between texinfos and textures. Parts of the texinfo are used as a key for looking up the corresponding texture. Should the same texinfo be looked up twice, both lookups will return the same texture, but only the first lookup will cause a new texture to be created (i.e. converted and copied from source texture page).

## description ##

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
