#pragma once
#include <cstdint>
#include <array>
#include "RenderList.hpp"
#include "Project.hpp"
#include "game/Level.hpp"
#include "game/Config.hpp"

namespace gv {

class Game;

class Renderer {
public:
    void setCamera(const Camera& c);
    const Camera& camera() const { return cam; }

    void buildScene(RenderList& rl, const Game& game, fx scrollX) const;
    void buildOverlay(RenderList& rl, const Game& game) const;

private:
    Camera cam{};

    // --- Ship trail (level-space ring buffer) ---
    struct TrailPt {
        fx levelX;  // level-space X (advances with scroll)
        fx y;       // world Y
        fx z;       // world Z
    };

    mutable std::array<TrailPt, kTrailMax> trail_{};
    mutable int trailCount_ = 0;
    mutable int trailHead_  = 0;

private:
    // --- Shape constructors ---
    void addShip(RenderList& rl, const Vec3fx& pos, uint16_t color, fx shipY, fx shipVy) const;

    void addCube(RenderList& rl, const Vec3fx& pos, uint16_t color) const;

    void addSquarePyramid(RenderList& rl, const Vec3fx& pos, uint16_t color,
                          ModId mod, fx apexScale, const Vec3fx& origin) const;

    void addRightTriPrism(RenderList& rl, const Vec3fx& pos, uint16_t color,
                          ModId mod, const Vec3fx& origin) const;

    void addText(RenderList& rl, const Text& text, int16_t x, int16_t y, uint16_t color) const;

private:
    void trailPushLevelPoint(fx levelX, fx y, fx z) const;
    void trailDraw(RenderList& dl, fx scrollX, uint16_t color) const;
};

} // namespace gv