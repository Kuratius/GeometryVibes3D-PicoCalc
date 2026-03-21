# Geometry Vibes 3D - PicoCalc

A faithful RP2040 port of **Geometry Vibes 3D**, targeting the **ClockworkPi PicoCalc**.

Current state: playable wireframe “fake 3D” implementation with a state-driven app flow, persistent save data, selectable gameplay difficulty, collectible level stars, animated obstacle groups with rotation and scaling, gameplay-plane collision against static and animated geometry, and stable fixed-rate rendering on the ILI9488.

## Features

- **Fixed-point 3D** camera + projection
- **State-machine app flow**
  - title screen
  - home menu
  - saved-games menu
  - new-game name entry
  - level select
  - gameplay
  - options
- **Difficulty selection**
  - selectable gameplay difficulty
  - current presets:
    - Rookie
    - Pro
    - Expert
  - difficulty affects scroll speed and fly-out speed at runtime
- **Persistent save system**
  - up to **10 saved games**
  - compact binary save file stored on SD card
  - continue from the most recently played save
  - delete saves from the saved-games menu
- **Level-select menu**
  - highlighted selection
  - locked/unlocked level progression
  - per-level completion percentage display
  - per-level star progress display
  - `[Locked]` status for unavailable levels
- **Collectible stars**
  - each level contains **3 collectible stars**
  - stars are placed directly in packed level cells
  - per-save collection state is persisted
  - collected stars do not respawn after save/load
  - level completion is **not** required to keep collected stars
- **HUD layer** with:
  - controls hint
  - level label
  - progress bar + percentage
  - fading event text
  - collectible-star event notifications
- **Ship controls** with 45° up/down travel like the original
- **Collision detection** against:
  - static level geometry
  - rotation / inversion modifiers
  - animated obstacle groups
  - collectible stars
- **Gameplay-plane collision model**
  - 2D primitive overlap for static and animated obstacles
  - collectible stars handled directly from streamed level data
  - more accurate ship collision than the earlier sphere-style approximation
- **Wireframe effects**, including:
  - animated portal rays
  - ship trail
  - ship explosion chunks
  - rotating collectible stars
- **Animated obstacle groups**
  - grouped primitive definitions stored in the level file
  - per-group pivot
  - per-step rotation and target scale
  - half-cell anchor offsets
  - synced runtime animation playback
- **Per-level colors**
  - background color
  - obstacle / wireframe color
- **Runtime options**
  - enable serial renderer stats output
  - collision-highlight overlay

## Runtime filesystem layout

Runtime data is organized under a dedicated **`/GV3D`** root directory on the SD card.

Current subdirectories include:

- `/GV3D/levels`
- `/GV3D/saves`
- `/GV3D/assets`

This keeps level data, save data, and runtime assets grouped under a single app-specific root.

## Hardware / platform highlights

- **ILI9488 320×320 display**
  - dual-core slab renderer
  - SPI DMA streaming
  - screen-space text + fill-rect overlay primitives
  - stable **~30 FPS** pacing
- **SD card + FAT32**
  - streams level columns on demand from `/GV3D/levels`
  - loads and saves persistent game data under `/GV3D`
  - no full level load into RAM
- **PicoCalc keyboard**
  - polled through the platform/input layer
- **Southbridge integration**
  - battery percentage and charging-state display in the overlay footer

## Rendering notes

- Fixed-capacity render lists using static storage
- Major-axis slab line rasterization for cleaner wireframe output
- ROM-resident **8×8 bitmap font**
- Cached screen-space text objects for HUD and menu rendering
- Scene building and animated-group rendering are driven from level-streamed data
- Animated group rendering supports rotation and uniform scaling around a shared pivot
- Collectible stars are rendered as upright 5-point wireframe stars with a lightweight billboard-style spin
- Overlay/footer rendering supports battery state, warnings, and menu/status text

## Save format

The current runtime stores save data in a compact binary file on the SD card:

- magic: `GVS`
- versioned header
- packed list of active save entries
- up to **10 save entries**
- each entry stores:
  - save name
  - unlocked level count
  - per-level completion percentage
  - per-level collected stars bitmask

Deleting a save compacts the entry list and immediately rewrites the file.

## Level format

The current runtime uses the **GVL3** format:

- 16-byte header
- optional animated-group definition section
- packed 56-bit obstacle columns
- support for:
  - primitive obstacles
  - collectible stars
  - animation-group references
  - half-cell anchor offsets
  - per-level background and obstacle colors

Animated groups are authored as reusable definitions and then referenced from packed level cells.

Animation steps support:

- signed quarter-turn rotation deltas
- target scale in Q7 format (`128 = 1.0`)
- duration in milliseconds

## Tools

- **Level editor**: `tools/level_editor/level_editor.py`
  - Tkinter-based 9×N level editor
  - paint primitive obstacles and modifiers
  - paint collectible stars (up to **3 per level**)
  - create named animated obstacle groups from selected primitives
  - edit group pivot and animation steps
  - edit per-level background and obstacle colors
  - preview placed animation groups in-editor
  - live animation playback preview
  - locked auto-generated endcap + portal preview
  - rectangle select, copy, cut, paste, undo, and redo
  - Open / Save / Save As JSON workflow
  - export packed **GVL3** binary files for the game

- **RGB565 image converter**: `tools/image_convert/convert_rgb565.py`
  - converts source images into raw RGB565 assets for use in-game
  - intended for bitmap-based assets such as the title screen

## Toolchain

- Raspberry Pi Pico SDK v2.2.0
- ARM GCC 14.2
- VS Code Pico Project extension

## Build

In VS Code: **Ctrl+Shift+B**

## Flash

Use the `picotool` task or drag the UF2 in **BOOTSEL** mode.

## Gameplay Images

![Level 1 start](images/ScreenShot1.png)
![Portal rays](images/ScreenShot2.png)
![Level 1 late](images/ScreenShot3.png)
![Level 1 obstacles](images/ScreenShot4.png)
![Title screen](images/ScreenShot5.png)