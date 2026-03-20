#!/usr/bin/env python3
from __future__ import annotations

import json
import math
import tkinter as tk
from tkinter import filedialog, messagebox, colorchooser
from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple, Any, List, Union

# -----------------------------
# Spec constants
# -----------------------------
GRID_H = 9
DEFAULT_W = 332
CELL = 28
GRID_LINE = "#2b2b2b"

LEVEL_MAGIC = b"GVL"
LEVEL_VERSION = 3
DEFAULT_BACKGROUND_COLOR_565 = 0x0000
DEFAULT_OBSTACLE_COLOR_565 = 0x07E0

MAX_ANIM_GROUPS = 8

# Base shapes
SHAPE_SQUARE = "square"
SHAPE_RIGHT_TRI = "right_tri"
SHAPE_HALF_SPIKE = "half_spike"
SHAPE_FULL_SPIKE = "full_spike"
SHAPE_STAR = "star"
SHAPES = [SHAPE_SQUARE, SHAPE_RIGHT_TRI, SHAPE_HALF_SPIKE, SHAPE_FULL_SPIKE]

# Modifiers (optional)
MOD_NONE = "none"
MOD_ROT_L = "rot_left"
MOD_ROT_R = "rot_right"
MOD_INVERT = "invert"
MODS = [MOD_NONE, MOD_ROT_L, MOD_ROT_R, MOD_INVERT]
MOD_SHAPES = {SHAPE_RIGHT_TRI, SHAPE_HALF_SPIKE, SHAPE_FULL_SPIKE}

# Editor paint modes
PAINT_MODE_PRIMITIVE = "primitive"
PAINT_MODE_ANIM_REF = "anim_ref"
PAINT_MODE_STAR = "star"

# Shared packed-cell offset mods
OFF_NONE = "none"
OFF_RIGHT = "shift_right"
OFF_DOWN = "shift_down"
OFF_DOWN_RIGHT = "shift_down_right"
OFFSET_MODS = [OFF_NONE, OFF_RIGHT, OFF_DOWN, OFF_DOWN_RIGHT]

# Colors
PRIM_OUTLINE = "#d0d0d0"
PRIM_FILL = "#0b0b0b"
ANIM_OUTLINE = "#4fc3f7"
ANIM_FILL = ""
ANIM_OWNING_SELECTED = "#ffd54a"
ANIM_GHOST = "#7fdfff"
STAR_OUTLINE = "#ffd54a"

# -----------------------------
# Endcap
# -----------------------------
ENDCAP_W = 6
ENDCAP_OBS = [
    {"dx": -5, "y": 0, "shape": "right_tri", "mod": "rot_left"},
    {"dx": -4, "y": 0, "shape": "square"},
    {"dx": -3, "y": 0, "shape": "square"},
    {"dx": -2, "y": 0, "shape": "square"},
    {"dx": -1, "y": 0, "shape": "square"},
    {"dx":  0, "y": 0, "shape": "square"},
    {"dx": -4, "y": 1, "shape": "right_tri", "mod": "rot_left"},
    {"dx": -3, "y": 1, "shape": "square"},
    {"dx": -2, "y": 1, "shape": "square"},
    {"dx": -1, "y": 1, "shape": "square"},
    {"dx":  0, "y": 1, "shape": "square"},
    {"dx": -3, "y": 2, "shape": "right_tri", "mod": "rot_left"},
    {"dx": -2, "y": 2, "shape": "square"},
    {"dx": -1, "y": 2, "shape": "square"},
    {"dx":  0, "y": 2, "shape": "square"},
    {"dx": -2, "y": 3, "shape": "square"},
    {"dx": -1, "y": 3, "shape": "square"},
    {"dx":  0, "y": 3, "shape": "square"},
    {"dx": -2, "y": 4, "shape": "square"},
    {"dx": -1, "y": 4, "shape": "square"},
    {"dx":  0, "y": 4, "shape": "square"},
    {"dx": -2, "y": 5, "shape": "square"},
    {"dx": -1, "y": 5, "shape": "square"},
    {"dx":  0, "y": 5, "shape": "square"},
    {"dx": -3, "y": 6, "shape": "right_tri"},
    {"dx": -2, "y": 6, "shape": "square"},
    {"dx": -1, "y": 6, "shape": "square"},
    {"dx":  0, "y": 6, "shape": "square"},
    {"dx": -4, "y": 7, "shape": "right_tri"},
    {"dx": -3, "y": 7, "shape": "square"},
    {"dx": -2, "y": 7, "shape": "square"},
    {"dx": -1, "y": 7, "shape": "square"},
    {"dx":  0, "y": 7, "shape": "square"},
    {"dx": -5, "y": 8, "shape": "right_tri"},
    {"dx": -4, "y": 8, "shape": "square"},
    {"dx": -3, "y": 8, "shape": "square"},
    {"dx": -2, "y": 8, "shape": "square"},
    {"dx": -1, "y": 8, "shape": "square"},
    {"dx":  0, "y": 8, "shape": "square"},
]

PORTAL_REL = {"dx": -3, "y": 4}

# -----------------------------
# Binary mappings
# -----------------------------
SHAPE_ID = {
    "empty": 0,
    "square": 1,
    "right_tri": 2,
    "half_spike": 3,
    "full_spike": 4,
    "star": 5,
}
ID_SHAPE = {v: k for k, v in SHAPE_ID.items()}

MOD_ID = {
    "none": 0,
    "rot_left": 1,
    "rot_right": 2,
    "invert": 3,
}

OFFSET_MOD_ID = {
    OFF_NONE: 0,
    OFF_RIGHT: 1,
    OFF_DOWN: 2,
    OFF_DOWN_RIGHT: 3,
}

# -----------------------------
# Data model
# -----------------------------

@dataclass(frozen=True)
class PrimitiveObstacle:
    shape: str
    mod: str = MOD_NONE
    offset_mod: str = OFF_NONE

    def to_obj(self) -> Dict[str, Any]:
        o = {"type": "primitive", "shape": self.shape}
        if self.mod != MOD_NONE:
            o["mod"] = self.mod
        if self.shape == SHAPE_STAR and self.offset_mod != OFF_NONE:
            o["offsetMod"] = self.offset_mod
        return o

    @staticmethod
    def from_obj(obj: Dict[str, Any]) -> "PrimitiveObstacle":
        shape = str(obj.get("shape", SHAPE_SQUARE))
        mod = str(obj.get("mod", MOD_NONE))

        if shape not in SHAPES and shape != SHAPE_STAR:
            shape = SHAPE_SQUARE

        if mod not in MODS:
            mod = MOD_NONE
        if shape not in MOD_SHAPES:
            mod = MOD_NONE

        return PrimitiveObstacle(shape=shape, mod=mod)

@dataclass(frozen=True)
class AnimGroupRef:
    group_index: int
    offset_mod: str = OFF_NONE

    def to_obj(self) -> Dict[str, Any]:
        return {
            "type": "anim_ref",
            "groupIndex": int(self.group_index),
            "offsetMod": self.offset_mod,
        }

    @staticmethod
    def from_obj(obj: Dict[str, Any]) -> "AnimGroupRef":
        gi = int(obj.get("groupIndex", 0))
        gi = max(0, min(MAX_ANIM_GROUPS - 1, gi))
        off = str(obj.get("offsetMod", OFF_NONE))
        if off not in OFFSET_MODS:
            off = OFF_NONE
        return AnimGroupRef(group_index=gi, offset_mod=off)

CellValue = Union[PrimitiveObstacle, AnimGroupRef]

@dataclass(frozen=True)
class AnimPrimitive:
    hx: int
    hy: int
    shape: str
    mod: str = MOD_NONE

    def to_obj(self) -> Dict[str, Any]:
        o = {
            "hx": int(self.hx),
            "hy": int(self.hy),
            "shape": self.shape,
        }
        if self.mod != MOD_NONE:
            o["mod"] = self.mod
        return o

    @staticmethod
    def from_obj(obj: Dict[str, Any]) -> "AnimPrimitive":
        shape = str(obj.get("shape", SHAPE_SQUARE))
        mod = str(obj.get("mod", MOD_NONE))
        if shape not in SHAPES:
            shape = SHAPE_SQUARE
        if mod not in MODS:
            mod = MOD_NONE
        if shape not in MOD_SHAPES:
            mod = MOD_NONE
        return AnimPrimitive(
            hx=int(obj.get("hx", 0)),
            hy=int(obj.get("hy", 0)),
            shape=shape,
            mod=mod,
        )

@dataclass(frozen=True)
class AnimStep:
    delta_qturns: int = 0
    target_scale_q7: int = 128
    duration_ms: int = 250

    def to_obj(self) -> Dict[str, Any]:
        return {
            "deltaQTurns": int(self.delta_qturns),
            "targetScaleQ7": int(self.target_scale_q7),
            "durationMs": int(self.duration_ms),
        }

    @staticmethod
    def from_obj(obj: Dict[str, Any]) -> "AnimStep":
        dq = int(obj.get("deltaQTurns", 0))
        scale = max(1, min(255, int(obj.get("targetScaleQ7", 128))))
        dur = max(1, int(obj.get("durationMs", 250)))
        return AnimStep(
            delta_qturns=dq,
            target_scale_q7=scale,
            duration_ms=dur,
        )

@dataclass
class AnimDef:
    name: str
    pivot_hx: int = 0
    pivot_hy: int = 0
    base_scale_q7: int = 128
    primitives: List[AnimPrimitive] = field(default_factory=list)
    steps: List[AnimStep] = field(default_factory=list)

    def to_obj(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "pivotHx": int(self.pivot_hx),
            "pivotHy": int(self.pivot_hy),
            "baseScaleQ7": int(self.base_scale_q7),
            "primitives": [p.to_obj() for p in self.primitives],
            "steps": [s.to_obj() for s in self.steps],
        }

    @staticmethod
    def from_obj(obj: Dict[str, Any]) -> "AnimDef":
        name = str(obj.get("name", "Unnamed"))
        pivx = int(obj.get("pivotHx", 0))
        pivy = int(obj.get("pivotHy", 0))
        base_scale = max(1, min(255, int(obj.get("baseScaleQ7", 128))))
        prims = [AnimPrimitive.from_obj(p) for p in obj.get("primitives", [])]
        steps = [AnimStep.from_obj(s) for s in obj.get("steps", [])]
        return AnimDef(
            name=name,
            pivot_hx=pivx,
            pivot_hy=pivy,
            base_scale_q7=base_scale,
            primitives=prims,
            steps=steps
        )

