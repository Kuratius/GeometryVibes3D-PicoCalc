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

    void update(fx dt);
    void resetEffects();
    void startShipExplosion(const Game& game, uint32_t seed);
    bool explosionActive() const { return explosionActive_; }

    void initPortalRays() const;
    void updatePortalRays(fx dt) const;
    void drawPortalRays(RenderList& rl, const Game& game, fx scrollX, uint16_t color) const;

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

    // --- Ship explosion ---
    struct ExplosionChunk {
        Vec3fx origin{};
        Vec3fx vel{};
        Vec3fx a{};
        Vec3fx b{};
        Vec3fx c{};
    };

    static constexpr int kExplosionChunkCount = 8;

    mutable std::array<ExplosionChunk, kExplosionChunkCount> explosion_{};
    mutable fx explosionAge_{};
    mutable bool explosionActive_ = false;

    struct PortalRay {
        fx dirX{};
        fx dirY{};
        fx dirZ{};
        fx offset{};
        fx speed{};
        fx length{};
    };

    static constexpr int kPortalRayCount = 16;
    mutable std::array<PortalRay, kPortalRayCount> portalRays_{};
    mutable bool portalRaysInit_ = false;
    fx dtCache_ = fx::zero();

private:
    // --- Shape constructors ---
    void addShip(RenderList& rl, const Vec3fx& pos, uint16_t color, fx shipY, fx shipVy) const;

    void addCube(RenderList& rl, const Vec3fx& pos, uint16_t color) const;

    void addSquarePyramid(RenderList& rl, const Vec3fx& pos, uint16_t color,
                          ModId mod, fx apexScale, const Vec3fx& origin) const;

    void addRightTriPrism(RenderList& rl, const Vec3fx& pos, uint16_t color,
                          ModId mod, const Vec3fx& origin) const;

    void addText(RenderList& rl, const Text& text, int16_t x, int16_t y,
             uint16_t color, uint8_t alpha = 255, bool inverted = false) const;
             
    void drawExplosion(RenderList& rl, uint16_t color) const;

private:
    void trailPushLevelPoint(fx levelX, fx y, fx z) const;
    void trailDraw(RenderList& dl, fx scrollX, uint16_t color) const;
};

} // namespace gv