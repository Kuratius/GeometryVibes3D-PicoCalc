# Geometry Vibes 3D - PicoCalc

A faithful RP2040 port of **Geometry Vibes 3D**, targeting the **ClockworkPi PicoCalc**.

Current state: playable wireframe “fake 3D” implementation with a level-select menu (5 levels), HUD, portal/ship effects, and stable fixed-rate rendering on the ILI9488.

## Features

- **Fixed-point 3D** camera + projection (no frame-time floats required)
- **Streaming level playback** using the **GVL1 / 56-bit column format**
- **Level-select menu** with highlighted selection
- **HUD layer** with:
  - controls hint
  - level label
  - progress bar + percentage
  - fading file-load event text
- **Ship controls** with 45° up/down travel like the original
- **Collision detection** against level geometry, including **rotation/inversion modifiers**
- **Wireframe effects**, including:
  - animated portal rays
  - ship trail
  - ship explosion chunks

## Hardware / platform highlights

- **ILI9488 320×320 display**
  - dual-core slab renderer
  - SPI DMA streaming
  - screen-space text + fill-rect overlay primitives
  - stable **~30 FPS** pacing
- **SD card + FAT32**
  - streams `levels/*.BIN` columns on demand
  - no full level load into RAM
- **PicoCalc keyboard**
  - polled through the platform/input layer

## Rendering notes

- Fixed-capacity render lists using static storage
- Major-axis slab line rasterization for cleaner wireframe output
- ROM-resident **8×8 bitmap font**
- Cached screen-space text objects for HUD and menu rendering

## Tools

- **Level editor**: `tools/level_editor/level_editor.py`
  - tkinter-based 9×N obstacle editor
  - place ship start position
  - paint obstacles and modifiers
  - locked auto-generated endcap + portal preview
  - rectangle select, copy, cut, paste, and undo
  - import/export JSON
  - export packed **GVL1** binary files for the game

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

