## Phase 0: Guardrails

- No gameplay logic changes yet. Just reroute rendering plumbing.
- No asset loading yet. First we draw a hardcoded test sprite + hardcoded test text so the display path is validated.
- No heap allocations on the hot path. (Asset loading can use heap if you want, but frame building should be fixed-capacity.)

## Phase 1: Introduce a RenderList without changing behavior

### New files

#### 1) `src/render/RenderList.hpp` (new)

- Replaces “DrawList is only lines” with “RenderList is primitives.”
- Fixed-capacity container (arrays + counters), not `vector`.
- Defines:
  - `PrimType { Line, Sprite, Text }`
  - `PrimLine`, `PrimSprite`, `PrimText`
  - Prim tagged union
- Provides four entry points (append in layer order):
  - `addWorldLine(...)`
  - `addWorldSprite(...)`
  - `addUiSprite(...)`
  - `addUiText(...)` (copies into frame text arena)

#### 2) `src/render/Font8x8.hpp` (new, later in Phase 3 if you prefer)

- ROM glyph table (1bpp), and minimal metadata (first char, count).

### Files to change

#### 3) `src/platform/IDisplay.hpp` (or wherever `IDisplay` lives now)

Change signature from:

- `drawLines(const DrawList&)`

to:

- `draw(const RenderList&)`

Keep `beginFrame/endFrame` unchanged.

#### 4) `src/app/App.hpp/.cpp`

- Replace `DrawList dl;` with `RenderList rl;`
- Replace `display().drawLines(drawList())` with `display().draw(renderList())`
- Keep the rest the same (camera/game update unchanged).

#### 5) `src/render/Renderer.hpp/.cpp`

- Update `buildScene` to write to `RenderList`.
- For now: emit only line prims (so behavior is identical).
- Ship trail/portal markers remain “line-based” for now (no sprites yet).

#### 6) `src/render/DrawList.hpp`

Either:

- delete later, or
- keep it temporarily but stop using it.

(I’d keep it until the new path is proven.)

✅ **End of Phase 1:** Everything still renders exactly as it does today, but the public API is now `RenderList`.

## Phase 2: Convert Ili9488Display from “Frame stores lines” to “Frame stores prims”

This is the big structural refactor, but it’s still “lines only” initially.

### Files to change

#### 7) `src/platform/pico/Ili9488Display.hpp`

Change `Frame` contents from:

- `Line lines[MAX_LINES]`

to:

- `Prim prims[MAX_PRIMS]` (where `PrimLine` matches the old `Line` layout closely)

Keep:

- `slabCount`, `slabOffset`, `slabCursor`, `slabIndices`, `MAX_BINNED_ENTRIES`

Add:

- `char textBuf[TEXT_BUF_BYTES]` and a cursor

(text isn’t used yet, but adding it now keeps the frame layout stable)

#### 8) `src/platform/pico/Ili9488Display.cpp`

Rename methods conceptually:

- `drawLines(const DrawList&)` → `draw(const RenderList&)`

Core0 path becomes:

- Convert incoming prims → internal `Frame::prims` (clip lines only for now)
- Bin prims by Y-span (line case uses min/max y exactly as today)

Core1 path becomes:

- slab loop reads prim index → if `Line`, draw it using the existing Bresenham code

✅ **End of Phase 2:** Still only lines visually, but the frame/binner now works on generic prim indices. This is the core of Option C.

### Memory checkpoint after Phase 2

- Run `arm-none-eabi-size` and `nm` again.
- Goal: `s_frame` does not balloon dramatically.
- We should be replacing line storage with prim storage, not adding parallel buffers.
- The only new RAM should be `textBuf` (1–2 KB/slot) and maybe a few bytes of metadata.

## Phase 3: Add TextPrim (string prim) using ROM 8×8 font

Still no file I/O. Just use a built-in font table.

### Files to change

#### 9) `src/render/RenderList.hpp`

Implement `addUiText(x,y,color565,const char*)`:

- copies into frame-owned `textBuf` arena via `offset+len`
- stores a `TextPrim` with `x,y,color,ofs,len`

#### 10) `src/platform/pico/Ili9488Display.cpp`

In core1 slab rendering:

- When encountering a `Text` prim:
  - iterate chars
  - for each glyph, only draw rows that intersect this slab
  - set pixels to `color565` when bit=1

✅ **End of Phase 3:** You can draw menu labels/HUD text with deterministic cost and small command count.

## Phase 4: Add SpritePrim via SpriteDef IDs (RAM-resident indexed textures)

This is where your “prebuilt sprites referenced by ID” becomes real.

### New files

#### 11) `src/assets/AssetBank.hpp/.cpp` (new)

Owns:

- `Texture[]` (indexed pixel pointers + optional palette pointer + size + format)
- `SpriteDef[]` (texId + rect + w/h + default flags)

Supports:

- `loadMenuBank(fs)`
- `loadGameBank(fs)`
- `unload()`

Uses your `IFileSystem` to load files into RAM.

Defines the contract that pointers remain valid until `unload()`.

#### 12) `src/assets/AssetIds.hpp` (new)

Central list of sprite IDs (e.g., `kSprPortalFrame0`, `kSprCursor`, etc.)

Helps keep “magic numbers” out of gameplay/menu code.

### Files to change

#### 13) `src/render/RenderList.hpp`

`SpritePrim` is compact:

- `int16 x,y`
- `uint16 spriteId`
- `uint8 flags`

No `w/h/srcX/srcY` in prim.

#### 14) `src/platform/pico/Ili9488Display.cpp`

Core1 sprite draw:

- Lookup `SpriteDef` → `Texture`
- Clip sprite rect against slab
- For each pixel: read index byte; if !=0 write `palette[index]` to slab

#### 15) `src/app/App.cpp` + menu/game states (wherever you put state machine)

Add a simple “mode switch” point to reload banks:

- entering menu loads `MenuBank`
- entering gameplay loads `GameBank`

For now, you can keep it manual: call bank load in `App::init` just to prove it.

✅ **End of Phase 4:** Sprites work, indexed palette works, asset reloading works.

## Phase 5: Streaming escape hatch (optional, menus only)

Not required to start, but this is how you avoid RAM blowups later.

### 16) AssetBank enhancement

Allow a texture to be marked “streamable.”

Sprite draw checks:

- resident pointer available → fast path
- streamable → read just the rows needed for this slab into a small scratch row buffer  
  (core1 must not do file I/O; so core0 prefetches into a per-frame buffer or you disable streaming during gameplay and render big images via a “menu draw routine” on core0 only)

Given your current multicore design, the safest approach is:

- streaming path is core0-only on menu screens (no core1), or
- core0 pre-decodes into a tiny “slab sprite cache” per frame slot.

We can decide later—start with resident textures first.

## What stays untouched

- Game logic (until you choose to draw sprites for effects)
- 3D projection/camera math
- Ili9488Display DMA + slab pipeline approach (we only extend the raster stage)
- your current I/O layer, except asset loader additions

## Suggested initial caps (so we don’t blow `s_frame`)

- `MAX_PRIMS`: keep 2048 to match current line limit
- `TEXT_BUF_BYTES`: 1024 per frame slot
- `MAX_BINNED_ENTRIES`: keep current for now (8192) unless `nm` tells us it’s too big
- Sprite count is implicitly capped by `MAX_PRIMS` (and you’ll likely be nowhere near 2048 sprites)