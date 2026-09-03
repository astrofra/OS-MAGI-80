# MAGI-80 Graphics Architecture --- Performance-Oriented Design Notes

## Purpose

MAGI-80 should not behave like a generic chunky framebuffer awkwardly
mapped onto an Amiga 1200. A more promising direction is to define a
small **virtual graphics architecture whose layers deliberately map onto
the strengths of AGA**.

The main objective is predictable performance on a stock A1200, with
**25 Hz as a safe baseline** and 50 Hz available when a cartridge stays
within budget.

The central principle is:

> **Do not redraw or convert pixels when the Amiga chipset can move,
> compose, or display them directly.**

------------------------------------------------------------------------

## 1. A Three-Layer Virtual GPU

The proposed MAGI-80 display can be understood as three conceptually
independent layers.

### Layer A --- Planar / Blitter Layer

A native planar layer intended for operations that map efficiently to
Amiga hardware.

Typical uses:

-   tile maps;
-   backgrounds;
-   large filled areas;
-   scrolling scenery;
-   bitmap fonts;
-   HUD elements;
-   simple 2D primitives;
-   graphics that can be copied or manipulated efficiently by the
    Blitter.

This layer should avoid chunky-to-planar conversion entirely whenever
possible.

Its implementation can use:

-   native AGA bitplanes;
-   the Blitter;
-   bitplane pointer manipulation;
-   hardware fine scrolling.

This is the layer where MAGI-80 can exploit the Amiga rather than fight
it.

------------------------------------------------------------------------

## 2. Layer B --- Chunky / Pixel Layer

The chunky layer is the expensive but flexible layer.

It is intended for effects that genuinely benefit from random pixel
access:

-   software 3D;
-   plasma and procedural effects;
-   raycasting;
-   particle effects;
-   arbitrary pixel drawing;
-   other framebuffer-oriented algorithms.

The important design change is that **this layer does not need to cover
the complete 256 × 256 display**.

A useful default could be:

-   160 × 128 for a small viewport;
-   192 × 160 for a medium/default viewport;
-   256 × 256 only as an explicitly expensive full-screen mode.

For example:

``` text
MAGI-80 logical display: 256 × 256

┌──────────────────────────────┐
│      PLANAR / BLITTER        │
│                              │
│    ┌──────────────────┐      │
│    │                  │      │
│    │  CHUNKY VIEWPORT │      │
│    │     192×160      │      │
│    │                  │      │
│    └──────────────────┘      │
│                              │
│       OBJECT / HUD           │
└──────────────────────────────┘
```

A 192 × 160 × 4-bit chunky framebuffer contains only 15 KiB of pixel
data, substantially reducing the amount of data involved in C2P
conversion.

The surrounding screen remains useful for scenery, frames, instruments,
text, HUDs, cockpits, etc.

------------------------------------------------------------------------

## 3. Layer C --- Object / Sprite Layer

The third layer can expose an **object-oriented sprite/tile
abstraction**, conceptually closer to a Mega Drive sprite table than to
direct Amiga sprite programming.

For example:

``` lua
object_set(slot, tile, x, y, flags)
```

or:

``` lua
obj(0, HERO, 120, 100)
obj(1, ENEMY, 180, 80)
```

The programmer should not normally manipulate physical Amiga sprite
channels.

Instead, MAGI-80 maintains a virtual object list and attempts to map it
onto AGA hardware sprites.

Possible implementation strategies include:

-   direct hardware sprites;
-   attached sprites for additional colours;
-   vertical sprite multiplexing;
-   Copper-assisted scheduling;
-   fallback rendering into another layer when hardware sprite resources
    are exhausted.

This layer is particularly attractive for:

-   players and enemies;
-   projectiles;
-   icons;
-   status indicators;
-   relatively static objects;
-   tile-like graphical elements.

Because sprite image data can remain resident in memory, moving an
object may require changing only coordinates and control data rather
than redrawing framebuffer pixels.

------------------------------------------------------------------------

## 4. AGA Sprites as a Third Visual Plane

AGA hardware sprites can therefore be treated conceptually as a **third
compositing plane**, although they are not a true bitmap playfield.

AGA supports sprite widths of 16, 32, and 64 pixels.

Conceptually:

-   4-colour sprites use one hardware sprite channel;
-   16-colour attached sprites use a pair of channels.

