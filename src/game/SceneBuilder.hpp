#pragma once
#include <cstdint>
#include <array>
#include "render/RenderList.hpp"
#include "render/Project.hpp"
#include "Level.hpp"
#include "core/Config.hpp"
#include "render/Colors.hpp"

namespace gv {

class Game;

struct TrailPt {
    fx levelX{};
    fx y{};
    fx z{};
};

struct TrailState {
    std::array<TrailPt, kTrailMax> pts{};
    int count = 0;
    int head  = 0;
};

struct ExplosionChunk {
    Vec3fx origin{};
    Vec3fx vel{};
    Vec3fx a{};
    Vec3fx b{};
    Vec3fx c{};
};

struct ExplosionState {
    static constexpr int CHUNK_COUNT = 8;

    std::array<ExplosionChunk, CHUNK_COUNT> chunks{};
    fx age{};
    bool active = false;
};

struct PortalRay {
    fx dirX{};
    fx dirY{};
    fx dirZ{};
    fx offset{};
    fx speed{};
    fx length{};
};

struct PortalRayState {
    static constexpr int RAY_COUNT = 16;

    std::array<PortalRay, RAY_COUNT> rays{};
    bool initialized = false;
};

class SceneBuilder {
public:
    void updateExplosion(ExplosionState& explosion, fx dt) const;
    void startShipExplosion(ExplosionState& explosion, const Game& game, uint32_t seed) const;

    void initPortalRays(PortalRayState& portalRays) const;
    void updatePortalRays(PortalRayState& portalRays, fx dt) const;

    void buildScene(RenderList& rl,
                    const Camera& cam,
                    const Game& game,
                    fx scrollX,
                    TrailState& trail,
                    const ExplosionState& explosion,
                    const PortalRayState& portalRays) const;

    void buildHud(RenderList& rl, const Game& game, int screenW, int screenH) const;

    void buildCollisionDebug(RenderList& rl, const Camera& cam, const Game& game) const;

private:
    void addShip(RenderList& rl, const Camera& cam, const Vec3fx& pos,
                 uint16_t color, fx shipY, fx shipVy) const;

    void addCube(RenderList& rl, const Camera& cam, const Vec3fx& pos,
                 uint16_t color) const;

    void addSquarePyramid(RenderList& rl, const Camera& cam, const Vec3fx& pos,
                          uint16_t color, ShapeMod mod, fx apexScale,
                          const Vec3fx& origin) const;

    void addRightTriPrism(RenderList& rl, const Camera& cam, const Vec3fx& pos,
                          uint16_t color, ShapeMod mod, const Vec3fx& origin) const;

    void addAnimatedPrimitive(RenderList& rl,
                              const Camera& cam,
                              ObstacleId sid,
                              ShapeMod mod,
                              const Vec3fx& primitiveCenter,
                              const Vec3fx& groupPivot,
                              fx groupScale,
                              fx groupCos,
                              fx groupSin,
                              uint16_t color) const;

    void addStar(RenderList& rl,
                 const Camera& cam,
                 const Vec3fx& center,
                 fx turns,
                 uint16_t color) const;

    void addText(RenderList& rl, const Text& text, int16_t x, int16_t y,
                 uint16_t color, uint8_t alpha = 255, bool inverted = false,
                 uint16_t bgColor565 = gv::color::Black,
                 uint8_t styleFlags = TextStyle::None) const;

    void drawExplosion(RenderList& rl, const Camera& cam,
                       const ExplosionState& explosion, uint16_t color) const;

    void drawPortalRays(RenderList& rl, const Camera& cam, const Game& game,
                        fx scrollX, const PortalRayState& portalRays,
                        uint16_t color) const;

    void trailPushLevelPoint(TrailState& trail, const Camera& cam,
                             fx levelX, fx y, fx z) const;

    void trailDraw(RenderList& rl, const Camera& cam,
                   const TrailState& trail, fx scrollX, uint16_t color) const;

private:
    void addParallaxStarfield(RenderList& rl, const Camera& cam, fx scrollX) const;
};

} // namespace gv