@dataclass
class Level:
    width: int
    height: int = GRID_H
    cells: Dict[str, CellValue] = None
    background_color_565: int = DEFAULT_BACKGROUND_COLOR_565
    obstacle_color_565: int = DEFAULT_OBSTACLE_COLOR_565
    anim_defs: List[Optional[AnimDef]] = None

    def __post_init__(self) -> None:
        if self.height != GRID_H:
            raise ValueError("Height must be 9.")
        if self.cells is None:
            self.cells = {}
        if self.anim_defs is None:
            self.anim_defs = [None] * MAX_ANIM_GROUPS
        elif len(self.anim_defs) != MAX_ANIM_GROUPS:
            raise ValueError("anim_defs must have length 8.")

    def key(self, x: int, y: int) -> str:
        return f"{x},{y}"

    def in_bounds(self, x: int, y: int) -> bool:
        return 0 <= x < self.width and 0 <= y < self.height

    def endcap_x0(self) -> int:
        return max(0, self.width - ENDCAP_W)

    def in_endcap(self, x: int) -> bool:
        return x >= self.endcap_x0()

    def resize_width(self, new_w: int) -> None:
        new_w = int(new_w)
        if new_w < 1:
            new_w = 1
        self.width = new_w

        kill: List[str] = []
        for k in self.cells.keys():
            xs, ys = k.split(",")
            x = int(xs)
            if x >= new_w:
                kill.append(k)
        for k in kill:
            self.cells.pop(k, None)

    def set_cell(self, x: int, y: int, value: Optional[CellValue]) -> None:
        if not self.in_bounds(x, y):
            return
        if self.in_endcap(x):
            return
        k = self.key(x, y)
        if value is None:
            self.cells.pop(k, None)
        else:
            self.cells[k] = value

    def get_cell(self, x: int, y: int) -> Optional[CellValue]:
        return self.cells.get(self.key(x, y))

    def get_primitive(self, x: int, y: int) -> Optional[PrimitiveObstacle]:
        c = self.get_cell(x, y)
        return c if isinstance(c, PrimitiveObstacle) else None

    def get_anim_ref(self, x: int, y: int) -> Optional[AnimGroupRef]:
        c = self.get_cell(x, y)
        return c if isinstance(c, AnimGroupRef) else None

    def effective_cell_at(self, x: int, y: int) -> Optional[CellValue]:
        cell = self.get_cell(x, y)
        if cell is not None:
            return cell

        last = self.width - 1
        if x >= self.endcap_x0():
            dx = x - last
            for e in ENDCAP_OBS:
                if int(e["dx"]) == dx and int(e["y"]) == y:
                    return PrimitiveObstacle.from_obj(e)
        return None

    def clear_cell(self, x: int, y: int) -> None:
        if not self.in_bounds(x, y):
            return
        if self.in_endcap(x):
            return
        self.cells.pop(self.key(x, y), None)

    def strip_endcap_from_authored(self) -> None:
        x0 = self.endcap_x0()
        kill = []
        for k in self.cells.keys():
            xs, _ = k.split(",")
            if int(xs) >= x0:
                kill.append(k)
        for k in kill:
            self.cells.pop(k, None)

    def clear_anim_group_refs(self, group_index: int) -> None:
        kill = []
        for k, v in self.cells.items():
            if isinstance(v, AnimGroupRef) and v.group_index == group_index:
                kill.append(k)
        for k in kill:
            self.cells.pop(k, None)

    def apply_endcap_to_export_list(self, out_list: List[Dict[str, Any]]) -> None:
        last = self.width - 1
        for e in ENDCAP_OBS:
            x = last + int(e["dx"])
            y = int(e["y"])
            if 0 <= x < self.width and 0 <= y < self.height:
                o = {"x": x, "y": y, "shape": e["shape"]}
                if "mod" in e:
                    o["mod"] = e["mod"]
                out_list.append(o)

    def portal_abs(self) -> Tuple[int, int]:
        last = self.width - 1
        return (last + int(PORTAL_REL["dx"]), int(PORTAL_REL["y"]))
    
    def count_authored_stars(self) -> int:
        count = 0
        for cell in self.cells.values():
            if isinstance(cell, PrimitiveObstacle) and cell.shape == SHAPE_STAR:
                count += 1
        return count

    def can_place_more_stars(self) -> bool:
        return self.count_authored_stars() < 3

    def used_anim_def_count(self) -> int:
        last_used = -1
        for i, a in enumerate(self.anim_defs):
            if a is not None:
                last_used = i
        return last_used + 1

    def anim_def_max_primitive_count(self) -> int:
        m = 0
        for a in self.anim_defs:
            if a is not None:
                m = max(m, len(a.primitives))
        return m

    def count_stars(self) -> int:
        count = 0
        for y in range(self.height):
            for x in range(self.width):
                cell = self.effective_cell_at(x, y)
                if isinstance(cell, PrimitiveObstacle) and cell.shape == SHAPE_STAR:
                    count += 1
        return count

    def write_bin(self, path: str) -> None:
        self.strip_endcap_from_authored()

        star_count = self.count_stars()
        if star_count != 3:
            raise ValueError(f"Level must contain exactly 3 stars; found {star_count}.")

        width = int(self.width)
        portal_dx_signed = int(PORTAL_REL["dx"])
        background_color_565 = int(self.background_color_565) & 0xFFFF
        obstacle_color_565 = int(self.obstacle_color_565) & 0xFFFF

        anim_def_count = self.used_anim_def_count()
        anim_def_max_primitive_count = self.anim_def_max_primitive_count()

        anim_defs_blob = bytearray()
        for i in range(anim_def_count):
            a = self.anim_defs[i]
            if a is None:
                prims: List[AnimPrimitive] = []
                steps: List[AnimStep] = []
                pivot_hx = 0
                pivot_hy = 0
            else:
                prims = a.primitives
                steps = a.steps
                pivot_hx = a.pivot_hx
                pivot_hy = a.pivot_hy

            total_duration = sum(max(1, s.duration_ms) for s in steps)
            if total_duration > 0xFFFF:
                total_duration = 0xFFFF

            base_scale_q7 = 128 if a is None else max(1, min(255, int(a.base_scale_q7)))

            anim_defs_blob += bytes([
                len(prims) & 0xFF,
                pivot_hx & 0xFF,
                pivot_hy & 0xFF,
                len(steps) & 0xFF,
                base_scale_q7 & 0xFF,
            ])
            anim_defs_blob += int(total_duration).to_bytes(2, "little", signed=False)

            for p in prims:
                shape_id = SHAPE_ID.get(p.shape, 1) & 0x0F
                mod_id = MOD_ID.get(p.mod, 0) & 0x03
                anim_defs_blob += int(p.hx).to_bytes(1, "little", signed=True)
                anim_defs_blob += int(p.hy).to_bytes(1, "little", signed=True)
                anim_defs_blob += bytes([shape_id, mod_id])

            for s in steps:
                dq = int(s.delta_qturns)
                scale_q7 = max(1, min(255, int(s.target_scale_q7)))
                dur = max(1, int(s.duration_ms))
                anim_defs_blob += int(dq).to_bytes(1, "little", signed=True)
                anim_defs_blob += bytes([scale_q7 & 0xFF])
                anim_defs_blob += int(dur).to_bytes(2, "little", signed=False)

        level_data_offset = 16 + len(anim_defs_blob)

        header = bytearray()
        header += LEVEL_MAGIC
        header += bytes([LEVEL_VERSION])
        header += width.to_bytes(2, "little", signed=False)
        header += int.to_bytes(portal_dx_signed, 1, "little", signed=True)
        header += b"\x00"
        header += background_color_565.to_bytes(2, "little", signed=False)
        header += obstacle_color_565.to_bytes(2, "little", signed=False)
        header += bytes([anim_def_count & 0xFF])
        header += bytes([anim_def_max_primitive_count & 0xFF])
        header += level_data_offset.to_bytes(2, "little", signed=False)

        if len(header) != 16:
            raise ValueError(f"Header length is {len(header)}, expected 16 bytes.")

        with open(path, "wb") as f:
            f.write(header)
            f.write(anim_defs_blob)

            for x in range(width):
                col = 0
                for y in range(9):
                    cell = self.effective_cell_at(x, y)
                    if cell is None:
                        obs_id = 0
                        mod_id = 0
                    elif isinstance(cell, PrimitiveObstacle):
                        obs_id = SHAPE_ID.get(cell.shape, 0) & 0x0F

                        if cell.shape == SHAPE_STAR:
                            mod_id = OFFSET_MOD_ID.get(cell.offset_mod, 0) & 0x03
                        else:
                            mod_id = MOD_ID.get(cell.mod, 0) & 0x03
                            if cell.shape not in MOD_SHAPES:
                                mod_id = 0
                    elif isinstance(cell, AnimGroupRef):
                        obs_id = (8 + int(cell.group_index)) & 0x0F
                        mod_id = OFFSET_MOD_ID.get(cell.offset_mod, 0) & 0x03
                    else:
                        obs_id = 0
                        mod_id = 0

                    cell6 = (obs_id & 0x0F) | ((mod_id & 0x03) << 4)
                    col |= (cell6 & 0x3F) << (y * 6)

                f.write(int(col).to_bytes(7, "little", signed=False))

    def to_json_obj(self) -> Dict[str, Any]:
        authored = []
        for k, cell in self.cells.items():
            xs, ys = k.split(",")
            authored.append({"x": int(xs), "y": int(ys), **cell.to_obj()})
        authored.sort(key=lambda o: (o["y"], o["x"], o.get("type", "")))

        out_obs = list(authored)
        self.apply_endcap_to_export_list(out_obs)
        out_obs.sort(key=lambda o: (o["y"], o["x"], o.get("type", ""), o.get("shape", "")))

        px, py = self.portal_abs()
        return {
            "width": self.width,
            "height": self.height,
            "backgroundColor565": self.background_color_565,
            "obstacleColor565": self.obstacle_color_565,
            "cells": authored,
            "obstacles": out_obs,
            "animations": [
                a.to_obj() if a is not None else None
                for a in self.anim_defs
            ],
            "portal": {"dx": int(PORTAL_REL["dx"]), "y": int(PORTAL_REL["y"]), "x": px},
            "endcap": {"width": ENDCAP_W},
        }

    @staticmethod
    def from_json_obj(obj: Dict[str, Any]) -> "Level":
        w = int(obj.get("width", DEFAULT_W))
        h = int(obj.get("height", GRID_H))
        if h != GRID_H:
            raise ValueError("Expected height=9.")

        bg = int(obj.get("backgroundColor565", DEFAULT_BACKGROUND_COLOR_565)) & 0xFFFF
        fg = int(obj.get("obstacleColor565", DEFAULT_OBSTACLE_COLOR_565)) & 0xFFFF

        anim_defs_raw = obj.get("animations", [None] * MAX_ANIM_GROUPS)
        anim_defs: List[Optional[AnimDef]] = [None] * MAX_ANIM_GROUPS
        for i in range(min(MAX_ANIM_GROUPS, len(anim_defs_raw))):
            if anim_defs_raw[i] is not None:
                anim_defs[i] = AnimDef.from_obj(anim_defs_raw[i])

        lvl = Level(
            width=w,
            height=h,
            background_color_565=bg,
            obstacle_color_565=fg,
            anim_defs=anim_defs
        )
        lvl.cells.clear()

        cells_src = obj.get("cells")
        if cells_src is None:
            cells_src = obj.get("obstacles", [])

        for o in cells_src:
            x = int(o.get("x", 0))
            y = int(o.get("y", 0))
            if not lvl.in_bounds(x, y):
                continue

            typ = str(o.get("type", "primitive"))
            if typ == "anim_ref":
                cell: CellValue = AnimGroupRef.from_obj(o)
            else:
                cell = PrimitiveObstacle.from_obj(o)

            lvl.cells[lvl.key(x, y)] = cell

        lvl.strip_endcap_from_authored()
        return lvl

