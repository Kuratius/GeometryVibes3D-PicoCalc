#pragma once
#include "core/Math.hpp"
#include "game/Level.hpp"

namespace gv {

// Apply modifier to a 3D point around an origin (XY rotation/invert, Z preserved).
static inline void applyMod3(ShapeMod shapeMod, const Vec3fx& origin, Vec3fx& p) {
    fx ox = origin.x, oy = origin.y, oz = origin.z;

    fx dx = p.x - ox;
    fx dy = p.y - oy;
    fx dz = p.z - oz;

    switch (shapeMod) {
        case ShapeMod::None:
            break;

        // +90° about +Z (CCW in XY)
        case ShapeMod::RotLeft: {
            fx ndx = -dy;
            fx ndy =  dx;
            dx = ndx;
            dy = ndy;
        } break;

        // -90° about +Z (CW in XY)
        case ShapeMod::RotRight: {
            fx ndx =  dy;
            fx ndy = -dx;
            dx = ndx;
            dy = ndy;
        } break;

        // 180° about origin in XY
        case ShapeMod::Invert:
            dx = -dx;
            dy = -dy;
            break;
    }

    p.x = ox + dx;
    p.y = oy + dy;
    p.z = oz + dz;
}

// Unapply modifier to a 2D point (inverse transform) around an origin in XY.
static inline void unapplyMod2(ShapeMod shapeMod, fx ox, fx oy, fx& x, fx& y) {
    fx dx = x - ox;
    fx dy = y - oy;

    switch (shapeMod) {
        case ShapeMod::None:
            break;

        case ShapeMod::RotLeft: {
            // Invert RotLeft by applying RotRight: (dx,dy) -> (-dy, dx)
            fx ndx = -dy;
            fx ndy =  dx;
            dx = ndx;
            dy = ndy;
        } break;

        case ShapeMod::RotRight: {
            // Invert RotRight by applying RotLeft: (dx,dy) -> (dy, -dx)
            fx ndx =  dy;
            fx ndy = -dx;
            dx = ndx;
            dy = ndy;
        } break;

        case ShapeMod::Invert:
            // Invert is self-inverse.
            dx = -dx;
            dy = -dy;
            break;
    }

    x = ox + dx;
    y = oy + dy;
}

} // namespace gv