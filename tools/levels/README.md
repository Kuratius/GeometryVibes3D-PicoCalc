# Levels

This folder contains Geometry Vibes 3D level files.

## What’s in here

You may see two formats:

- **`.json`** — human-editable authoring format used by the Python level editor
- **`.bin`** — compact runtime format used by the PicoCalc build

If both exist for the same level, treat the **`.json` as the source of truth** and regenerate the `.bin` from it.

## Naming

Recommended naming:

- `level_01.json`
- `L01.BIN`

Keep numbering stable so the game can load levels by index.

## JSON format (authoring)

A JSON file contains authoring data for a level, including:

- `width`: number of columns
- `height`: always `9`
- `cells`: authored cell contents
  - primitive obstacles
  - animation-group references
- `animations`: up to 8 animation-group definitions
- `portal`: metadata stored relative to the last column
- `endcap`: metadata about the fixed auto-generated endcap
- optional editor/runtime metadata such as background color

Notes:

- The **player start position is no longer stored per-level**. It is defined in the game configuration.
- The **portal is not an obstacle**; it is metadata.
- The last **6 columns** are a fixed endcap structure that the editor:
  - renders as a ghost overlay
  - locks from editing
  - auto-appends on export
- JSON is the editable project format and should be preserved even after exporting BIN.

## Animated groups

The current authoring format supports reusable animated obstacle groups.

An animation group includes:

- a name
- a pivot in half-cell units
- a list of primitive members
- a list of animation steps

Animation steps currently support:

- **rotate**
- **hold**

Placed animation-group references in the grid store:

- which animation slot they use
- the anchor offset mode:
  - none
  - shift right by 1/2 cell
  - shift down by 1/2 cell
  - shift down-right by 1/2 cell

## BIN format (runtime)

Binary files are designed for streaming so the whole map does not need to be loaded into RAM.

Current runtime files use the **GVL2** format.

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
  - animation-group anchor offset for animation references

This yields **54 used bits** plus **2 spare bits** per column.

### Obstacle IDs

Cells may contain:

- primitive obstacles
- animation-group references
- empty cells

Primitive obstacles currently include:

- square
- right triangle
- half spike
- full spike

Animation-group references occupy reserved obstacle ID slots in the packed column format.

## Editing / Export

Use the Python level editor in `tools/level_editor/` to:

- create or modify `.json`
- define animation groups
- place animation-group references
- export `.bin` for runtime use

Typical workflow:

1. Open or create `level_XX.json`
2. Edit primitive obstacles and animation groups
3. Save the JSON project
4. Export `LXX.BIN`
5. Copy the BIN file to the SD card for runtime use

## Adding new levels

1. Create a new level in the editor with the desired width
2. Place primitive obstacles and/or animation-group references
3. Save the JSON project
4. Export the BIN file
5. Store the BIN on the SD card in the expected runtime location, for example:
   - `levels/L01.BIN`
   - `levels/L02.BIN`

## Important

- Keep the JSON files under version control
- Regenerate BIN files whenever the JSON changes
- Prefer editing with the level editor instead of hand-editing the BIN format