# -----------------------------
# Undo system
# -----------------------------

@dataclass
class UndoAction:
    before_cells: Dict[str, Optional[CellValue]]
    after_cells: Dict[str, Optional[CellValue]]
    before_width: int
    after_width: int
    before_anim_defs: List[Optional[AnimDef]]
    after_anim_defs: List[Optional[AnimDef]]

# -----------------------------
# Naming dialog
# -----------------------------

class NameDialog:
    def __init__(self, parent: tk.Tk, title: str, prompt: str):
        self.result: Optional[str] = None

        self.top = tk.Toplevel(parent)
        self.top.title(title)
        self.top.transient(parent)
        self.top.grab_set()
        self.top.resizable(False, False)

        frm = tk.Frame(self.top, padx=12, pady=12)
        frm.pack(fill="both", expand=True)

        tk.Label(frm, text=prompt).pack(anchor="w")
        self.var = tk.StringVar()
        ent = tk.Entry(frm, textvariable=self.var, width=32)
        ent.pack(fill="x", pady=(8, 12))
        ent.focus_set()
        ent.selection_range(0, "end")

        btns = tk.Frame(frm)
        btns.pack(fill="x")

        tk.Button(btns, text="OK", width=10, command=self.on_ok).pack(side="left")
        tk.Button(btns, text="Cancel", width=10, command=self.on_cancel).pack(side="right")

        self.top.bind("<Escape>", lambda e: self.on_cancel())
        self.top.bind("<Return>", lambda e: self.on_ok())
        self.top.protocol("WM_DELETE_WINDOW", self.on_cancel)

        parent.wait_window(self.top)

    def on_ok(self) -> None:
        s = self.var.get().strip()
        if not s:
            return
        self.result = s
        self.top.destroy()

    def on_cancel(self) -> None:
        self.result = None
        self.top.destroy()

# -----------------------------
# Geometry helpers
# -----------------------------

def poly_centroid_area(poly: List[Tuple[float, float]]) -> Tuple[float, float, float]:
    area2 = 0.0
    cx = 0.0
    cy = 0.0
    n = len(poly)
    for i in range(n):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % n]
        cross = x0 * y1 - x1 * y0
        area2 += cross
        cx += (x0 + x1) * cross
        cy += (y0 + y1) * cross
    if abs(area2) < 1e-9:
        xs = sum(p[0] for p in poly) / max(1, n)
        ys = sum(p[1] for p in poly) / max(1, n)
        return xs, ys, 0.0
    area = area2 / 2.0
    cx /= (3.0 * area2)
    cy /= (3.0 * area2)
    return cx, cy, abs(area)

# -----------------------------
# Editor
# -----------------------------

class EditorApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root

        self.level = Level(DEFAULT_W)
        self.current_file_path: Optional[str] = None
        self.is_dirty = False
        self._history_revision = 0
        self._saved_revision = 0

        self.width_var = tk.IntVar(value=self.level.width)
        self.paint_mode_var = tk.StringVar(value=PAINT_MODE_PRIMITIVE)
        self.shape_var = tk.StringVar(value=SHAPE_SQUARE)
        self.mod_var = tk.StringVar(value=MOD_NONE)
        self.star_offset_var = tk.StringVar(value=OFF_NONE)
        self.selected_anim_slot: Optional[int] = None
        self.anim_offset_var = tk.StringVar(value=OFF_NONE)
        self.base_scale_var = tk.StringVar(value="1.00")

        self._painting = False
        self._erasing = False

        self._selecting = False
        self._sel_a: Optional[Tuple[int, int]] = None
        self._sel_b: Optional[Tuple[int, int]] = None
        self._sel_rect_id: Optional[int] = None

        self._clipboard: Optional[Dict[str, Any]] = None

        self._undo: List[UndoAction] = []
        self._redo: List[UndoAction] = []
        self._txn_active = False
        self._txn_before_cells: Dict[str, Optional[CellValue]] = {}
        self._txn_after_cells: Dict[str, Optional[CellValue]] = {}
        self._txn_before_width = self.level.width
        self._txn_before_anim_defs: List[Optional[AnimDef]] = [None] * MAX_ANIM_GROUPS

        self._last_mouse_cell: Tuple[int, int] = (0, 0)
        self._y_offset_px = 0

        self.anim_slot_labels: List[tk.Label] = []
        self.anim_select_buttons: List[tk.Button] = []
        self.anim_delete_buttons: List[tk.Button] = []

        self.preview_time_ms = 0
        self.preview_running = True
        self._preview_timer_ms = 33  # ~30 FPS

        self._build_ui()
        self._rebuild_canvas_region()
        self.redraw_all()
        self._update_modifier_enable()
        self._refresh_animation_ui()
        self.refresh_color_ui()
        self._schedule_preview_tick()
        self._update_window_title()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close_request)

    # -------- Window --------

    def on_close_request(self) -> None:
        if self.is_dirty:
            if not messagebox.askyesno("Unsaved Changes", "Discard unsaved changes and quit?"):
                return
        self.root.destroy()

    def mark_dirty(self) -> None:
        if not self.is_dirty:
            self.is_dirty = True
            self._update_window_title()

    def clear_dirty(self) -> None:
        if self.is_dirty:
            self.is_dirty = False
            self._update_window_title()

    def _display_name_for_title(self) -> str:
        if not self.current_file_path:
            return "Untitled"
        import os
        return os.path.basename(self.current_file_path)

    def _update_window_title(self) -> None:
        star = " *" if self.is_dirty else ""
        self.root.title(f"GV3D Obstacle Level Editor - {self._display_name_for_title()}{star}")

    # -------- UI --------

    def _build_ui(self) -> None:
        outer = tk.Frame(self.root)
        outer.pack(fill="both", expand=True)

        left_wrap = tk.Frame(outer)
        left_wrap.pack(side="left", fill="y")

        self.left_canvas = tk.Canvas(left_wrap, width=320, highlightthickness=0)
        self.left_vbar = tk.Scrollbar(left_wrap, orient="vertical", command=self.left_canvas.yview)
        self.left_canvas.configure(yscrollcommand=self.left_vbar.set)

        self.left_canvas.pack(side="left", fill="y")
        self.left_vbar.pack(side="right", fill="y")

        left = tk.Frame(self.left_canvas, padx=8, pady=8)
        self.left_canvas_window = self.left_canvas.create_window((0, 0), window=left, anchor="nw")

        right = tk.Frame(outer)
        right.pack(side="right", fill="both", expand=True)

        tk.Label(left, text="File", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        tk.Button(left, text="New (clear)…", command=self.new_level).pack(fill="x", pady=(4, 2))
        tk.Button(left, text="Open…", command=self.open_json).pack(fill="x", pady=2)
        tk.Button(left, text="Save", command=self.save_json).pack(fill="x", pady=2)
        tk.Button(left, text="Save As…", command=self.save_json_as).pack(fill="x", pady=2)
        tk.Button(left, text="Export BIN…", command=self.export_bin).pack(fill="x", pady=2)

        tk.Label(left, text="").pack(pady=6)

        tk.Label(left, text="Grid", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        wrow = tk.Frame(left)
        wrow.pack(fill="x", pady=(4, 8))
        tk.Label(wrow, text="Width:").pack(side="left")
        tk.Spinbox(wrow, from_=1, to=9999, width=6, textvariable=self.width_var,
                   command=self.on_width_change).pack(side="left", padx=(6, 0))
        tk.Button(wrow, text="Apply", command=self.on_width_change).pack(side="left", padx=6)

        tk.Label(left, text="").pack(pady=6)
        tk.Label(left, text="Colors", font=("Segoe UI", 10, "bold")).pack(anchor="w")

        bg_row = tk.Frame(left)
        bg_row.pack(fill="x", pady=(4, 2))
        tk.Label(bg_row, text="Background:").pack(side="left")
        self.bg_color_swatch = tk.Label(bg_row, width=3, relief="sunken")
        self.bg_color_swatch.pack(side="left", padx=(6, 4))
        self.bg_color_label = tk.Label(bg_row, text="0x0000", width=8, anchor="w")
        self.bg_color_label.pack(side="left")
        tk.Button(bg_row, text="Choose…", command=self.choose_background_color).pack(side="right")

        obs_row = tk.Frame(left)
        obs_row.pack(fill="x", pady=(2, 8))
        tk.Label(obs_row, text="Obstacles:").pack(side="left")
        self.obs_color_swatch = tk.Label(obs_row, width=3, relief="sunken")
        self.obs_color_swatch.pack(side="left", padx=(6, 4))
        self.obs_color_label = tk.Label(obs_row, text="0x07E0", width=8, anchor="w")
        self.obs_color_label.pack(side="left")
        tk.Button(obs_row, text="Choose…", command=self.choose_obstacle_color).pack(side="right")

        tk.Label(left, text="Mode", font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(6, 0))

        self.paint_primitives_rb = tk.Radiobutton(
            left,
            text="Paint primitives",
            variable=self.paint_mode_var,
            value=PAINT_MODE_PRIMITIVE,
            command=self._refresh_animation_ui
        )
        self.paint_primitives_rb.pack(anchor="w")

        self.paint_anim_refs_rb = tk.Radiobutton(
            left,
            text="Paint animation refs",
            variable=self.paint_mode_var,
            value=PAINT_MODE_ANIM_REF,
            command=self._refresh_animation_ui
        )
        self.paint_anim_refs_rb.pack(anchor="w")

        self.paint_stars_rb = tk.Radiobutton(
            left,
            text="Paint stars",
            variable=self.paint_mode_var,
            value=PAINT_MODE_STAR,
            command=self._refresh_animation_ui
        )
        self.paint_stars_rb.pack(anchor="w")

        tk.Label(left, text="").pack(pady=6)

        tk.Label(left, text="Obstacle Shape", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        shape_menu = tk.OptionMenu(left, self.shape_var, *SHAPES,
                                   command=lambda _=None: self._update_modifier_enable())
        shape_menu.pack(fill="x", pady=(4, 4))

        tk.Label(left, text="Modifier", font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(10, 0))
        self.mod_buttons = []
        for m in MODS:
            rb = tk.Radiobutton(left, text=m, variable=self.mod_var, value=m)
            rb.pack(anchor="w")
            self.mod_buttons.append(rb)

        tk.Label(left, text="Star Offset", font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(10, 0))
        for off, label in [
            (OFF_NONE, "None"),
            (OFF_RIGHT, "Shift right"),
            (OFF_DOWN, "Shift down"),
            (OFF_DOWN_RIGHT, "Shift down-right"),
        ]:
            tk.Radiobutton(left, text=label, variable=self.star_offset_var, value=off).pack(anchor="w")

        tk.Label(left, text="").pack(pady=6)

        tk.Label(left, text="Animations", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        self.new_anim_btn = tk.Button(left, text="New", command=self.new_animation_from_selection)
        self.new_anim_btn.pack(fill="x", pady=(4, 4))

        tk.Label(left, text="Anim Ref Offset", font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(4, 0))
        for off, label in [
            (OFF_NONE, "None"),
            (OFF_RIGHT, "Shift right"),
            (OFF_DOWN, "Shift down"),
            (OFF_DOWN_RIGHT, "Shift down-right"),
        ]:
            tk.Radiobutton(left, text=label, variable=self.anim_offset_var, value=off).pack(anchor="w")

        slot_frame = tk.Frame(left, bd=1, relief="sunken", padx=4, pady=4)
        slot_frame.pack(fill="x", pady=(6, 4))

        for i in range(MAX_ANIM_GROUPS):
            row = tk.Frame(slot_frame)
            row.pack(fill="x", pady=1)

            lbl = tk.Label(row, text=f"{i+1}: <empty>", anchor="w", width=18)
            lbl.pack(side="left")
            self.anim_slot_labels.append(lbl)

            sel = tk.Button(row, text="Select", width=7,
                            command=lambda idx=i: self.select_anim_slot(idx))
            sel.pack(side="left", padx=(4, 2))
            self.anim_select_buttons.append(sel)

            dele = tk.Button(row, text="Delete", width=7,
                             command=lambda idx=i: self.delete_anim_slot(idx))
            dele.pack(side="left")
            self.anim_delete_buttons.append(dele)

        tk.Label(left, text="").pack(pady=6)
        tk.Label(left, text="Animation Steps", font=("Segoe UI", 10, "bold")).pack(anchor="w")

        self.anim_name_var = tk.StringVar(value="")
        self.anim_name_entry = tk.Entry(left, textvariable=self.anim_name_var)
        self.anim_name_entry.pack(fill="x", pady=(4, 4))
        self.anim_name_entry.bind("<FocusOut>", lambda e: self.apply_anim_name())

        pivot_row = tk.Frame(left)
        pivot_row.pack(fill="x", pady=(2, 4))
        tk.Label(pivot_row, text="Pivot hx:").pack(side="left")
        self.pivot_hx_var = tk.IntVar(value=0)
        tk.Spinbox(pivot_row, from_=-32, to=32, width=5, textvariable=self.pivot_hx_var,
                command=self.apply_anim_pivot).pack(side="left", padx=(4, 8))
        tk.Label(pivot_row, text="hy:").pack(side="left")
        self.pivot_hy_var = tk.IntVar(value=0)
        tk.Spinbox(pivot_row, from_=-32, to=32, width=5, textvariable=self.pivot_hy_var,
                command=self.apply_anim_pivot).pack(side="left", padx=(4, 0))

        base_row = tk.Frame(left)
        base_row.pack(fill="x", pady=(2, 4))
        tk.Label(base_row, text="Base scale:").pack(side="left")
        self.base_scale_entry = tk.Entry(base_row, width=6, textvariable=self.base_scale_var)
        self.base_scale_entry.pack(side="left", padx=(6, 0))
        tk.Button(base_row, text="Update", command=self.apply_anim_base_scale).pack(side="left", padx=(6, 0))

        self.step_listbox = tk.Listbox(left, height=8, exportselection=False)
        self.step_listbox.pack(fill="x", pady=(4, 4))

        step_edit = tk.Frame(left)
        step_edit.pack(fill="x", pady=(2, 4))

        tk.Label(step_edit, text="Δ qturns:").grid(row=0, column=0, sticky="w")
        self.step_delta_var = tk.IntVar(value=0)
        self.step_delta_spin = tk.Spinbox(step_edit, from_=-8, to=8, width=5, textvariable=self.step_delta_var)
        self.step_delta_spin.grid(row=0, column=1, sticky="w", padx=(4, 8))

        tk.Label(step_edit, text="Target Scale:").grid(row=0, column=2, sticky="w")
        self.step_scale_var = tk.StringVar(value="1.00")
        self.step_scale_entry = tk.Entry(step_edit, width=6, textvariable=self.step_scale_var)
        self.step_scale_entry.grid(row=0, column=3, sticky="w", padx=(4, 8))

        tk.Label(step_edit, text="Duration ms:").grid(row=1, column=0, sticky="w")
        self.step_duration_var = tk.IntVar(value=250)
        self.step_duration_spin = tk.Spinbox(step_edit, from_=1, to=10000, width=8, textvariable=self.step_duration_var)
        self.step_duration_spin.grid(row=1, column=1, sticky="w", padx=(4, 8))

        tk.Button(step_edit, text="Update Step", command=self.apply_selected_anim_step).grid(row=1, column=2, columnspan=2, sticky="ew")

        step_btns = tk.Frame(left)
        step_btns.pack(fill="x", pady=(2, 4))
        tk.Button(step_btns, text="Add Step", command=self.add_blank_anim_step).pack(fill="x", pady=1)
        tk.Button(step_btns, text="Delete Step", command=self.delete_selected_anim_step).pack(fill="x", pady=1)

        step_btns2 = tk.Frame(left)
        step_btns2.pack(fill="x", pady=(0, 4))
        tk.Button(step_btns2, text="Move Up", command=lambda: self.move_anim_step(-1)).pack(fill="x", pady=1)
        tk.Button(step_btns2, text="Move Down", command=lambda: self.move_anim_step(1)).pack(fill="x", pady=1)
        tk.Button(step_btns2, text="Duplicate Step", command=self.duplicate_selected_anim_step).pack(fill="x", pady=1)

        preview_row = tk.Frame(left)
        preview_row.pack(fill="x", pady=(4, 0))
        self.preview_toggle_btn = tk.Button(preview_row, text="Pause Preview", command=self.toggle_preview)
        self.preview_toggle_btn.pack(side="left")
        tk.Button(preview_row, text="Reset Preview", command=self.reset_preview).pack(side="left", padx=(6, 0))

        tk.Label(left, text="").pack(pady=6)
        tk.Label(left, text="Shortcuts", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        tk.Label(left,
                 text="• Shift+Drag: select rect\n"
                      "• Ctrl+C: copy\n"
                      "• Ctrl+X: cut\n"
                      "• Ctrl+V: paste at mouse\n"
                      "• Ctrl+Z: undo\n"
                      "• Ctrl+Y: redo\n"
                      "• Right-drag: erase\n"
                      "• Mouse wheel: scroll horizontally\n"
                      "• Endcap zone (last 6 cols) is locked",
                 justify="left", fg="#444").pack(anchor="w")

        def _on_left_frame_configure(event):
            self.left_canvas.configure(scrollregion=self.left_canvas.bbox("all"))

        def _on_left_canvas_configure(event):
            self.left_canvas.itemconfigure(self.left_canvas_window, width=event.width)

        left.bind("<Configure>", _on_left_frame_configure)
        self.left_canvas.bind("<Configure>", _on_left_canvas_configure)

        self.left_canvas.bind("<MouseWheel>", self.on_left_panel_wheel)
        self.left_canvas.bind("<Button-4>", lambda e: self.left_canvas.yview_scroll(-3, "units"))
        self.left_canvas.bind("<Button-5>", lambda e: self.left_canvas.yview_scroll(3, "units"))
        left.bind("<MouseWheel>", self.on_left_panel_wheel)
        left.bind("<Button-4>", lambda e: self.left_canvas.yview_scroll(-3, "units"))
        left.bind("<Button-5>", lambda e: self.left_canvas.yview_scroll(3, "units"))

        self.canvas = tk.Canvas(right, bg="#101010", highlightthickness=0)
        self.hbar = tk.Scrollbar(right, orient="horizontal", command=self.canvas.xview)
        self.canvas.configure(xscrollcommand=self.hbar.set)

        self.canvas.pack(side="top", fill="both", expand=True)
        self.hbar.pack(side="bottom", fill="x")

        self.status = tk.Label(right, text="x=?, y=?", anchor="w", fg="#ddd", bg="#202020")
        self.status.pack(side="bottom", fill="x")

        self.canvas.bind("<Configure>", self.on_canvas_resize)
        self.canvas.bind("<Motion>", self.on_motion)

        self.canvas.bind("<ButtonPress-1>", self.on_left_down)
        self.canvas.bind("<B1-Motion>", self.on_left_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_left_up)

        self.canvas.bind("<ButtonPress-3>", self.on_right_down)
        self.canvas.bind("<B3-Motion>", self.on_right_drag)
        self.canvas.bind("<ButtonRelease-3>", self.on_right_up)

        self.canvas.bind("<MouseWheel>", self.on_wheel)
        self.canvas.bind("<Button-4>", lambda e: self.canvas.xview_scroll(-3, "units"))
        self.canvas.bind("<Button-5>", lambda e: self.canvas.xview_scroll(3, "units"))

        self.step_listbox.bind("<<ListboxSelect>>", lambda e: self.refresh_selected_step_editor())

        self.root.bind_all("<Control-c>", lambda e: self.copy_selection())
        self.root.bind_all("<Control-x>", lambda e: self.cut_selection())
        self.root.bind_all("<Control-v>", lambda e: self.paste_clipboard())
        self.root.bind_all("<Control-z>", lambda e: self.undo())
        self.root.bind_all("<Control-y>", lambda e: self.redo())
        self.root.bind_all("<Escape>", lambda e: self.clear_selection())

        self.root.bind_all("<Control-o>", lambda e: self.open_json())
        self.root.bind_all("<Control-s>", lambda e: self.save_json())
        self.root.bind_all("<Control-Shift-S>", lambda e: self.save_json_as())

    def current_anim_def(self) -> Optional[AnimDef]:
        if self.selected_anim_slot is None:
            return None
        return self.level.anim_defs[self.selected_anim_slot]

    def current_anim_step_index(self) -> Optional[int]:
        sel = self.step_listbox.curselection()
        if not sel:
            return None
        return int(sel[0])

    def q7_to_scale_text(self, q7: int) -> str:
        return f"{q7 / 128.0:.2f}"

    def scale_text_to_q7(self, s: str) -> int:
        try:
            v = float(s.strip())
        except Exception:
            v = 1.0
        if v < (1.0 / 128.0):
            v = 1.0 / 128.0
        if v > (255.0 / 128.0):
            v = 255.0 / 128.0
        q7 = int(round(v * 128.0))
        return max(1, min(255, q7))

    def refresh_selected_step_editor(self) -> None:
        anim = self.current_anim_def()
        idx = self.current_anim_step_index()

        if anim is None:
            self.base_scale_var.set("1.00")
        else:
            self.base_scale_var.set(self.q7_to_scale_text(anim.base_scale_q7))

        if anim is None or idx is None or idx < 0 or idx >= len(anim.steps):
            self.step_delta_var.set(0)
            self.step_scale_var.set("1.00")
            self.step_duration_var.set(250)
            return

        step = anim.steps[idx]
        self.step_delta_var.set(step.delta_qturns)
        self.step_scale_var.set(self.q7_to_scale_text(step.target_scale_q7))
        self.step_duration_var.set(step.duration_ms)

    def apply_selected_anim_step(self) -> None:
        anim = self.current_anim_def()
        idx = self.current_anim_step_index()
        if anim is None or idx is None or idx < 0 or idx >= len(anim.steps):
            return

        dq = int(self.step_delta_var.get())
        scale_q7 = self.scale_text_to_q7(self.step_scale_var.get())
        dur = max(1, int(self.step_duration_var.get()))

        self.begin_txn()
        self.touch_all_anim_defs_before()
        anim.steps[idx] = AnimStep(
            delta_qturns=dq,
            target_scale_q7=scale_q7,
            duration_ms=dur
        )
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.refresh_anim_editor_panel()
        self.step_listbox.selection_set(idx)
        self.refresh_selected_step_editor()
        self.redraw_all()

    def move_anim_step(self, delta: int) -> None:
        anim = self.current_anim_def()
        idx = self.current_anim_step_index()
        if anim is None or idx is None:
            return

        j = idx + delta
        if j < 0 or j >= len(anim.steps):
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        anim.steps[idx], anim.steps[j] = anim.steps[j], anim.steps[idx]
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.refresh_anim_editor_panel()
        self.step_listbox.selection_set(j)
        self.refresh_selected_step_editor()
        self.redraw_all()

    def duplicate_selected_anim_step(self) -> None:
        anim = self.current_anim_def()
        idx = self.current_anim_step_index()
        if anim is None or idx is None or idx < 0 or idx >= len(anim.steps):
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        step = anim.steps[idx]
        anim.steps.insert(
            idx + 1,
            AnimStep(
                delta_qturns=step.delta_qturns,
                target_scale_q7=step.target_scale_q7,
                duration_ms=step.duration_ms
            )
        )
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.refresh_anim_editor_panel()
        self.step_listbox.selection_set(idx + 1)
        self.refresh_selected_step_editor()
        self.redraw_all()

    def refresh_anim_editor_panel(self) -> None:
        anim = self.current_anim_def()

        self.step_listbox.delete(0, "end")

        if anim is None:
            self.anim_name_var.set("")
            self.pivot_hx_var.set(0)
            self.pivot_hy_var.set(0)
            self.anim_name_entry.configure(state="disabled")
            return

        self.anim_name_entry.configure(state="normal")
        self.anim_name_var.set(anim.name)
        self.pivot_hx_var.set(anim.pivot_hx)
        self.pivot_hy_var.set(anim.pivot_hy)

        for i, step in enumerate(anim.steps):
            label = (
                f"{i+1}. Δrot {step.delta_qturns:+d} qturns, "
                f"scale {step.target_scale_q7/128.0:.2f}, "
                f"{step.duration_ms} ms"
            )
            self.step_listbox.insert("end", label)

        if anim is not None and anim.steps and not self.step_listbox.curselection():
            self.step_listbox.selection_set(0)
        self.refresh_selected_step_editor()

    def apply_anim_name(self) -> None:
        anim = self.current_anim_def()
        if anim is None:
            return
        name = self.anim_name_var.get().strip()
        if not name:
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        anim.name = name
        self.touch_all_anim_defs_after()
        self.commit_txn()
        self._refresh_animation_ui()

    def apply_anim_pivot(self) -> None:
        anim = self.current_anim_def()
        if anim is None:
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        anim.pivot_hx = int(self.pivot_hx_var.get())
        anim.pivot_hy = int(self.pivot_hy_var.get())
        self.touch_all_anim_defs_after()
        self.commit_txn()
        self.redraw_all()

    def apply_anim_base_scale(self) -> None:
        anim = self.current_anim_def()
        if anim is None:
            return

        q7 = self.scale_text_to_q7(self.base_scale_var.get())

        self.begin_txn()
        self.touch_all_anim_defs_before()
        anim.base_scale_q7 = q7
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.refresh_anim_editor_panel()
        self.redraw_all()

    def add_blank_anim_step(self) -> None:
        anim = self.current_anim_def()
        if anim is None:
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        anim.steps.append(AnimStep(delta_qturns=0, target_scale_q7=128, duration_ms=250))
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.refresh_anim_editor_panel()
        self.step_listbox.selection_clear(0, "end")
        self.step_listbox.selection_set(len(anim.steps) - 1)
        self.refresh_selected_step_editor()
        self.redraw_all()

    def add_anim_step(self, delta_qturns: int, target_scale_q7: int, duration_ms: int) -> None:
        anim = self.current_anim_def()
        if anim is None:
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        anim.steps.append(
            AnimStep(
                delta_qturns=delta_qturns,
                target_scale_q7=max(1, min(255, int(target_scale_q7))),
                duration_ms=duration_ms
            )
        )
        self.touch_all_anim_defs_after()
        self.commit_txn()
        self.refresh_anim_editor_panel()
        self.step_listbox.selection_clear(0, "end")
        self.step_listbox.selection_set(len(anim.steps) - 1)
        self.refresh_selected_step_editor()
        self.redraw_all()

    def delete_selected_anim_step(self) -> None:
        anim = self.current_anim_def()
        if anim is None:
            return

        sel = self.step_listbox.curselection()
        if not sel:
            return
        idx = int(sel[0])
        if idx < 0 or idx >= len(anim.steps):
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        del anim.steps[idx]
        self.touch_all_anim_defs_after()
        self.commit_txn()
        self.refresh_anim_editor_panel()
        if anim.steps:
            self.step_listbox.selection_set(min(idx, len(anim.steps) - 1))
        self.refresh_selected_step_editor()
        self.redraw_all()

    def toggle_preview(self) -> None:
        self.preview_running = not self.preview_running
        self.preview_toggle_btn.configure(text=("Pause Preview" if self.preview_running else "Resume Preview"))

    def reset_preview(self) -> None:
        self.preview_time_ms = 0
        self.redraw_all()

    def _schedule_preview_tick(self) -> None:
        self.root.after(self._preview_timer_ms, self._preview_tick)

    def _preview_tick(self) -> None:
        if self.preview_running:
            self.preview_time_ms += self._preview_timer_ms
            self.redraw_all()
        self._schedule_preview_tick()

    def eval_anim_state(self, anim: AnimDef) -> Tuple[float, float]:
        if anim is None:
            return 0.0, 1.0

        base_scale = anim.base_scale_q7 / 128.0

        if not anim.steps:
            return 0.0, base_scale

        total = sum(max(1, s.duration_ms) for s in anim.steps)
        if total <= 0:
            return 0.0, base_scale

        t = self.preview_time_ms % total
        angle = 0.0
        scale = base_scale
        walked = 0

        for step in anim.steps:
            dur = max(1, step.duration_ms)
            target_scale = step.target_scale_q7 / 128.0

            if t < walked + dur:
                u = (t - walked) / dur
                angle += 0.25 * step.delta_qturns * u
                scale = scale + (target_scale - scale) * u
                break

            angle += 0.25 * step.delta_qturns
            scale = target_scale
            walked += dur

        return angle, scale

    def eval_anim_angle_turns(self, anim: AnimDef) -> float:
        angle, _ = self.eval_anim_state(anim)
        return angle

    def clear_selection(self) -> None:
        self._sel_a = None
        self._sel_b = None
        self._selecting = False
        self._draw_selection_overlay()
        self._refresh_animation_ui()

    def _update_modifier_enable(self) -> None:
        shape = self.shape_var.get()
        enabled = shape in MOD_SHAPES
        for rb in self.mod_buttons:
            val = rb["value"]
            if val == MOD_NONE:
                rb.configure(state="normal")
            else:
                rb.configure(state=("normal" if enabled else "disabled"))
        if not enabled:
            self.mod_var.set(MOD_NONE)

    def _has_selected_primitives(self) -> bool:
        b = self._sel_bounds()
        if not b:
            return False
        x0, y0, x1, y1 = b
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                cell = self.level.get_cell(x, y)
                if isinstance(cell, PrimitiveObstacle) and cell.shape != SHAPE_STAR:
                    return True
        return False

    def _first_free_anim_slot(self) -> Optional[int]:
        for i, a in enumerate(self.level.anim_defs):
            if a is None:
                return i
        return None

    def _refresh_animation_ui(self) -> None:
        free = self._first_free_anim_slot()
        can_new = free is not None and self._has_selected_primitives()
        self.new_anim_btn.configure(state=("normal" if can_new else "disabled"))

        star_count = self.level.count_authored_stars()
        can_paint_stars = star_count < 3

        self.paint_stars_rb.configure(
            text=f"Paint stars ({star_count}/3)",
            state=("normal" if can_paint_stars else "disabled")
        )

        if not can_paint_stars and self.paint_mode_var.get() == PAINT_MODE_STAR:
            self.paint_mode_var.set(PAINT_MODE_PRIMITIVE)

        for i in range(MAX_ANIM_GROUPS):
            a = self.level.anim_defs[i]
            text = f"{i+1}: <empty>" if a is None else f"{i+1}: {a.name}"
            self.anim_slot_labels[i].configure(text=text)

            self.anim_select_buttons[i].configure(state=("normal" if a is not None else "disabled"))
            self.anim_delete_buttons[i].configure(state=("normal" if a is not None else "disabled"))

            if self.selected_anim_slot == i and a is not None:
                self.anim_slot_labels[i].configure(fg="#ffd54a")
            else:
                self.anim_slot_labels[i].configure(fg="#000000")

        self.refresh_anim_editor_panel()

    def select_anim_slot(self, idx: int) -> None:
        if self.level.anim_defs[idx] is None:
            return
        self.selected_anim_slot = idx
        self.paint_mode_var.set(PAINT_MODE_ANIM_REF)
        self._refresh_animation_ui()
        self.refresh_anim_editor_panel()
        self.redraw_all()

    def delete_anim_slot(self, idx: int) -> None:
        a = self.level.anim_defs[idx]
        if a is None:
            return

        if not messagebox.askyesno(
            "Delete animation",
            f'Delete animation group "{a.name}"?\n\nThis will also remove all references to it from the level.'
        ):
            return

        self.begin_txn()
        self.touch_all_anim_defs_before()
        self.touch_all_cells_for_anim_group_before(idx)

        self.level.anim_defs[idx] = None
        self.level.clear_anim_group_refs(idx)

        self.touch_all_anim_defs_after()
        self.touch_all_cells_for_anim_group_after(idx)
        self.commit_txn()

        if self.selected_anim_slot == idx:
            self.selected_anim_slot = None

        self.redraw_all()
        self._refresh_animation_ui()

    # -------- View / layout --------

    def on_canvas_resize(self, e: tk.Event) -> None:
        grid_h_px = GRID_H * CELL
        self._y_offset_px = max(0, (e.height - grid_h_px) // 2)
        self.redraw_all()

    def _rebuild_canvas_region(self) -> None:
        wpx = self.level.width * CELL
        hpx = max(GRID_H * CELL + 2 * self._y_offset_px, GRID_H * CELL)
        self.canvas.configure(scrollregion=(0, 0, wpx, hpx))

    # -------- Coords --------

    def event_to_cell(self, e: tk.Event) -> Optional[Tuple[int, int]]:
        cx = self.canvas.canvasx(e.x)
        cy = self.canvas.canvasy(e.y) - self._y_offset_px
        x = int(cx // CELL)
        y = int(cy // CELL)
        if 0 <= x < self.level.width and 0 <= y < self.level.height:
            return x, y
        return None

    def on_motion(self, e: tk.Event) -> None:
        c = self.event_to_cell(e)
        if c:
            self._last_mouse_cell = c
            self.status.configure(text=f"x={c[0]}, y={c[1]}")

    # -------- Width --------

    def on_width_change(self) -> None:
        try:
            w = int(self.width_var.get())
        except Exception:
            return

        self.begin_txn()
        old_w = self.level.width

        self.level.resize_width(w)
        self.level.strip_endcap_from_authored()

        self.commit_txn()

        if self.level.width != old_w:
            self.width_var.set(self.level.width)
            self._rebuild_canvas_region()
            self.redraw_all()
            self._refresh_animation_ui()

    # -------- Undo plumbing --------

    def refresh_dirty_from_history(self) -> None:
        self.is_dirty = (self._history_revision != self._saved_revision)
        self._update_window_title()

    def reset_undo_state(self) -> None:
        self._undo.clear()
        self._redo.clear()
        self._txn_active = False
        self._txn_before_cells.clear()
        self._txn_after_cells.clear()
        self._txn_before_width = self.level.width
        self._txn_before_anim_defs = list(self.level.anim_defs)

    def begin_txn(self) -> None:
        if self._txn_active:
            return
        self._txn_active = True
        self._txn_before_cells.clear()
        self._txn_after_cells.clear()
        self._txn_before_width = self.level.width
        self._txn_before_anim_defs = list(self.level.anim_defs)

    def touch_cell_before(self, x: int, y: int) -> None:
        k = self.level.key(x, y)
        if k in self._txn_before_cells:
            return
        self._txn_before_cells[k] = self.level.get_cell(x, y)

    def touch_cell_after(self, x: int, y: int) -> None:
        k = self.level.key(x, y)
        self._txn_after_cells[k] = self.level.get_cell(x, y)

    def touch_all_anim_defs_before(self) -> None:
        self._txn_before_anim_defs = list(self.level.anim_defs)

    def touch_all_anim_defs_after(self) -> None:
        pass

    def touch_all_cells_for_anim_group_before(self, idx: int) -> None:
        for y in range(self.level.height):
            for x in range(self.level.width):
                c = self.level.get_cell(x, y)
                if isinstance(c, AnimGroupRef) and c.group_index == idx:
                    self.touch_cell_before(x, y)

    def touch_all_cells_for_anim_group_after(self, idx: int) -> None:
        for y in range(self.level.height):
            for x in range(self.level.width):
                c = self.level.get_cell(x, y)
                if isinstance(c, AnimGroupRef) and c.group_index == idx:
                    self.touch_cell_after(x, y)

    def commit_txn(self) -> None:
        if not self._txn_active:
            return

        for k in list(self._txn_before_cells.keys()):
            if k not in self._txn_after_cells:
                xs, ys = k.split(",")
                self._txn_after_cells[k] = self.level.get_cell(int(xs), int(ys))

        act = UndoAction(
            before_cells=dict(self._txn_before_cells),
            after_cells=dict(self._txn_after_cells),
            before_width=self._txn_before_width,
            after_width=self.level.width,
            before_anim_defs=list(self._txn_before_anim_defs),
            after_anim_defs=list(self.level.anim_defs),
        )

        if (act.before_cells == act.after_cells and
            act.before_width == act.after_width and
            act.before_anim_defs == act.after_anim_defs):
            self._txn_active = False
            return

        self._undo.append(act)
        self._redo.clear()
        self._txn_active = False
        self._history_revision += 1
        self.refresh_dirty_from_history()

    def undo(self) -> None:
        if not self._undo:
            return

        act = self._undo.pop()
        self._redo.append(act)
        self._history_revision -= 1
        self.refresh_dirty_from_history()

        self.level.resize_width(act.before_width)

        for k, prev in act.before_cells.items():
            xs, ys = k.split(",")
            x, y = int(xs), int(ys)

            if not self.level.in_bounds(x, y):
                continue

            if prev is None:
                self.level.cells.pop(k, None)
            else:
                self.level.cells[k] = prev

        self.level.anim_defs = list(act.before_anim_defs)

        self.level.strip_endcap_from_authored()

        self.width_var.set(self.level.width)
        self._rebuild_canvas_region()
        self.redraw_all()
        self._refresh_animation_ui()

    def redo(self) -> None:
        if not self._redo:
            return

        act = self._redo.pop()
        self._undo.append(act)
        self._history_revision += 1
        self.refresh_dirty_from_history()

        self.level.resize_width(act.after_width)

        for k, after in act.after_cells.items():
            xs, ys = k.split(",")
            x, y = int(xs), int(ys)

            if not self.level.in_bounds(x, y):
                continue

            if after is None:
                self.level.cells.pop(k, None)
            else:
                self.level.cells[k] = after

        self.level.anim_defs = list(act.after_anim_defs)

        self.level.strip_endcap_from_authored()

        self.width_var.set(self.level.width)
        self._rebuild_canvas_region()
        self.redraw_all()
        self._refresh_animation_ui()

    # -------- Selection -> AnimDef conversion --------

    def _sel_bounds(self) -> Optional[Tuple[int, int, int, int]]:
        if not self._sel_a or not self._sel_b:
            return None
        ax, ay = self._sel_a
        bx, by = self._sel_b
        x0, x1 = (ax, bx) if ax <= bx else (bx, ax)
        y0, y1 = (ay, by) if ay <= by else (by, ay)
        return x0, y0, x1, y1

    def _base_polygon(self, shape: str):
        if shape == SHAPE_SQUARE:
            return [(0.12, 0.12), (0.88, 0.12), (0.88, 0.88), (0.12, 0.88)]
        if shape == SHAPE_RIGHT_TRI:
            return [(0.08, 0.92), (0.92, 0.92), (0.92, 0.08)]
        if shape == SHAPE_HALF_SPIKE:
            return [(0.18, 0.88), (0.82, 0.88), (0.50, 0.45)]
        if shape == SHAPE_FULL_SPIKE:
            return [(0.12, 0.90), (0.88, 0.90), (0.50, 0.10)]
        return [(0.12, 0.12), (0.88, 0.12), (0.88, 0.88), (0.12, 0.88)]

    def _apply_mod(self, p, mod: str):
        u, v = p
        if mod == MOD_NONE:
            return (u, v)
        if mod == MOD_ROT_L:
            return (v, 1.0 - u)
        if mod == MOD_ROT_R:
            return (1.0 - v, u)
        if mod == MOD_INVERT:
            return (1.0 - u, 1.0 - v)
        return (u, v)

    def _compute_weighted_origin_from_selection(self, cells: List[Tuple[int, int, PrimitiveObstacle]]) -> Tuple[float, float]:
        total_area = 0.0
        sum_cx = 0.0
        sum_cy = 0.0

        for x, y, obs in cells:
            poly = self._base_polygon(obs.shape)
            poly = [self._apply_mod(p, obs.mod if obs.shape in MOD_SHAPES else MOD_NONE) for p in poly]
            world_poly = [(x + u, y + v) for (u, v) in poly]
            cx, cy, area = poly_centroid_area(world_poly)
            if area <= 1e-9:
                cx = x + 0.5
                cy = y + 0.5
                area = 1.0
            total_area += area
            sum_cx += cx * area
            sum_cy += cy * area

        if total_area <= 1e-9:
            return 0.5, 0.5

        ox = sum_cx / total_area
        oy = sum_cy / total_area

        ox = round(ox * 2.0) / 2.0
        oy = round(oy * 2.0) / 2.0
        return ox, oy

    def _derive_owning_cell_and_offset(self, origin_x: float, origin_y: float) -> Tuple[int, int, str]:
        fxp = origin_x - math.floor(origin_x)
        fyp = origin_y - math.floor(origin_y)

        if abs(fxp - 0.5) < 1e-6:
            cell_x = math.floor(origin_x)
            shift_x = False
        else:
            cell_x = int(round(origin_x)) - 1
            shift_x = True

        if abs(fyp - 0.5) < 1e-6:
            cell_y = math.floor(origin_y)
            shift_y = False
        else:
            cell_y = int(round(origin_y)) - 1
            shift_y = True

        if shift_x and shift_y:
            off = OFF_DOWN_RIGHT
        elif shift_x:
            off = OFF_RIGHT
        elif shift_y:
            off = OFF_DOWN
        else:
            off = OFF_NONE

        return int(cell_x), int(cell_y), off

    def _build_anim_def_from_selection(self, name: str) -> Optional[Tuple[int, int, str, AnimDef, List[Tuple[int, int]]]]:
        b = self._sel_bounds()
        if not b:
            return None

        x0, y0, x1, y1 = b
        selected_prims: List[Tuple[int, int, PrimitiveObstacle]] = []
        selected_cells: List[Tuple[int, int]] = []

        x1 = min(x1, self.level.endcap_x0() - 1) if self.level.endcap_x0() > 0 else x1
        if x1 < x0:
            return None

        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                cell = self.level.get_cell(x, y)
                if isinstance(cell, PrimitiveObstacle) and cell.shape != SHAPE_STAR:
                    selected_prims.append((x, y, cell))
                    selected_cells.append((x, y))

        if not selected_prims:
            return None

        origin_x, origin_y = self._compute_weighted_origin_from_selection(selected_prims)
        owning_x, owning_y, offset_mod = self._derive_owning_cell_and_offset(origin_x, origin_y)

        if not self.level.in_bounds(owning_x, owning_y):
            return None
        if self.level.in_endcap(owning_x):
            return None

        primitives: List[AnimPrimitive] = []
        for x, y, obs in selected_prims:
            pcx = x + 0.5
            pcy = y + 0.5
            hx = int(round((pcx - origin_x) * 2.0))
            hy = int(round((pcy - origin_y) * 2.0))
            primitives.append(AnimPrimitive(hx=hx, hy=hy, shape=obs.shape, mod=obs.mod))

        anim_def = AnimDef(
            name=name,
            pivot_hx=0,
            pivot_hy=0,
            base_scale_q7=128,
            primitives=primitives,
            steps=[]
        )

        return owning_x, owning_y, offset_mod, anim_def, selected_cells

    def new_animation_from_selection(self) -> None:
        free = self._first_free_anim_slot()
        if free is None:
            return

        if not self._has_selected_primitives():
            messagebox.showinfo("New animation", "Select one or more primitive shapes first.")
            return

        dlg = NameDialog(self.root, "New Animation Group", "Enter animation group name:")
        if dlg.result is None:
            return

        built = self._build_anim_def_from_selection(dlg.result)
        if built is None:
            messagebox.showerror("New animation", "Could not create animation group from the current selection.")
            return

        owning_x, owning_y, offset_mod, anim_def, selected_cells = built

        self.begin_txn()
        self.touch_all_anim_defs_before()

        for (x, y) in selected_cells:
            self.touch_cell_before(x, y)

        self.touch_cell_before(owning_x, owning_y)

        for (x, y) in selected_cells:
            self.level.clear_cell(x, y)

        self.level.anim_defs[free] = anim_def

        if not anim_def.steps:
            anim_def.steps = [
                AnimStep(delta_qturns=1, target_scale_q7=128, duration_ms=250),
                AnimStep(delta_qturns=0, target_scale_q7=128, duration_ms=250),
                AnimStep(delta_qturns=1, target_scale_q7=128, duration_ms=250),
                AnimStep(delta_qturns=0, target_scale_q7=128, duration_ms=250),
                AnimStep(delta_qturns=1, target_scale_q7=128, duration_ms=250),
                AnimStep(delta_qturns=0, target_scale_q7=128, duration_ms=250),
                AnimStep(delta_qturns=-3, target_scale_q7=128, duration_ms=250),
                AnimStep(delta_qturns=0, target_scale_q7=128, duration_ms=500),
            ]

        self.level.set_cell(owning_x, owning_y, AnimGroupRef(group_index=free, offset_mod=offset_mod))

        for (x, y) in selected_cells:
            self.touch_cell_after(x, y)
        self.touch_cell_after(owning_x, owning_y)
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.selected_anim_slot = free
        self.paint_mode_var.set(PAINT_MODE_ANIM_REF)
        self.redraw_all()
        self._refresh_animation_ui()
        self.clear_selection()

    # -------- Drawing --------

    def redraw_all(self) -> None:
        self.canvas.delete("all")
        self._rebuild_canvas_region()
        self.canvas.configure(bg=self.rgb565_to_hex(self.level.background_color_565))

        for y in range(self.level.height):
            for x in range(self.level.width):
                self._draw_cell(x, y, tag=f"cell_{x}_{y}")

        self._draw_grid()
        self._draw_endcap_ghost()
        self._draw_portal_icon()
        self._draw_selection_overlay()

    def redraw_cell(self, x: int, y: int) -> None:
        tag = f"cell_{x}_{y}"
        self.canvas.delete(tag)
        self._draw_cell(x, y, tag=tag)
        self._draw_endcap_ghost()
        self._draw_portal_icon()
        self._draw_selection_overlay()

    def _cell_rect(self, x: int, y: int) -> Tuple[int, int, int, int]:
        x0 = x * CELL
        y0 = y * CELL + self._y_offset_px
        return x0, y0, x0 + CELL, y0 + CELL

    def _draw_grid(self) -> None:
        wpx = self.level.width * CELL
        hpx = GRID_H * CELL
        y0 = self._y_offset_px
        y1 = y0 + hpx

        for x in range(self.level.width + 1):
            xx = x * CELL
            self.canvas.create_line(xx, y0, xx, y1, fill=GRID_LINE)
        for y in range(self.level.height + 1):
            yy = y0 + y * CELL
            self.canvas.create_line(0, yy, wpx, yy, fill=GRID_LINE)

        x0 = self.level.endcap_x0() * CELL
        self.canvas.create_line(x0, y0, x0, y1, fill="#7a2cff", width=3)

    def _rotate_point_about(self, x: float, y: float, ox: float, oy: float, turns: float) -> Tuple[float, float]:
        a = turns * math.tau
        c = math.cos(a)
        s = math.sin(a)
        dx = x - ox
        dy = y - oy
        return (ox + dx * c - dy * s, oy + dx * s + dy * c)

    def _draw_rotated_primitive_polygon(self,
                                        center_col: float,
                                        center_row: float,
                                        pivot_col: float,
                                        pivot_row: float,
                                        turns: float,
                                        scale: float,
                                        shape: str,
                                        mod: str,
                                        tag: str,
                                        fill: str,
                                        outline: str) -> None:
        poly = self._base_polygon(shape)
        use_mod = mod if shape in MOD_SHAPES else MOD_NONE
        poly = [self._apply_mod(p, use_mod) for p in poly]

        pts = []
        for u, v in poly:
            col = (center_col - 0.5) + u
            row = (center_row - 0.5) + v

            col = pivot_col + (col - pivot_col) * scale
            row = pivot_row + (row - pivot_row) * scale

            col, row = self._rotate_point_about(col, row, pivot_col, pivot_row, turns)

            x = col * CELL
            y = row * CELL + self._y_offset_px
            pts.extend([x, y])

        self.canvas.create_polygon(
            pts,
            fill=fill,
            outline=outline,
            width=2,
            tags=tag
        )

    def _draw_primitive_polygon(self, x0, y0, x1, y1, shape: str, mod: str,
                                tag: str, fill: str, outline: str, stipple: str = "") -> None:
        use_mod = mod if shape in MOD_SHAPES else MOD_NONE
        poly = self._base_polygon(shape)
        poly = [self._apply_mod(p, use_mod) for p in poly]

        pts = []
        for u, v in poly:
            pts.extend([x0 + u * (x1 - x0), y0 + v * (y1 - y0)])

        self.canvas.create_polygon(
            pts,
            fill=fill,
            outline=outline,
            width=2,
            stipple=stipple,
            tags=tag
        )

    def _draw_star_cell(self, x: int, y: int, offset_mod: str, tag: str) -> None:
        cx = x + 0.5
        cy = y + 0.5

        if offset_mod in (OFF_RIGHT, OFF_DOWN_RIGHT):
            cx += 0.5
        if offset_mod in (OFF_DOWN, OFF_DOWN_RIGHT):
            cy += 0.5

        px = cx * CELL
        py = cy * CELL + self._y_offset_px

        r = CELL * 0.40

        pts = []
        for i in range(5):
            a = -math.pi / 2 + i * (2.0 * math.pi / 5.0)
            pts.append((px + math.cos(a) * r, py + math.sin(a) * r))

        order = [0, 2, 4, 1, 3, 0]
        for i in range(5):
            x0, y0 = pts[order[i]]
            x1, y1 = pts[order[i + 1]]
            self.canvas.create_line(x0, y0, x1, y1, fill=STAR_OUTLINE, width=2, tags=tag)

    def _draw_cell(self, x: int, y: int, tag: str) -> None:
        x0, y0, x1, y1 = self._cell_rect(x, y)

        cell = self.level.get_cell(x, y)
        if isinstance(cell, PrimitiveObstacle):
            if cell.shape == SHAPE_STAR:
                self._draw_star_cell(x, y, cell.offset_mod, tag)
            else:
                self._draw_primitive_polygon(
                    x0, y0, x1, y1,
                    cell.shape, cell.mod,
                    tag,
                    "",
                    self.rgb565_to_hex(self.level.obstacle_color_565)
                )
        elif isinstance(cell, AnimGroupRef):
            self._draw_anim_group_preview(x, y, cell, tag)

        self.canvas.create_rectangle(x0, y0, x1, y1, outline=GRID_LINE, width=1, tags=tag)

    def _draw_anim_group_preview(self, cell_x: int, cell_y: int, ref: AnimGroupRef, tag: str) -> None:
        anim = self.level.anim_defs[ref.group_index]
        if anim is None:
            x0, y0, x1, y1 = self._cell_rect(cell_x, cell_y)
            self.canvas.create_rectangle(
                x0 + 3, y0 + 3, x1 - 3, y1 - 3,
                outline=ANIM_OUTLINE, width=2, tags=tag
            )
            return

        anchor_col = cell_x + 0.5
        anchor_row = cell_y + 0.5

        if ref.offset_mod in (OFF_RIGHT, OFF_DOWN_RIGHT):
            anchor_col += 0.5
        if ref.offset_mod in (OFF_DOWN, OFF_DOWN_RIGHT):
            anchor_row += 0.5

        pivot_col = anchor_col + 0.5 * anim.pivot_hx
        pivot_row = anchor_row + 0.5 * anim.pivot_hy

        turns, scale = self.eval_anim_state(anim)

        for p in anim.primitives:
            prim_center_col = anchor_col + 0.5 * p.hx
            prim_center_row = anchor_row + 0.5 * p.hy

            self._draw_rotated_primitive_polygon(
                prim_center_col,
                prim_center_row,
                pivot_col,
                pivot_row,
                turns,
                scale,
                p.shape,
                p.mod,
                tag,
                "",
                ANIM_OUTLINE
            )

        cx0, cy0, cx1, cy1 = self._cell_rect(cell_x, cell_y)
        owner_outline = ANIM_OWNING_SELECTED if self.selected_anim_slot == ref.group_index else ANIM_OUTLINE

        self.canvas.create_rectangle(
            cx0 + 3, cy0 + 3, cx1 - 3, cy1 - 3,
            outline=owner_outline,
            width=2,
            dash=(4, 2),
            tags=tag
        )

        self.canvas.create_text(
            (cx0 + cx1) / 2,
            cy0 + 8,
            text=f"A{ref.group_index + 1}",
            fill=owner_outline,
            tags=tag,
            font=("Segoe UI", 8, "bold")
        )

    def _cell_rect_float(self, col0: float, row0: float, col1: float, row1: float) -> Tuple[float, float, float, float]:
        x0 = col0 * CELL
        y0 = row0 * CELL + self._y_offset_px
        x1 = col1 * CELL
        y1 = row1 * CELL + self._y_offset_px
        return x0, y0, x1, y1

    def _draw_endcap_ghost(self) -> None:
        self.canvas.delete("endcap")
        last = self.level.width - 1
        for e in ENDCAP_OBS:
            x = last + int(e["dx"])
            y = int(e["y"])
            if not self.level.in_bounds(x, y):
                continue
            x0, y0, x1, y1 = self._cell_rect(x, y)
            obs = PrimitiveObstacle.from_obj(e)
            self._draw_primitive_polygon(
                x0, y0, x1, y1,
                obs.shape, obs.mod,
                "endcap",
                "", "#ff3bd4", "gray25"
            )

    def _draw_portal_icon(self) -> None:
        self.canvas.delete("portal")
        px, py = self.level.portal_abs()
        if not self.level.in_bounds(px, py):
            return

        y_top = max(0, py - 1)
        y_bot = min(self.level.height - 1, py + 1)

        x0, y0, x1, y1 = self._cell_rect(px, y_top)
        _, _, _, y1b = self._cell_rect(px, y_bot)
        pad = 3
        self.canvas.create_oval(
            x0 + pad, y0 + pad, x1 - pad, y1b - pad,
            outline="#ff3bd4", width=3, stipple="gray25", tags="portal"
        )
        self.canvas.create_oval(
            x0 + pad + 5, y0 + pad + 5, x1 - pad - 5, y1b - pad - 5,
            outline="#ff8fe9", width=2, stipple="gray25", tags="portal"
        )

    def _draw_selection_overlay(self) -> None:
        if self._sel_rect_id is not None:
            self.canvas.delete(self._sel_rect_id)
            self._sel_rect_id = None

        b = self._sel_bounds()
        if not b:
            return
        x0, y0, x1, y1 = b
        x1 = min(x1, self.level.endcap_x0() - 1) if self.level.endcap_x0() > 0 else x1
        if x1 < x0:
            return

        px0 = x0 * CELL
        py0 = y0 * CELL + self._y_offset_px
        px1 = (x1 + 1) * CELL
        py1 = (y1 + 1) * CELL + self._y_offset_px
        self._sel_rect_id = self.canvas.create_rectangle(
            px0, py0, px1, py1,
            outline="#ffd54a",
            width=3,
            dash=(6, 4)
        )

    # -------- Copy/Cut/Paste --------

    def copy_selection(self) -> None:
        b = self._sel_bounds()
        if not b:
            messagebox.showinfo("Copy", "No selection. Hold Shift and drag to select.")
            return
        x0, y0, x1, y1 = b
        x1 = min(x1, self.level.endcap_x0() - 1) if self.level.endcap_x0() > 0 else x1
        if x1 < x0:
            return

        cells = []
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                cell = self.level.get_cell(x, y)
                if cell:
                    cells.append({"dx": x - x0, "dy": y - y0, **cell.to_obj()})

        self._clipboard = {"w": (x1 - x0 + 1), "h": (y1 - y0 + 1), "cells": cells}

    def cut_selection(self) -> None:
        self.copy_selection()
        if not self._clipboard:
            return
        b = self._sel_bounds()
        if not b:
            return
        x0, y0, x1, y1 = b
        x1 = min(x1, self.level.endcap_x0() - 1) if self.level.endcap_x0() > 0 else x1
        if x1 < x0:
            return

        self.begin_txn()
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if self.level.in_endcap(x):
                    continue
                self.touch_cell_before(x, y)
                self.level.clear_cell(x, y)
                self.touch_cell_after(x, y)
        self.commit_txn()
        self.redraw_all()
        self._refresh_animation_ui()
        self.clear_selection()

    def paste_clipboard(self) -> None:
        if not self._clipboard:
            messagebox.showinfo("Paste", "Clipboard is empty. Use Shift+drag then Ctrl+C.")
            return

        tx, ty = self._last_mouse_cell
        w = int(self._clipboard["w"])

        need_w = tx + w + ENDCAP_W
        if need_w > self.level.width:
            self.begin_txn()
            self._txn_before_width = self.level.width
            self.level.resize_width(need_w)
            self.level.strip_endcap_from_authored()
            self.commit_txn()

            self.width_var.set(self.level.width)
            self._rebuild_canvas_region()
            self.clear_selection()

        self.begin_txn()
        for c in self._clipboard["cells"]:
            x = tx + int(c["dx"])
            y = ty + int(c["dy"])
            if not self.level.in_bounds(x, y):
                continue
            if self.level.in_endcap(x):
                continue

            self.touch_cell_before(x, y)
            typ = c.get("type", "primitive")
            if typ == "anim_ref":
                cell: CellValue = AnimGroupRef.from_obj(c)
            else:
                cell = PrimitiveObstacle.from_obj(c)
            self.level.set_cell(x, y, cell)
            self.touch_cell_after(x, y)
        self.commit_txn()
        self.redraw_all()
        self._refresh_animation_ui()

    # -------- Color utils --------

    def rgb565_to_hex(self, c565: int) -> str:
        c565 &= 0xFFFF
        r5 = (c565 >> 11) & 0x1F
        g6 = (c565 >> 5) & 0x3F
        b5 = c565 & 0x1F
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31
        return f"#{r8:02x}{g8:02x}{b8:02x}"

    def hex_to_rgb565(self, hex_color: str) -> int:
        s = hex_color.lstrip("#")
        if len(s) != 6:
            return 0
        r8 = int(s[0:2], 16)
        g8 = int(s[2:4], 16)
        b8 = int(s[4:6], 16)
        r5 = (r8 * 31) // 255
        g6 = (g8 * 63) // 255
        b5 = (b8 * 31) // 255
        return ((r5 & 0x1F) << 11) | ((g6 & 0x3F) << 5) | (b5 & 0x1F)

    def refresh_color_ui(self) -> None:
        if hasattr(self, "bg_color_swatch"):
            self.bg_color_swatch.configure(bg=self.rgb565_to_hex(self.level.background_color_565))
        if hasattr(self, "bg_color_label"):
            self.bg_color_label.configure(text=f"0x{self.level.background_color_565:04X}")

        if hasattr(self, "obs_color_swatch"):
            self.obs_color_swatch.configure(bg=self.rgb565_to_hex(self.level.obstacle_color_565))
        if hasattr(self, "obs_color_label"):
            self.obs_color_label.configure(text=f"0x{self.level.obstacle_color_565:04X}")

    def choose_background_color(self) -> None:
        initial = self.rgb565_to_hex(self.level.background_color_565)
        result = colorchooser.askcolor(color=initial, title="Choose Background Color")
        if not result or not result[1]:
            return
        new_c = self.hex_to_rgb565(result[1])

        self.begin_txn()
        self.touch_all_anim_defs_before()
        self.level.background_color_565 = new_c
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.refresh_color_ui()
        self.redraw_all()

    def choose_obstacle_color(self) -> None:
        initial = self.rgb565_to_hex(self.level.obstacle_color_565)
        result = colorchooser.askcolor(color=initial, title="Choose Obstacle Color")
        if not result or not result[1]:
            return
        new_c = self.hex_to_rgb565(result[1])

        self.begin_txn()
        self.touch_all_anim_defs_before()
        self.level.obstacle_color_565 = new_c
        self.touch_all_anim_defs_after()
        self.commit_txn()

        self.refresh_color_ui()
        self.redraw_all()

    # -------- Painting / selection --------

    def on_left_down(self, e: tk.Event) -> None:
        cell = self.event_to_cell(e)
        if not cell:
            return
        x, y = cell

        if (e.state & 0x0001) != 0:
            self._selecting = True
            self._sel_a = cell
            self._sel_b = cell
            self._draw_selection_overlay()
            self._refresh_animation_ui()
            return

        self._painting = True
        self._erasing = False

        self.begin_txn()
        self._txn_before_width = self.level.width

        self._apply_left_action(x, y)

    def on_left_drag(self, e: tk.Event) -> None:
        cell = self.event_to_cell(e)
        if not cell:
            return
        x, y = cell

        if self._selecting:
            self._sel_b = cell
            self._draw_selection_overlay()
            self._refresh_animation_ui()
            return

        if not self._painting or self._erasing:
            return
        self._apply_left_action(x, y)

    def on_left_up(self, e: tk.Event) -> None:
        self._painting = False
        if self._selecting:
            self._selecting = False
            self._draw_selection_overlay()
            self._refresh_animation_ui()
            return
        self.commit_txn()
        self.redraw_all()
        self._refresh_animation_ui()

    def on_right_down(self, e: tk.Event) -> None:
        cell = self.event_to_cell(e)
        if not cell:
            return
        x, y = cell
        if self.level.in_endcap(x):
            return

        self._painting = True
        self._erasing = True

        self.begin_txn()
        self._txn_before_width = self.level.width

        self.touch_cell_before(x, y)
        self.level.clear_cell(x, y)
        self.touch_cell_after(x, y)

        self.redraw_cell(x, y)

    def on_right_drag(self, e: tk.Event) -> None:
        cell = self.event_to_cell(e)
        if not cell or not self._painting or not self._erasing:
            return
        x, y = cell
        if self.level.in_endcap(x):
            return

        self.touch_cell_before(x, y)
        self.level.clear_cell(x, y)
        self.touch_cell_after(x, y)
        self.redraw_cell(x, y)

    def on_right_up(self, e: tk.Event) -> None:
        self._painting = False
        self._erasing = False
        self.commit_txn()
        self.redraw_all()
        self._refresh_animation_ui()

    def _apply_left_action(self, x: int, y: int) -> None:
        if self.level.in_endcap(x):
            return

        self.touch_cell_before(x, y)

        mode = self.paint_mode_var.get()

        if mode == PAINT_MODE_ANIM_REF:
            if self.selected_anim_slot is None or self.level.anim_defs[self.selected_anim_slot] is None:
                self.touch_cell_after(x, y)
                return

            self.level.set_cell(
                x, y,
                AnimGroupRef(group_index=self.selected_anim_slot, offset_mod=self.anim_offset_var.get())
            )

        elif mode == PAINT_MODE_STAR:
            existing = self.level.get_cell(x, y)
            if isinstance(existing, PrimitiveObstacle) and existing.shape == SHAPE_STAR:
                # allow repainting/updating offset in-place without consuming a slot
                self.level.set_cell(
                    x, y,
                    PrimitiveObstacle(shape=SHAPE_STAR, mod=MOD_NONE, offset_mod=self.star_offset_var.get())
                )
            elif self.level.can_place_more_stars():
                self.level.set_cell(
                    x, y,
                    PrimitiveObstacle(shape=SHAPE_STAR, mod=MOD_NONE, offset_mod=self.star_offset_var.get())
                )
            else:
                self.touch_cell_after(x, y)
                return

        else:
            shape = self.shape_var.get()
            mod = self.mod_var.get()

            if shape not in MOD_SHAPES:
                mod = MOD_NONE

            self.level.set_cell(x, y, PrimitiveObstacle(shape=shape, mod=mod, offset_mod=OFF_NONE))

        self.touch_cell_after(x, y)
        self.redraw_cell(x, y)

    # -------- Scrolling --------

    def on_wheel(self, e: tk.Event) -> None:
        delta = -1 if e.delta > 0 else 1
        self.canvas.xview_scroll(delta * 3, "units")

    def on_left_panel_wheel(self, e: tk.Event) -> None:
        delta = -1 if e.delta > 0 else 1
        self.left_canvas.yview_scroll(delta * 3, "units")

    # -------- File ops --------

    def export_bin(self) -> None:
        path = filedialog.asksaveasfilename(
            title="Export Level Binary",
            defaultextension=".bin",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")]
        )
        if not path:
            return
        try:
            self.level.write_bin(path)
            messagebox.showinfo("Export", f"Saved:\n{path}")
        except Exception as ex:
            messagebox.showerror("Export failed", str(ex))

    def _write_json_to_path(self, path: str) -> None:
        self.level.strip_endcap_from_authored()
        data = self.level.to_json_obj()

        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

        self.current_file_path = path
        self._saved_revision = self._history_revision
        self.refresh_dirty_from_history()

    def save_json(self) -> None:
        if not self.current_file_path:
            self.save_json_as()
            return

        try:
            self._write_json_to_path(self.current_file_path)
            messagebox.showinfo("Save", f"Saved:\n{self.current_file_path}")
        except Exception as ex:
            messagebox.showerror("Save failed", str(ex))

    def save_json_as(self) -> None:
        path = filedialog.asksaveasfilename(
            title="Save Level JSON As",
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        if not path:
            return

        try:
            self._write_json_to_path(path)
            messagebox.showinfo("Save", f"Saved:\n{path}")
        except Exception as ex:
            messagebox.showerror("Save failed", str(ex))

    def open_json(self) -> None:
        path = filedialog.askopenfilename(
            title="Import Level JSON",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                obj = json.load(f)
            self.level = Level.from_json_obj(obj)
            self.reset_undo_state()
            self.width_var.set(self.level.width)
            self._rebuild_canvas_region()
            self.redraw_all()
            self._refresh_animation_ui()
            self.refresh_color_ui()
            self.current_file_path = path
            self._history_revision = 0
            self._saved_revision = 0
            self.refresh_dirty_from_history()
        except Exception as ex:
            messagebox.showerror("Import failed", str(ex))

    def new_level(self) -> None:
        if not messagebox.askyesno("New", "Clear the level?"):
            return
        self.level = Level(width=DEFAULT_W)
        self.reset_undo_state()
        self.selected_anim_slot = None
        self.width_var.set(self.level.width)
        self._rebuild_canvas_region()
        self.redraw_all()
        self._refresh_animation_ui()
        self.refresh_color_ui()
        self.current_file_path = None
        self._history_revision = 0
        self._saved_revision = 0
        self.refresh_dirty_from_history()

# -----------------------------
# Main
# -----------------------------

def main() -> None:
    root = tk.Tk()
    EditorApp(root)
    root.minsize(1120, 420)
    root.mainloop()

if __name__ == "__main__":
    main()