#pragma once
#include "core/Config.hpp"
#include "core/Fixed.hpp"

namespace gv {

static inline constexpr int kPlayfieldRows = kLevelHeight;

static inline constexpr fx playCenterY() { return fx::zero(); }

// Treat the playfield height as kLevelHeight * kCellSize, centered on 0.
static inline constexpr fx playHalfH() {
    return fx::fromInt((kLevelHeight * kCellSize) / 2);
}

static inline constexpr fx cellSizeFx() { return fx::fromInt(kCellSize); }

// SceneBuilder cell origin mapping (matches current buildScene):
//   worldY(cell origin at row) = center + halfH - cellH - row*cellH
static inline fx worldYForRow(int row) {
    const fx cellH = cellSizeFx();
    return playCenterY() + playHalfH() - cellH - mulInt(cellH, row);
}

static inline fx topEdgeY() {
    return playCenterY() + playHalfH();
}

// Invert worldYForRow() so we can get row index for a given world Y.
// Clamp the result into 0..kLevelHeight-1.
//
// Cells occupy:
//   row 0: [topEdgeY() - 1*cellH, topEdgeY()]
//   row 1: [topEdgeY() - 2*cellH, topEdgeY() - 1*cellH]
// etc.
//
// We therefore map from the top edge of the playfield, not from the
// origin of row 0.
static inline int rowFromWorldY(fx wy) {
    const int rawDist = (topEdgeY() - wy).raw();
    const int rawCell = fx::fromInt(kCellSize).raw();

    int row = rawDist / rawCell;
    if ((rawDist < 0) && (rawDist % rawCell)) {
        --row; // force floor division for negatives
    }

    if (row < 0) row = 0;
    if (row > (kLevelHeight - 1)) row = (kLevelHeight - 1);
    return row;
}

// The "ship is fixed on screen" convention uses kShipFixedX as a world-space X.
// Obstacles are placed at (col*kCellSize - scrollX + kShipFixedX).
static inline fx worldXForColumn(int col, fx scrollX) {
    return fx::fromInt(col * kCellSize) - scrollX + fx::fromInt(kShipFixedX);
}

// For collision, we want local X inside a column cell. Since the ship is at
// worldX = kShipFixedX and the cell is at worldXForColumn(), local X becomes:
//   lx = shipX - cellWorldX = scrollX - colX0
static inline fx localXInColumn(fx scrollX, int col) {
    const fx colX0 = fx::fromInt(col * kCellSize);
    return scrollX - colX0;
}

} // namespace gv