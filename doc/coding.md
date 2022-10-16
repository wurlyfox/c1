# Coding Standards
## Formatting
- 2 space tab size
- no newlines before open brackets
    ```
    if (cond) {
      do something
    }
    ```
    instead of
    ```
    if (cond)
    {
      do something
    }
    ```
- no increase of nesting level at case statements

## Variables
- variable declarations at beginnings of function definitions
- no initializers in declarations (excluding value initialized globals)
- concise but clear naming preferred (i.e. abbreviations)
- underscore to delimit multi-word names (`int the_var;` instead of `int theVar;` (camelCase))

## Functions
- `UpperCamelCase` for function names
- precede function definition with address of original implementation

## Types
- use plain `int` for:
  - indices, offsets, counts, integral iterators, ids, booleans, error/success codes
- use `uint32_t`, `uint16_t`, `uint8_t` for all other unsigned types
- use `int32_t`, `int16_t`, `int8_t` for all other signed types
- use `void*` for pointer to polymorphic data with no base type

## Style
- minimize nesting level where possible.
  - use `if (cond) { return code; }` instead of
    ```
    if (!cond) {
      do something
    }
    return code;   
    ```

## Project specific
### Common names

#### Generic
- integral iterator - `int i, ii` or `int i, j, k`
- index - `int idx`
- count - `int count`
- result code - `int res`
- value pair - `int a,b`
- character or character iterator - `char [*]c`
- entry - `entry *entry` or `entry *en`
- page - `page *page` or `page *pg`
- page struct - `page_struct *ps`
- page table entry - `nsd_pte *pte`
- entry reference - `entry_ref *ref`
- header - `<entry_type>_header *header`
- gool object - `gool_object *obj`
- gool object child [iterator] - `gool_object *child`
- timestamp (ticks or frames) - `uint32_t timestamp`

#### Specific
- index - `int <name>_idx`
- count - `int <name>_count`
- entry - `entry *svtx,*tgeo,*wgeo,*slst,*zone,*exec`
- page struct (texture page) - `page_struct *tps`
- page struct (audio page) - `page_struct *aps`
- header (secondary) - `<entry_type>_header *<abbrev>_header`
- timestamp - `uint32_t <name>_stamp`

#### Graphics
- model polygon - `<entry_type>_polygon [*]poly`
- local vector (generic) - `vec<subtype> [*]v`
- local vector - `vec<subtype> [*]trans,[*]rot,[*]scale`
- local color - `rgb<subtype> [*][<name>_]color`
- local color pair - `rgb<subtype> c1,c2`
- local uv - `vec2<subtype> [*]uv`
- local vertex - `vec<subtype> *vert`
- local vertex (model coordinates) - `<entry_type>_vertex *vert`
  (amongst usages of the other more specific local vertex name variants)
- local vertex (world/unrotated view coordinates) - `vec<subtype> u_vert`
- local vertex ([rotated] view coordinates) - `vec<subtype> r_vert`
- local vertex (screen coordinates) - `vec<subtype> s_vert`
- local matrix (generic) - `mat<subtype> [*]m`
- local matrix - `mat<subtype> [*]m_<name>`
- global matrix - `mat<subtype> [*]mn_<name>`
- global matrix, scaled - `mat<subtype> [*]ms_<name>`
- graphics primitive buffer - `void **prims_tail`
- local graphics primitive - `<typename> *prim`
- screen proj (global) - `int32_t screen_proj` (gfx.c)
- screen proj (local) - `int32_t proj`
- sin/cos results - `int16_t [s,c][x,y,z]`
- point difference - `vec<subtype> delta`

#### Naming conventions
- current <full_name> - `<typename> cur_<name>`
- previous <full_name> - `<typename> prev_<name>`  (**or `<typename> <name>_prev`)

### Ordering

#### Locals
- important structs/struct pointers first
- ints last, incl. counting iterators and indices

#### Globals
- closest as possible to original implementation order
- separate into `.rdata`, `.data`, `.sdata`, `.bss`, and `.sbss` sections
- follow declaration with comment containing original memory address

#### Includes
- source specific header first
- then in the following order:
  1. `common.h`
  2. `ns.h`
  3. `geom.h`  
  4. `gfx.h`
  5. `gool.h`
  6. `level.h`
  7. `slst.h`
  8. `cam.h`
  9. `solid.h`
 10. `pbak.h`
 11. `audio.h`
 12. `midi.h`
 13. `title.h`

- then any other headers at root level
- then format headers (i.e. headers from /formats), in order of entry
- then target specific headers
