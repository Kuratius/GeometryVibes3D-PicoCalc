### Level Editor (Python)

A Tkinter-based editor for GV3D levels with support for both static obstacles and animated obstacle groups.

Features:
- Place grid-aligned primitive obstacles (square, right triangle, half spike, full spike)
- Apply primitive modifiers where supported
- Horizontal scrolling playfield with vertically centered grid
- Rectangle selection with Shift+drag
- Copy / cut / paste selected cells
- Undo (`Ctrl+Z`)
- Document-style workflow:
  - New
  - Open
  - Save
  - Save As
- Title bar shows current file name, or `Untitled` if unsaved
- Dirty-state indicator (`*`) in the title bar when there are unsaved changes
- Fixed 6-column endcap and portal preview overlay; endcap region is locked
- Supports up to 8 named animation groups
- Create a new animation group from a selected set of primitive obstacles
- Animation group slots can be selected and deleted
- Deleting an animation group removes all references to that group from the level
- Paint animation-group references into the grid
- Supports animation-group anchor offsets:
  - None
  - Shift right by 1/2 cell
  - Shift down by 1/2 cell
  - Shift down-right by 1/2 cell
- In-editor preview of placed animation groups as they appear in-game
- Live playback preview of animation steps
- Edit animation group name, pivot, and step list
- Export JSON project files
- Export packed `.bin` level files compatible with the GV3D runtime format

Notes:
- The editor stores full authoring data in JSON.
- Binary export writes the packed level format used by the game.
- The portal and endcap are generated automatically during export and shown in the editor as overlays.
- Animation-group origins are derived from the weighted average of the selected primitive footprint, snapped to the half-cell grid.

Run:
```bash
python level_editor/level_editor.py