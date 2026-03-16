#include "SceneBuilder.hpp"
#include "ShapeGeometry.hpp"
#include "Game.hpp"
#include "Hud.hpp"
#include "Playfield.hpp"
#include "LevelMath.hpp"
#include <cmath>

namespace gv {

static inline fx fi(int v) { return fx::fromInt(v); }

static inline Vec3fx add3(const Vec3fx& a, const Vec3fx& b) {
    return Vec3fx{ a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline void line3(const Vec3fx& A, const Vec3fx& B, uint16_t color,
                         const Camera& cam, RenderList& rl) {
    Vec2i a, b;
    if (projectPoint(cam, A, a) && projectPoint(cam, B, b))
        rl.addLine(a.x, a.y, b.x, b.y, color);
}

static inline int iabs(int v) { return v < 0 ? -v : v; }

static inline uint32_t xorshift32(uint32_t& s) {
    if (s == 0) s = 0x6D2B79F5u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

static inline int randRange(uint32_t& s, int lo, int hi) {
    const uint32_t span = uint32_t(hi - lo + 1);
    return lo + int(xorshift32(s) % span);
}

static inline fx fxFromMilli(int v) {
    return fx::fromRaw((v * (1 << fx::SHIFT)) / 1000);
}

static inline void turnsToSinCos(fx turns, fx& c, fx& s) {
    const double t = double(turns.raw()) / double(1 << fx::SHIFT);
    const double a = t * 6.28318530717958647692;
    c = fx::fromRaw(int32_t(std::lround(std::cos(a) * double(1 << fx::SHIFT))));
    s = fx::fromRaw(int32_t(std::lround(std::sin(a) * double(1 << fx::SHIFT))));
}

static inline void rotateXYAround(Vec3fx& p, const Vec3fx& pivot, fx c, fx s) {
    const fx dx = p.x - pivot.x;
    const fx dy = p.y - pivot.y;
    p.x = pivot.x + dx * c - dy * s;
    p.y = pivot.y + dx * s + dy * c;
}

static inline void scaleXYAround(Vec3fx& p, const Vec3fx& pivot, fx scale) {
    p.x = pivot.x + (p.x - pivot.x) * scale;
    p.y = pivot.y + (p.y - pivot.y) * scale;
}

static inline void transformEdgePoint(Vec3fx& p,
                                      const Vec3fx& translate,
                                      ShapeMod mod,
                                      const Vec3fx* modOrigin,
                                      const Vec3fx* scalePivot,
                                      fx scale,
                                      const Vec3fx* rotatePivot,
                                      fx rotateCos,
                                      fx rotateSin) {
    p = add3(p, translate);

    if (modOrigin) {
        applyMod3(mod, *modOrigin, p);
    }

    if (scalePivot) {
        scaleXYAround(p, *scalePivot, scale);
    }

    if (rotatePivot) {
        rotateXYAround(p, *rotatePivot, rotateCos, rotateSin);
    }
}

static inline void drawTransformedEdges(RenderList& rl,
                                        const Camera& cam,
                                        const EdgeBuffer<16>& edges,
                                        const Vec3fx& translate,
                                        ShapeMod mod,
                                        const Vec3fx* modOrigin,
                                        const Vec3fx* scalePivot,
                                        fx scale,
                                        const Vec3fx* rotatePivot,
                                        fx rotateCos,
                                        fx rotateSin,
                                        uint16_t color) {
    for (std::size_t i = 0; i < edges.count; ++i) {
        Vec3fx a = edges.edges[i].a;
        Vec3fx b = edges.edges[i].b;

        transformEdgePoint(a, translate, mod, modOrigin, scalePivot, scale, rotatePivot, rotateCos, rotateSin);
        transformEdgePoint(b, translate, mod, modOrigin, scalePivot, scale, rotatePivot, rotateCos, rotateSin);

        line3(a, b, color, cam, rl);
    }
}

void SceneBuilder::updateExplosion(ExplosionState& explosion, fx dt) const {
    if (!explosion.active) return;

    explosion.age += dt;

    static constexpr fx kExplosionLife = fx::fromRatio(7, 10); // 0.7 s
    if (explosion.age >= kExplosionLife) {
        explosion.active = false;
    }
}

void SceneBuilder::startShipExplosion(ExplosionState& explosion, const Game& game, uint32_t seed) const {
    uint32_t s = seed ? seed : 0xA341316Cu;

    const Vec3fx shipPos{ game.shipRenderX(), game.ship().y, fi(kCellSize / 2) };

    for (int i = 0; i < ExplosionState::CHUNK_COUNT; ++i) {
        ExplosionChunk& ch = explosion.chunks[i];

        const int ox = randRange(s, -3, 3);
        const int oy = randRange(s, -3, 3);
        const int oz = randRange(s, -2, 2);

        ch.origin = Vec3fx{
            shipPos.x + fi(ox),
            shipPos.y + fi(oy),
            shipPos.z + fi(oz)
        };

        int vx = randRange(s, -70, 90);
        int vy = randRange(s, -90, 90);
        int vz = randRange(s, -30, 40);

        if ((i & 1) == 0) {
            vz += randRange(s, 35, 85);
        }

        ch.vel = Vec3fx{ fi(vx), fi(vy), fi(vz) };

        const int sx = randRange(s, 2, 5);
        const int sy = randRange(s, 2, 5);
        const int sz = randRange(s, 1, 3);

        ch.a = Vec3fx{ fi(-sx), fi(-sy), fi(0) };
        ch.b = Vec3fx{ fi( sx), fi(0),   fi( sz) };
        ch.c = Vec3fx{ fi(0),   fi( sy), fi(-sz) };
    }

    explosion.age = fx::zero();
    explosion.active = true;
}

void SceneBuilder::initPortalRays(PortalRayState& portalRays) const {
    if (portalRays.initialized) return;

    static constexpr int kDirMilli[16][2] = {
        { 1000,    0 }, {  924,  383 }, {  707,  707 }, {  383,  924 },
        {    0, 1000 }, { -383,  924 }, { -707,  707 }, { -924,  383 },
        {-1000,    0 }, { -924, -383 }, { -707, -707 }, { -383, -924 },
        {    0,-1000 }, {  383, -924 }, {  707, -707 }, {  924, -383 }
    };

    for (int i = 0; i < PortalRayState::RAY_COUNT; ++i) {
        PortalRay& r = portalRays.rays[i];
        r.dirX = fxFromMilli(180 + (i % 4) * 40);
        r.dirY = fxFromMilli(kDirMilli[i][0]);
        r.dirZ = fxFromMilli(kDirMilli[i][1]);

        r.offset = fx::fromInt((i * 3) % 12);
        r.speed  = fx::fromInt(10 + (i % 5) * 3);
        r.length = fx::fromInt(4 + (i % 4) * 2);
    }

    portalRays.initialized = true;
}

void SceneBuilder::updatePortalRays(PortalRayState& portalRays, fx dt) const {
    initPortalRays(portalRays);

    static constexpr fx kWrap = fx::fromInt(18);

    for (int i = 0; i < PortalRayState::RAY_COUNT; ++i) {
        PortalRay& r = portalRays.rays[i];
        r.offset += r.speed * dt;

        if (r.offset > kWrap) {
            r.offset -= kWrap;
        }
    }
}

void SceneBuilder::drawPortalRays(RenderList& rl, const Camera& cam, const Game& game,
                                  fx scrollX, const PortalRayState& portalRays,
                                  uint16_t color) const {
    if (!game.hasLevel()) return;
    if (!portalRays.initialized) return;

    const LevelHeader& h = game.levelHeader();
    const int portalCol = portal_abs_x(h);
    if (portalCol < 0 || portalCol >= int(h.width)) return;

    const fx portalX = worldXForColumn(portalCol, scrollX) + fi(kCellSize / 2);

    const fx portalY = worldYForRow(4) + fi(kCellSize / 2);
    const fx portalZ = fi(kCellSize / 2);

    const Vec3fx center{ portalX, portalY, portalZ };

    for (int i = 0; i < PortalRayState::RAY_COUNT; ++i) {
        const PortalRay& r = portalRays.rays[i];

        const fx r0 = r.offset;
        const fx r1 = r.offset + r.length;

        Vec3fx a{
            center.x - r.dirX * r0,
            center.y + r.dirY * r0,
            center.z + r.dirZ * r0
        };

        Vec3fx b{
            center.x - r.dirX * r1,
            center.y + r.dirY * r1,
            center.z + r.dirZ * r1
        };

        line3(a, b, color, cam, rl);
    }
}

void SceneBuilder::trailPushLevelPoint(TrailState& trail, const Camera& cam,
                                       fx levelX, fx y, fx z) const {
    if (trail.count > 0) {
        int lastIdx = trail.head - 1;
        if (lastIdx < 0) lastIdx += kTrailMax;

        const TrailPt last = trail.pts[lastIdx];

        Vec2i a{}, b{};
        Vec3fx wa{ last.levelX - levelX + fx::fromInt(kShipFixedX), last.y, last.z };
        Vec3fx wb{ fx::fromInt(kShipFixedX), y, z };

        if (projectPoint(cam, wa, a) && projectPoint(cam, wb, b)) {
            const int dx = iabs(int(b.x) - int(a.x));
            const int dy = iabs(int(b.y) - int(a.y));
            if (dx + dy > 120) {
                trail.count = 0;
                trail.head = 0;
            }
        }
    }

    trail.pts[trail.head] = TrailPt{ levelX, y, z };
    ++trail.head;
    if (trail.head >= kTrailMax) trail.head = 0;

    if (trail.count < kTrailMax) ++trail.count;
}

void SceneBuilder::trailDraw(RenderList& rl, const Camera& cam,
                             const TrailState& trail, fx scrollX, uint16_t color) const {
    if (trail.count < 2) return;

    int start = trail.head - trail.count;
    if (start < 0) start += kTrailMax;

    Vec2i prev{};
    bool havePrev = false;

    int idx = start;
    for (int i = 0; i < trail.count; ++i) {
        const TrailPt tp = trail.pts[idx];

        const fx wx = tp.levelX - scrollX + fx::fromInt(kShipFixedX);
        Vec3fx w{ wx, tp.y, tp.z };

        Vec2i cur{};
        if (!projectPoint(cam, w, cur)) {
            havePrev = false;
        } else {
            if (havePrev) {
                rl.addLine(prev.x, prev.y, cur.x, cur.y, color);
            }
            prev = cur;
            havePrev = true;
        }

        ++idx;
        if (idx >= kTrailMax) idx = 0;
    }
}

#ifdef GV3D_TESTING
void SceneBuilder::buildCollisionDebug(RenderList& rl, const Camera& cam, const Game& game) const {
    static constexpr uint16_t kCollisionDebug = 0xF800; // red

    for (const auto& p : game.collisionDebugPrimitives()) {
        if (p.animated) {
            addAnimatedPrimitive(
                rl,
                cam,
                p.sid,
                p.mod,
                Vec3fx{ p.primitiveCenterX, p.primitiveCenterY, p.primitiveCenterZ },
                Vec3fx{ p.groupPivotX, p.groupPivotY, p.groupPivotZ },
                p.groupScale,
                p.groupCos,
                p.groupSin,
                kCollisionDebug
            );
        } else {
            const Vec3fx worldOrigin{ p.worldOriginX, p.worldOriginY, p.worldOriginZ };
            const Vec3fx primitiveCenter{ p.primitiveCenterX, p.primitiveCenterY, p.primitiveCenterZ };

            switch (p.sid) {
                case ObstacleId::Square:
                    addCube(rl, cam, worldOrigin, kCollisionDebug);
                    break;

                case ObstacleId::RightTri:
                    addRightTriPrism(rl, cam, worldOrigin, kCollisionDebug, p.mod, primitiveCenter);
                    break;

                case ObstacleId::HalfSpike:
                    addSquarePyramid(rl, cam, worldOrigin, kCollisionDebug, p.mod, fx::half(), primitiveCenter);
                    break;

                case ObstacleId::FullSpike:
                    addSquarePyramid(rl, cam, worldOrigin, kCollisionDebug, p.mod, fx::one(), primitiveCenter);
                    break;

                default:
                    break;
            }
        }
    }
}
#endif

void SceneBuilder::addShip(RenderList &rl, const Camera& cam, const Vec3fx &pos,
                           uint16_t color, fx shipY, fx shipVy) const {
    const fx halfW = fi(kCellSize/4);
    const fx len   = fi(kCellSize) * fx::fromRatio(9, 20);
    const fx hz    = fi(kCellSize) * fx::fromRatio(3, 50);

    Vec3fx v0{  len,  fx::zero(), fx::zero() };
    Vec3fx v1{ -len,  halfW,      fx::zero() };
    Vec3fx v2{ -len, -halfW,      fx::zero() };

    const fx playHalf = playHalfH();
    const fx clipZoneStart = playHalf - halfW;
    const fx absY = abs(shipY);
    const bool clipping = (absY > clipZoneStart);

    fx c = fx::one();
    fx s = fx::zero();

    if (!clipping) {
        constexpr fx cos45 = fx::fromRaw(46341);
        c = cos45;
        if (shipVy.raw() > 0) {
            s = cos45;
        } else if (shipVy.raw() < 0) {
            s = -cos45;
        } else {
            s = fx::zero();
            c = fx::one();
        }
    }

    auto rotZ = [&](Vec3fx& p) {
        fx x = p.x;
        fx y = p.y;
        p.x = x * c - y * s;
        p.y = x * s + y * c;
    };

    rotZ(v0); rotZ(v1); rotZ(v2);

    Vec3fx a0{ v0.x, v0.y, v0.z - hz }, a1{ v1.x, v1.y, v1.z - hz }, a2{ v2.x, v2.y, v2.z - hz };
    Vec3fx b0{ v0.x, v0.y, v0.z + hz }, b1{ v1.x, v1.y, v1.z + hz }, b2{ v2.x, v2.y, v2.z + hz };

    auto add = [&](const Vec3fx& p) { return Vec3fx{ pos.x + p.x, pos.y + p.y, pos.z + p.z }; };

    line3(add(a0), add(a1), color, cam, rl);
    line3(add(a1), add(a2), color, cam, rl);
    line3(add(a2), add(a0), color, cam, rl);

    line3(add(b0), add(b1), color, cam, rl);
    line3(add(b1), add(b2), color, cam, rl);
    line3(add(b2), add(b0), color, cam, rl);

    line3(add(a0), add(b0), color, cam, rl);
    line3(add(a1), add(b1), color, cam, rl);
    line3(add(a2), add(b2), color, cam, rl);
}

void SceneBuilder::addCube(RenderList &rl, const Camera& cam, const Vec3fx &pos, uint16_t color) const {
    EdgeBuffer<16> edges{};
    buildCubeLocal(edges);
    drawTransformedEdges(
        rl,
        cam,
        edges,
        pos,
        ShapeMod::None,
        nullptr,
        nullptr,
        fx::one(),
        nullptr,
        fx::one(),
        fx::zero(),
        color
    );
}

void SceneBuilder::addSquarePyramid(RenderList& rl, const Camera& cam, const Vec3fx& pos, uint16_t color,
                                    ShapeMod mod, fx apexScale, const Vec3fx& origin) const {
    EdgeBuffer<16> edges{};
    buildSquarePyramidLocal(edges, apexScale);
    drawTransformedEdges(
        rl,
        cam,
        edges,
        pos,
        mod,
        &origin,
        nullptr,
        fx::one(),
        nullptr,
        fx::one(),
        fx::zero(),
        color
    );
}

void SceneBuilder::addRightTriPrism(RenderList& rl, const Camera& cam, const Vec3fx& pos, uint16_t color,
                                    ShapeMod mod, const Vec3fx& origin) const {
    EdgeBuffer<16> edges{};
    buildRightTriPrismLocal(edges);
    drawTransformedEdges(
        rl,
        cam,
        edges,
        pos,
        mod,
        &origin,
        nullptr,
        fx::one(),
        nullptr,
        fx::one(),
        fx::zero(),
        color
    );
}

void SceneBuilder::addAnimatedPrimitive(RenderList& rl,
                                        const Camera& cam,
                                        ObstacleId sid,
                                        ShapeMod mod,
                                        const Vec3fx& primitiveCenter,
                                        const Vec3fx& groupPivot,
                                        fx groupScale,
                                        fx groupCos,
                                        fx groupSin,
                                        uint16_t color) const {
    if (sid != ObstacleId::Square &&
        sid != ObstacleId::RightTri &&
        sid != ObstacleId::HalfSpike &&
        sid != ObstacleId::FullSpike) {
        return;
    }

    const fx half = fi(kCellSize / 2);
    const Vec3fx primitiveOrigin{
        primitiveCenter.x - half,
        primitiveCenter.y - half,
        fx::zero()
    };

    EdgeBuffer<16> edges{};
    buildPrimitiveLocal(edges, sid);

    drawTransformedEdges(
        rl,
        cam,
        edges,
        primitiveOrigin,
        mod,
        &primitiveCenter,
        &groupPivot,
        groupScale,
        &groupPivot,
        groupCos,
        groupSin,
        color
    );
}

void SceneBuilder::addText(RenderList& rl, const Text& text, int16_t x, int16_t y,
                           uint16_t color, uint8_t alpha, bool inverted,
                           uint16_t bgColor565, uint8_t styleFlags) const {
    if (!text.empty()) {
        rl.addText(&text, x, y, color, alpha, inverted, bgColor565, styleFlags);
    }
}

void SceneBuilder::drawExplosion(RenderList& rl, const Camera& cam,
                                 const ExplosionState& explosion, uint16_t color) const {
    if (!explosion.active) return;

    static constexpr fx kBurstTime = fx::fromRatio(3, 20);
    const fx age = explosion.age;

    for (int i = 0; i < ExplosionState::CHUNK_COUNT; ++i) {
        const ExplosionChunk& ch = explosion.chunks[i];
        const Vec3fx pos{
            ch.origin.x + ch.vel.x * age,
            ch.origin.y + ch.vel.y * age,
            ch.origin.z + ch.vel.z * age
        };

        if (age < kBurstTime) {
            const fx burstScale = fx::fromInt(2);
            const fx t = age * burstScale;
            Vec3fx burstEnd{
                ch.origin.x + ch.vel.x * t,
                ch.origin.y + ch.vel.y * t,
                ch.origin.z + ch.vel.z * t
            };
            line3(ch.origin, burstEnd, color, cam, rl);
        }

        const Vec3fx p0{ pos.x + ch.a.x, pos.y + ch.a.y, pos.z + ch.a.z };
        const Vec3fx p1{ pos.x + ch.b.x, pos.y + ch.b.y, pos.z + ch.b.z };
        const Vec3fx p2{ pos.x + ch.c.x, pos.y + ch.c.y, pos.z + ch.c.z };

        line3(p0, p1, color, cam, rl);
        line3(p1, p2, color, cam, rl);
        line3(p2, p0, color, cam, rl);
    }
}

static inline void rectWireXZ(
    RenderList& rl, const Camera& cam,
    fx x0, fx x1, fx y, fx z0, fx z1,
    uint16_t color)
{
    Vec3fx p00{ x0, y, z0 };
    Vec3fx p10{ x1, y, z0 };
    Vec3fx p11{ x1, y, z1 };
    Vec3fx p01{ x0, y, z1 };

    line3(p00, p10, color, cam, rl);
    line3(p10, p11, color, cam, rl);
    line3(p11, p01, color, cam, rl);
    line3(p01, p00, color, cam, rl);
}

void SceneBuilder::buildScene(RenderList& rl,
                              const Camera& cam,
                              const Game& game,
                              fx scrollX,
                              TrailState& trail,
                              const ExplosionState& explosion,
                              const PortalRayState& portalRays) const {
    const uint16_t kWire   = 0xFFFF;
    const uint16_t kShip   = 0xFFFF;
    const uint16_t kCyan   = 0x07FF;
    const uint16_t kPurple = 0xF81F;

    if (!game.hasLevel()) return;

    const LevelHeader& hdr = game.levelHeader();
    const uint16_t obstacleColor = (hdr.obstacleColor565 != 0) ? hdr.obstacleColor565 : uint16_t(0x07E0);
    const uint16_t bgColor = hdr.backgroundColor565;

    const int levelW = int(hdr.width);

    int scrollCol = scrollX.toInt() / kCellSize;
    if (scrollCol < 0) scrollCol = 0;

    int col0 = scrollCol - kColsPadLeft;
    if (col0 < 0) col0 = 0;

    int col1 = col0 + kColsVisible;
    if (col1 > levelW) col1 = levelW;

    const fx z0 = fx::zero();
    const fx z1 = fi(kCellSize);
    const fx yTop = playCenterY() + playHalfH();
    const fx yBot = playCenterY() - playHalfH();

    const fx xLeft  = fx::fromInt(col0 * kCellSize) - scrollX + fx::fromInt(kShipFixedX) - fi(kCellSize * 2);
    const fx xRight = fx::fromInt(col1 * kCellSize) - scrollX + fx::fromInt(kShipFixedX) + fi(kCellSize * 2);

    rectWireXZ(rl, cam, xLeft, xRight, yTop, z0, z1, kWire);
    rectWireXZ(rl, cam, xLeft, xRight, yBot, z0, z1, kWire);

    Column56 col{};
    for (int cx = col0; cx < col1; ++cx) {
        if (!game.readLevelColumn((uint16_t)cx, col))
            continue;

        const fx worldX = worldXForColumn(cx, scrollX);
        const fx cellCenterX = worldX + fi(kCellSize / 2);

        for (int row = 0; row < kLevelHeight; ++row) {
            const ObstacleId sid = col.obstacle(row);
            if (sid == ObstacleId::Empty) continue;

            const fx worldY = worldYForRow(row);
            const fx cellCenterY = worldY + fi(kCellSize / 2);
            const fx cz = fx::zero();

            const fx ox = worldX + fi(kCellSize/2);
            const fx oy = worldY + fi(kCellSize/2);

            if (is_anim_group(sid)) {
                const uint8_t defIndex = anim_group_index(sid);
                if (defIndex >= game.animGroupDefs().size()) {
                    continue;
                }

                const AnimGroupOffsetMod gm = col.groupMod(row);

                fx anchorX = cellCenterX;
                fx anchorY = cellCenterY;

                const fx halfCell = fi(kCellSize / 2);

                if (gm == AnimGroupOffsetMod::ShiftRight) {
                    anchorX = anchorX + halfCell;
                } else if (gm == AnimGroupOffsetMod::ShiftDown) {
                    anchorY = anchorY - halfCell;
                } else if (gm == AnimGroupOffsetMod::ShiftDownRight) {
                    anchorX = anchorX + halfCell;
                    anchorY = anchorY - halfCell;
                }

                const LoadedAnimGroupDef& def = game.animGroupDefs()[defIndex];
                const fx angleTurns = game.animGroupAngleTurns(defIndex);
                const fx groupScale = game.animGroupScale(defIndex);

                fx groupCos = fx::one();
                fx groupSin = fx::zero();
                turnsToSinCos(angleTurns, groupCos, groupSin);

                const Vec3fx groupPivot{
                    anchorX + gv::mulInt(halfCell, def.hdr.pivotHx),
                    anchorY - gv::mulInt(halfCell, def.hdr.pivotHy),
                    fi(kCellSize / 2)
                };

                for (uint8_t i = 0; i < def.hdr.primitiveCount; ++i) {
                    const AnimPrimitiveDef& p = game.animPrimitives()[def.firstPrimitive + i];

                    const Vec3fx primitiveCenter{
                        anchorX + gv::mulInt(halfCell, p.hx),
                        anchorY - gv::mulInt(halfCell, p.hy),
                        fi(kCellSize / 2)
                    };

                    addAnimatedPrimitive(
                        rl,
                        cam,
                        p.obstacle,
                        p.mod,
                        primitiveCenter,
                        groupPivot,
                        groupScale,
                        groupCos,
                        groupSin,
                        obstacleColor
                    );
                }

                continue;
            }

            const ShapeMod mid = col.shapeMod(row);

            switch (sid) {
                case ObstacleId::Square:
                    addCube(rl, cam, {worldX, worldY, cz}, obstacleColor);
                    break;

                case ObstacleId::RightTri:
                    addRightTriPrism(rl, cam, {worldX, worldY, cz}, obstacleColor, mid, {ox, oy, cz});
                    break;

                case ObstacleId::HalfSpike:
                    addSquarePyramid(rl, cam, {worldX, worldY, cz}, obstacleColor, mid, fx::half(), {ox, oy, cz});
                    break;

                case ObstacleId::FullSpike:
                    addSquarePyramid(rl, cam, {worldX, worldY, cz}, obstacleColor, mid, fx::one(), {ox, oy, cz});
                    break;

                default:
                    break;
            }
        }
    }

    const int portalCol = portal_abs_x(hdr);
    const int py = 4;

    if (portalCol >= col0 && portalCol < col1) {
        const fx px = worldXForColumn(portalCol, scrollX);
        const fx cz = fx::zero();

        for (int dy = -1; dy <= 1; ++dy) {
            int row = py + dy;
            if (row < 0) row = 0;
            if (row > (kLevelHeight - 1)) row = (kLevelHeight - 1);

            const fx pyWorld = worldYForRow(row);
            addCube(rl, cam, {px, pyWorld, cz}, kPurple);
        }
        drawPortalRays(rl, cam, game, scrollX, portalRays, kWire);
    }

#ifdef GV3D_TESTING
    buildCollisionDebug(rl, cam, game);
#endif

    const Vec3fx shipPos{ game.shipRenderX(), game.ship().y, fi(kCellSize/2) };

    if (game.state() != RunState::Dead) {
        trailDraw(rl, cam, trail, scrollX, kCyan);

        const fx shipLevelX = scrollX + (game.shipRenderX() - fx::fromInt(kShipFixedX));
        trailPushLevelPoint(trail, cam, shipLevelX, game.ship().y, fi(kCellSize/2));

        addShip(rl, cam, shipPos, kShip, game.ship().y, game.ship().vy);
    } else {
        drawExplosion(rl, cam, explosion, kShip);
    }
}

void SceneBuilder::buildHud(RenderList& rl, const Game& game, int screenW, int screenH) const {
    static constexpr uint16_t kHud = 0xFFFF;

    const Hud& hud = game.hud();

    addText(rl, hud.controlsHint(), 4, 4, kHud);

    const int levelW = hud.levelLabel().width();
    if (levelW > 0) {
        addText(rl, hud.levelLabel(), int16_t((screenW - levelW) / 2), 4, kHud);
    }

    const int progressW = hud.progress().width();

    static constexpr int kBarW = 64;
    static constexpr int kBarH = 8;
    const int barX = screenW - 4 - progressW - 6 - kBarW;
    const int barY = 4;

    const int pct = game.progressPercent();
    int fillW = (pct * (kBarW - 2)) / 100;
    if (fillW < 0) fillW = 0;
    if (fillW > (kBarW - 2)) fillW = (kBarW - 2);

    rl.addLine((int16_t)barX, (int16_t)barY, (int16_t)(barX + kBarW - 1), (int16_t)barY, kHud);
    rl.addLine((int16_t)(barX + kBarW - 1), (int16_t)barY, (int16_t)(barX + kBarW - 1), (int16_t)(barY + kBarH - 1), kHud);
    rl.addLine((int16_t)(barX + kBarW - 1), (int16_t)(barY + kBarH - 1), (int16_t)barX, (int16_t)(barY + kBarH - 1), kHud);
    rl.addLine((int16_t)barX, (int16_t)(barY + kBarH - 1), (int16_t)barX, (int16_t)barY, kHud);

    if (fillW > 0) {
        rl.addFillRect(
            (int16_t)(barX + 1),
            (int16_t)(barY + 1),
            (int16_t)fillW,
            (int16_t)(kBarH - 2),
            kHud
        );
    }

    if (progressW > 0) {
        addText(rl, hud.progress(), int16_t(screenW - 4 - progressW), 4, kHud);
    }

    if (hud.eventVisible()) {
        const uint16_t eventColor = hud.eventColor();
        if (eventColor != 0) {
            addText(rl, hud.eventText(), 4, int16_t(screenH - 12), eventColor);
        }
    }
}

} // namespace gv