This makes a virtual object/tile layer plausible, especially at
MAGI-80's modest 256-pixel logical width.

The important architectural decision is to expose **virtual objects**,
not the physical sprite channels themselves. The runtime remains free to
allocate, multiplex, or fall back as necessary.

------------------------------------------------------------------------

## 5. Independent Hardware Scrolling

Each logical layer should ideally expose a camera/scroll offset:

``` lua
layer_scroll(PLANAR, x, y)
layer_scroll(CHUNKY, x, y)
layer_scroll(OBJECTS, x, y)
```

or possibly a common camera abstraction:

``` lua
camera(x, y)
```

The backend can implement this differently for each layer.

### Planar layer

Scrolling can primarily use:

-   AGA fine scrolling;
-   bitplane pointer adjustment;
-   a small amount of additional backing storage.

### Chunky layer

The converted planar representation of the chunky viewport can have a
small hidden margin around the visible viewport.

For example:

``` text
Chunky visible viewport: 192 × 160
Planar backing area:     256 × 224

             ±32 px margin
```

The visible viewport can then move inside this backing area without
immediately requiring a complete redraw and C2P conversion.

### Object layer

Scrolling is particularly cheap: the camera offset can simply be applied
to object/sprite coordinates.

------------------------------------------------------------------------

## 6. Limited Scroll Margin Instead of Larger Drawing Areas

A useful MAGI-80 constraint would be to guarantee only a small amount of
hardware-assisted scrolling, for example **±16 or ±32 pixels**.

The important distinction is:

> The backing representation may be slightly larger, but the application
> is not required to redraw a larger viewport every frame.

Example for a 256-pixel planar display:

``` text
                 backing store

     32 px          256 px          32 px
┌───────────┬────────────────────┬───────────┐
│           │                    │           │
│           │   visible window   │           │
│           │                    │           │
└───────────┴────────────────────┴───────────┘
                ⇆ ±32 pixels
```

When scrolling slowly, newly exposed material can potentially be
generated incrementally.

For a tile map, this could mean updating only a newly visible row or
column rather than rebuilding the entire screen.

------------------------------------------------------------------------

## 7. C2P Strategy

The chunky layer still requires chunky-to-planar conversion, but several
factors make the MAGI-80 case more favourable than a conventional
full-screen 8-bit C2P.

### Reduced viewport

A 192 × 160 framebuffer contains far fewer pixels than a 256 × 256
full-screen framebuffer.

### Reduced colour depth

If the chunky layer uses 4 bits per pixel, only four bitplanes need to
be produced.

### Blitter-assisted conversion

The conversion should be benchmarked using at least:

1.  CPU-only 68020 C2P;
2.  Blitter-assisted C2P;
3.  CPU/Blitter hybrid C2P.

The Blitter can perform logical operations, shifts, and memory transfers
useful for parts of the bit permutation.

However, CPU and Blitter share access to Chip RAM, so Blitter assistance
is not automatically faster. Actual measurements on an A1200-class
configuration are essential.

### Pipeline opportunities

Where useful, CPU work and Blitter work may overlap:

``` text
CPU                         BLITTER

draw chunky frame N
        │
prepare conversion
        │──────────────────► convert/copy
draw other work N+1
```

The achievable overlap depends heavily on Chip RAM contention and must
be measured rather than assumed.

------------------------------------------------------------------------

## 8. Avoiding C2P Entirely

The best C2P optimization is not performing C2P.

Because the MAGI-80 graphics API is abstract, high-level operations do
not necessarily have to modify a chunky framebuffer.

For example:

``` lua
cls(...)
map(...)
spr(...)
rectfill(...)
print(...)
```

can potentially have hardware-specific implementations.

Depending on the target layer, an operation could become:

``` text
MAGI graphics command
        │
        ├── planar native operation
        │       └── CPU / Blitter
        │
        ├── chunky operation
        │       └── framebuffer → C2P
        │
        └── object operation
                └── AGA sprite scheduler
```

This preserves a simple fantasy-console API while allowing a highly
Amiga-specific implementation underneath.

------------------------------------------------------------------------

## 9. Dirty-Layer Updates

The three layers should not implicitly force one another to update.

A typical game frame might contain:

``` text
PLANAR BACKGROUND
    unchanged
        ↓
    no work

CHUNKY VIEWPORT
    3D updated
        ↓
    C2P viewport only

OBJECT LAYER
    player moved
        ↓
    sprite coordinates only
```

Conversely, a scrolling tile-map game might barely use the chunky layer
at all.

This makes **layer dirtiness** an important part of the internal
renderer.

------------------------------------------------------------------------

## 10. Frame-Rate Contract

MAGI-80 should favour deterministic performance over an optimistic
nominal frame rate.

A sensible baseline is:

``` text
PAL video output     50 Hz
standard game tick   25 Hz
```

The same rendered frame can be displayed for two PAL frames.

A cartridge that satisfies a stricter graphics/CPU budget could
optionally request:

``` text
game tick            50 Hz
```

This could become two explicit performance profiles:

### MAGI-80 Standard

-   50 Hz PAL output;
-   25 Hz simulation/rendering;
-   predictable baseline on a stock target;
-   all standard graphics features available within documented budgets.

### MAGI-80 Turbo

-   50 Hz PAL output;
-   50 Hz simulation/rendering;
-   stricter resource budgets;
-   cartridge explicitly opts in.

If a frame misses its deadline, repeating the previous display frame is
preferable to allowing simulation timing to become unpredictable.

------------------------------------------------------------------------

## 11. Profiling as Part of the Fantasy Hardware

The development environment should make the hardware budget visible.

For example:

``` text
CPU   ███████████░░░░   72%
C2P   █████░░░░░░░░░░   31%
BLT   ████░░░░░░░░░░░   26%
AUD   ██░░░░░░░░░░░░░   11%

FRAME 18.3 / 40.0 ms
```

Other useful counters could include:

-   chunky pixels touched;
-   C2P bytes converted;
-   Blitter occupancy;
-   dirty layers;
-   hardware sprite channels used;
-   sprite allocation failures/fallbacks;
-   Chip RAM bandwidth estimates;
-   25/50 Hz deadline misses.

Performance limitations then become visible properties of the fantasy
machine rather than mysterious emulator slowdowns.

------------------------------------------------------------------------

## 12. Recommended Phase-0 Prototype

Before implementing the complete language/compiler/runtime stack, build
a small A1200 graphics benchmark.

It should test at least:

``` text
1. 256×256 full-screen 8-bit C2P
2. 256×256 4-bit C2P
3. 192×160 4-bit C2P
4. 160×128 4-bit C2P
5. CPU-only C2P
6. Blitter-assisted C2P
7. CPU/Blitter hybrid C2P
8. native planar tile rendering
9. planar hardware scrolling
10. ±16 / ±32 pixel backing margins
11. AGA sprite object layer
12. attached sprites
13. sprite multiplexing
14. simultaneous bitplane + Blitter + sprite DMA load
```

Measure each case at both 25 Hz and 50 Hz targets.

Raster bars or hardware counters should make the time consumed by
drawing, C2P, Blitter work, and idle time immediately visible.

This prototype determines the actual MAGI-80 graphics specification
before substantial work is invested in the compiler.

------------------------------------------------------------------------

## 13. Resulting MAGI-80 Model

The resulting virtual GPU can be summarized as:

``` text
                         MAGI-80 GPU
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
    PLANAR / TILE          CHUNKY              OBJECT
          │                   │                   │
    native bitplanes       framebuffer        virtual list
          │                   │                   │
 CPU + Blitter         CPU + hybrid C2P    sprite allocator
          │                   │                   │
 hardware scroll       hardware scroll     coord. offset
          │                   │                   │
          └───────────────────┼───────────────────┘
                              │
                       AGA composition
                              │
                         PAL 50 Hz
```

This is more interesting than simply emulating a generic framebuffer.

MAGI-80 becomes a deliberately constrained hybrid architecture
combining:

-   Amiga planar graphics;
-   Blitter acceleration;
-   a small chunky framebuffer for expensive pixel effects and software
    3D;
-   AGA sprites exposed as a virtual object/tile layer;
-   hardware-assisted independent scrolling;
-   predictable performance budgets.

The fantasy abstraction remains simple, while its implementation
deliberately exploits the unusual strengths of the Amiga chipset.

The key design rule remains:

> **Pixels should only be regenerated when their content changes.
> Movement, composition, scrolling, and object positioning should be
> delegated to the chipset whenever possible.**
