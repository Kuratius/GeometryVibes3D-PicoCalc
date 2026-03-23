# Levels

This folder contains Geometry Vibes 3D level files and level-authoring projects.

## What’s in here

You may see two formats:

- **`.json`** — human-editable authoring format used by the Python level editor
- **`.BIN`** — compact runtime format used by the PicoCalc build

If both exist for the same level, treat the **`.json` as the source of truth** and regenerate the `.BIN` from it.

## Naming

Recommended naming:

- `level_01.json`
- `L01.BIN`

Keep numbering stable so the game can load levels by index.

## Runtime location

At runtime, the PicoCalc build expects level files under the app root on the SD card:

- `/GV3D/levels/L01.BIN`
- `/GV3D/levels/L02.BIN`
- ...
- `/GV3D/levels/L10.BIN`

The game now uses a dedicated `/GV3D` root directory for runtime data, including:

- `/GV3D/levels`
- `/GV3D/saves`
- `/GV3D/assets`

## JSON format (authoring)

A JSON file contains authoring data for a level, including:

- `width`: number of authored columns
- `height`: always `9`
- `cells`: authored cell contents
  - primitive obstacles
  - collectible stars
  - animation-group references
- `animDefs`: up to 8 animation-group definitions
- `backgroundColor565`: level background color in RGB565
- `obstacleColor565`: level obstacle/wireframe color in RGB565

Notes:

- The **player start position is not stored per-level**. It is defined in game configuration.
- The **portal is not an obstacle cell**; it is derived from fixed level metadata and runtime rules.
- The editor maintains a fixed **auto-generated endcap / portal preview** and excludes authored endcap data from export.
- JSON is the editable project format and should be preserved even after exporting `.BIN`.

## Collectible stars

Levels support collectible stars directly in the grid.

Current rules:

- each level must contain **exactly 3 stars**
- stars are authored as normal grid cells
- star ordering is defined by:
  - **column ascending**
  - then **row ascending**
- that ordering is used by the runtime to assign stable star indices for save-data tracking

Stars support the same half-cell anchor offset modes used by other offset-capable content:

- none
- shift right by 1/2 cell
- shift down by 1/2 cell
- shift down-right by 1/2 cell

## Animated groups

The current authoring format supports reusable animated obstacle groups.

An animation group includes:

- a name
- a pivot in half-cell units
- a base scale in Q7
- a list of primitive members
- a list of animation steps

Animation steps store:

- `deltaQTurns`: signed quarter-turn delta for the step
- `targetScaleQ7`: target uniform scale for the step, where:
  - `128 = 1.0`
  - `64 = 0.5`
  - `32 = 0.25`
  - `255 ≈ 1.992`
- `durationMs`: step duration in milliseconds

Placed animation-group references in the grid store:

- which animation slot they use
- the anchor offset mode:
  - none
  - shift right by 1/2 cell
  - shift down by 1/2 cell
  - shift down-right by 1/2 cell

## BIN format (runtime)

Binary files are designed for streaming so the whole map does not need to be loaded into RAM.

Current runtime files use the **GVL3** format.

### Layout

- **16-byte header**
- optional **animation definition section**
- packed level column data

### Header

The header stores:

- magic/version
- level width
- portal offset from the right edge
- background color
- obstacle color
- animation definition count
- maximum primitive count used by any animation definition
- offset to packed column data

### Packed columns

After the animation definition section, the level data is stored as packed columns:

- `width` columns
- each column is **7 bytes (56 bits)**

Each column stores **9 cells**, each using **6 bits**:

- low 4 bits: obstacle ID
- high 2 bits:
  - primitive modifier for normal obstacles
  - offset mode for stars, animation-group references, and other offset-capable cells

This yields **54 used bits** plus **2 spare bits** per column.

### Cell / obstacle IDs

Cells may contain:

- empty space
- primitive obstacles
- collectible stars
- animation-group references

Primitive obstacles currently include:

- square
- right triangle
- half spike
- full spike

Animation-group references occupy reserved obstacle ID slots in the packed column format.

### Animation definition section

Each animation definition contains:

- an `AnimGroupDefHeader`
- `primitiveCount` primitive records
- `stepCount` step records

Each definition stores:

- primitive count
- pivot
- step count
- base scale in Q7
- total animation duration

Each step record stores:

- signed quarter-turn delta
- target scale in Q7 format
- duration in milliseconds

## Runtime behavior

Runtime level data is streamed column-by-column from the `.BIN` file.

Important implications:

- the full level is **not** loaded into RAM
- collision/rendering operate on streamed column data
- collectible stars are **not preloaded into a full-level runtime list**
- star rendering and collection checks operate directly from streamed level cells
- collected-star state is tracked in save data using the stable col-then-row star ordering

## Editing / Export

Use the Python level editor in `tools/level_editor/` to:

- create or modify `.json`
- define animated groups
- place primitive obstacles, stars, and animation-group references
- choose level background and obstacle colors
- edit per-step rotation, scale, and duration
- export `.BIN` for runtime use

Typical workflow:

1. Open or create `level_XX.json`
2. Edit primitive obstacles, stars, colors, and animation groups
3. Save the JSON project
4. Export `LXX.BIN`
5. Copy the `.BIN` file to the SD card under `/GV3D/levels/`

## Adding new levels

1. Create a new level in the editor with the desired width
2. Place primitive obstacles, stars, and/or animation-group references
3. Set background and obstacle colors if desired
4. Save the JSON project
5. Export the `.BIN` file
6. Store the `.BIN` on the SD card in the expected runtime location, for example:
   - `/GV3D/levels/L01.BIN`
   - `/GV3D/levels/L10.BIN`

## Important

- Keep the JSON files under version control
- Regenerate `.BIN` files whenever the JSON changes
- Prefer editing with the level editor instead of hand-editing the binary format
- Keep level numbering stable once a release depends on it