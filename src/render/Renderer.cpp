#include "Renderer.hpp"
#include "game/Config.hpp"
#include "game/Game.hpp"
#include "game/Hud.hpp"
#include "game/Playfield.hpp"
#include "game/LevelMath.hpp"

namespace gv {

void Renderer::setCamera(const Camera& c) {
    cam = c;
    buildCameraBasis(cam);
}

void Renderer::update(fx dt) {
    dtCache_ = dt;
    if (!explosionActive_) return;

    explosionAge_ += dt;

    static constexpr fx kExplosionLife = fx::fromRatio(7, 10); // 0.7 s
    if (explosionAge_ >= kExplosionLife) {
        explosionActive_ = false;
    }
}

void Renderer::resetEffects() {
    explosionActive_ = false;
    explosionAge_ = fx::zero();
    trailCount_ = 0;
    trailHead_ = 0;
    portalRaysInit_ = false;
}

static inline fx fi(int v) { return fx::fromInt(v); }

static inline Vec3fx add3(const Vec3fx& a, const Vec3fx& b) {
    return Vec3fx{ a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline void line3(const Vec3fx& A, const Vec3fx& B, uint16_t color, const Camera& cam, RenderList& rl) {
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
    // v / 1000
    return fx::fromRaw((v * (1 << fx::SHIFT)) / 1000);
}

void Renderer::startShipExplosion(const Game& game, uint32_t seed) {
    uint32_t s = seed ? seed : 0xA341316Cu;

    const Vec3fx shipPos{ game.shipRenderX(), game.ship().y, fi(kCellSize / 2) };

    for (int i = 0; i < kExplosionChunkCount; ++i) {
        ExplosionChunk& ch = explosion_[i];

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

        // Bias a few chunks toward the camera.
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

    explosionAge_ = fx::zero();
    explosionActive_ = true;
}

void Renderer::initPortalRays() const {
    if (portalRaysInit_) return;

    // 16 evenly spaced directions around the portal center in the Y/Z plane.
    // These are approximate unit-circle directions in fixed point.
    static constexpr int kDirMilli[16][2] = {
        { 1000,    0 }, {  924,  383 }, {  707,  707 }, {  383,  924 },
        {    0, 1000 }, { -383,  924 }, { -707,  707 }, { -924,  383 },
        {-1000,    0 }, { -924, -383 }, { -707, -707 }, { -383, -924 },
        {    0,-1000 }, {  383, -924 }, {  707, -707 }, {  924, -383 }
    };

    for (int i = 0; i < kPortalRayCount; ++i) {
        PortalRay& r = portalRays_[i];
        r.dirX = fxFromMilli(180 + (i % 4) * 40);
        r.dirY = fxFromMilli(kDirMilli[i][0]);
        r.dirZ = fxFromMilli(kDirMilli[i][1]);

        // Stagger the rays so they are already distributed when first seen.
        r.offset = fx::fromInt((i * 3) % 12);

        // Small variation in speed and length.
        r.speed  = fx::fromInt(10 + (i % 5) * 3);
        r.length = fx::fromInt(4 + (i % 4) * 2);
    }

    portalRaysInit_ = true;
}

void Renderer::updatePortalRays(fx dt) const {
    initPortalRays();

    static constexpr fx kWrap = fx::fromInt(18);

    for (int i = 0; i < kPortalRayCount; ++i) {
        PortalRay& r = portalRays_[i];
        r.offset += r.speed * dt;

        if (r.offset > kWrap) {
            r.offset -= kWrap;
        }
    }
}

void Renderer::drawPortalRays(RenderList& rl, const Game& game, fx scrollX, uint16_t color) const {
    if (!game.hasLevel()) return;

    initPortalRays();

    const LevelHeaderV1& h = game.levelHeader();
    const int portalCol = portal_abs_x(h);
    if (portalCol < 0 || portalCol >= int(h.width)) return;

    const fx portalX = worldXForColumn(portalCol, scrollX) + fi(kCellSize / 2);

    int py = int(h.portalY);
    if (py < 0) py = 0;
    if (py >= kLevelHeight) py = kLevelHeight - 1;

    const fx portalY = worldYForRow(py) + fi(kCellSize / 2);
    const fx portalZ = fi(kCellSize / 2);

    const Vec3fx center{ portalX, portalY, portalZ };

    for (int i = 0; i < kPortalRayCount; ++i) {
        const PortalRay& r = portalRays_[i];

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

void Renderer::trailPushLevelPoint(fx levelX, fx y, fx z) const {
    // A large jump usually indicates a reset/spawn; clearing avoids a long diagonal streak.
    if (trailCount_ > 0) {
        int lastIdx = trailHead_ - 1;
        if (lastIdx < 0) lastIdx += kTrailMax;

        const TrailPt last = trail_[lastIdx];

        Vec2i a{}, b{};
        Vec3fx wa{ last.levelX - levelX + fx::fromInt(kShipFixedX), last.y, last.z };
        Vec3fx wb{ fx::fromInt(kShipFixedX), y, z };

        if (projectPoint(cam, wa, a) && projectPoint(cam, wb, b)) {
            const int dx = iabs(int(b.x) - int(a.x));
            const int dy = iabs(int(b.y) - int(a.y));
            if (dx + dy > 120) {
                trailCount_ = 0;
                trailHead_ = 0;
            }
        }
    }

    trail_[trailHead_] = TrailPt{ levelX, y, z };
    ++trailHead_;
    if (trailHead_ >= kTrailMax) trailHead_ = 0;

    if (trailCount_ < kTrailMax) ++trailCount_;
}

void Renderer::trailDraw(RenderList& rl, fx scrollX, uint16_t color) const {
    if (trailCount_ < 2) return;

    int start = trailHead_ - trailCount_;
    if (start < 0) start += kTrailMax;

    Vec2i prev{};
    bool havePrev = false;

    int idx = start;
    for (int i = 0; i < trailCount_; ++i) {
        const TrailPt tp = trail_[idx];

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

void Renderer::addShip(RenderList &rl, const Vec3fx &pos, uint16_t color, fx shipY, fx shipVy) const
{
    // Size is about half a cell wide.
    const fx halfW = fi(kCellSize/4);
    const fx len   = fi(kCellSize) * fx::fromRatio(9, 20);
    const fx hz    = fi(kCellSize) * fx::fromRatio(3, 50);

    // Local triangle in XY, pointing straight ahead (+X) when angle = 0.
    Vec3fx v0{  len,  fx::zero(), fx::zero() };       // tip
    Vec3fx v1{ -len,  halfW,      fx::zero() };       // base top
    Vec3fx v2{ -len, -halfW,      fx::zero() };       // base bottom

    // Clipping zone is within half-width of the playfield boundary.
    const fx playHalf = playHalfH();
    const fx clipZoneStart = playHalf - halfW;
    const fx absY = abs(shipY);
    const bool clipping = (absY > clipZoneStart);

    // Tilt angle points up/down only when not clipping.
    fx c = fx::one();
    fx s = fx::zero();

    if (!clipping) {
        constexpr fx cos45 = fx::fromRaw(46341); // ~0.7071 in Q16.16
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
        // Rotate in XY plane around Z: (x',y') = (x*c - y*s, x*s + y*c)
        fx x = p.x;
        fx y = p.y;
        p.x = x * c - y * s;
        p.y = x * s + y * c;
    };

    rotZ(v0); rotZ(v1); rotZ(v2);

    // Extrude in Z.
    Vec3fx a0{ v0.x, v0.y, v0.z - hz }, a1{ v1.x, v1.y, v1.z - hz }, a2{ v2.x, v2.y, v2.z - hz };
    Vec3fx b0{ v0.x, v0.y, v0.z + hz }, b1{ v1.x, v1.y, v1.z + hz }, b2{ v2.x, v2.y, v2.z + hz };

    auto add = [&](const Vec3fx& p) { return Vec3fx{ pos.x + p.x, pos.y + p.y, pos.z + p.z }; };

    // Wireframe edges.
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

void Renderer::addCube(RenderList &rl, const Vec3fx &pos, uint16_t color) const
{
    const Vec3fx verts[] = {
        { fx::zero(),    fx::zero(),    fx::zero()    }, { fi(kCellSize), fx::zero(),    fx::zero()    },
        { fi(kCellSize), fi(kCellSize), fx::zero()    }, { fx::zero(),    fi(kCellSize), fx::zero()    },
        { fx::zero(),    fx::zero(),    fi(kCellSize) }, { fi(kCellSize), fx::zero(),    fi(kCellSize) },
        { fi(kCellSize), fi(kCellSize), fi(kCellSize) }, { fx::zero(),    fi(kCellSize), fi(kCellSize) }
    };

    const int indices[] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    for (size_t i = 0; i < sizeof(indices)/sizeof(indices[0]); i += 2) {
        Vec3fx vA = add3(pos, verts[indices[i]]);
        Vec3fx vB = add3(pos, verts[indices[i+1]]);
        line3(vA, vB, color, cam, rl);
    }
}

void Renderer::addSquarePyramid(RenderList& rl, const Vec3fx& pos, uint16_t color,
                                ModId mod, fx apexScale, const Vec3fx& origin) const
{
    const Vec3fx verts[] = {
        { fi(kCellSize/2), apexScale*fi(kCellSize), fi(kCellSize/2) }, // apex
        { fx::zero(),      fx::zero(),              fi(kCellSize)   }, // base corner 0
        { fi(kCellSize),   fx::zero(),              fi(kCellSize)   }, // base corner 1
        { fi(kCellSize),   fx::zero(),              fx::zero()      }, // base corner 2
        { fx::zero(),      fx::zero(),              fx::zero()      }  // base corner 3
    };

    const int indices[] = {
        0,1, 0,2, 0,3, 0,4, // sides
        1,2, 2,3, 3,4, 4,1  // base
    };

    for (size_t i = 0; i < sizeof(indices)/sizeof(indices[0]); i += 2) {
        Vec3fx vA = add3(pos, verts[indices[i]]);
        Vec3fx vB = add3(pos, verts[indices[i+1]]);

        applyMod3(mod, origin, vA);
        applyMod3(mod, origin, vB);

        line3(vA, vB, color, cam, rl);
    }
}

void Renderer::addRightTriPrism(RenderList& rl, const Vec3fx& pos, uint16_t color,
                               ModId mod, const Vec3fx& origin) const
{
    // Right triangle prism with right angle at bottom-right.
    const Vec3fx verts[] = {
        { fi(kCellSize), fi(kCellSize), fx::zero()    }, // front-right-top
        { fi(kCellSize), fx::zero(),    fx::zero()    }, // front-right-bottom
        { fx::zero(),    fx::zero(),    fx::zero()    }, // front-left-bottom
        { fi(kCellSize), fi(kCellSize), fi(kCellSize) }, // back-right-top
        { fi(kCellSize), fx::zero(),    fi(kCellSize) }, // back-right-bottom
        { fx::zero(),    fx::zero(),    fi(kCellSize) }  // back-left-bottom
    };

    const int indices[] = {
        0,1, 1,2, 2,0, // front face
        3,4, 4,5, 5,3, // back face
        0,3, 1,4, 2,5  // connecting edges
    };

    for (size_t i = 0; i < sizeof(indices)/sizeof(indices[0]); i += 2) {
        Vec3fx vA = add3(pos, verts[indices[i]]);
        Vec3fx vB = add3(pos, verts[indices[i+1]]);

        applyMod3(mod, origin, vA);
        applyMod3(mod, origin, vB);

        line3(vA, vB, color, cam, rl);
    }
}

void Renderer::addText(RenderList& rl, const Text& text, int16_t x, int16_t y, uint16_t color, uint8_t alpha, bool inverted) const {
    if (!text.empty()) {
        rl.addText(&text, x, y, color, alpha, inverted);
    }
}

void Renderer::drawExplosion(RenderList& rl, uint16_t color) const {
    if (!explosionActive_) return;

    static constexpr fx kBurstTime = fx::fromRatio(3, 20); // 0.15 s
    const fx age = explosionAge_;

    for (int i = 0; i < kExplosionChunkCount; ++i) {
        const ExplosionChunk& ch = explosion_[i];
        const Vec3fx pos{
            ch.origin.x + ch.vel.x * age,
            ch.origin.y + ch.vel.y * age,
            ch.origin.z + ch.vel.z * age
        };

        // Brief burst streak from the ship position outward.
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

void Renderer::buildScene(RenderList& rl, const Game& game, fx scrollX) const
{
    const uint16_t kWire   = 0xFFFF; // white
    const uint16_t kGreen  = 0x07E0; // green
    const uint16_t kShip   = 0xFFFF; // white
    const uint16_t kCyan   = 0x07FF; // cyan
    const uint16_t kPurple = 0xF81F; // bright purple

    if (!game.hasLevel()) return;

    const int levelW = (int)game.levelHeader().width;

    int scrollCol = scrollX.toInt() / kCellSize;
    if (scrollCol < 0) scrollCol = 0;

    int col0 = scrollCol - kColsPadLeft;
    if (col0 < 0) col0 = 0;

    int col1 = col0 + kColsVisible;
    if (col1 > levelW) col1 = levelW;

    // ---- Bounds planes (top/bottom of playfield) ----
    const fx z0 = fx::zero();
    const fx z1 = fi(kCellSize);
    const fx yTop = playCenterY() + playHalfH();
    const fx yBot = playCenterY() - playHalfH();

    const fx xLeft  = fx::fromInt(col0 * kCellSize) - scrollX + fx::fromInt(kShipFixedX) - fi(kCellSize * 2);
    const fx xRight = fx::fromInt(col1 * kCellSize) - scrollX + fx::fromInt(kShipFixedX) + fi(kCellSize * 2);

    rectWireXZ(rl, cam, xLeft, xRight, yTop, z0, z1, kWire);
    rectWireXZ(rl, cam, xLeft, xRight, yBot, z0, z1, kWire);

    // ---- Stream + render level ----
    Column56 col{};
    for (int cx = col0; cx < col1; ++cx) {
        if (!game.readLevelColumn((uint16_t)cx, col))
            continue;

        fx worldX = worldXForColumn(cx, scrollX);

        for (int row = 0; row < kLevelHeight; ++row) {
            ShapeId sid = col.shape(row);
            if (sid == ShapeId::Empty) continue;

            ModId mid = col.mod(row);

            fx worldY = worldYForRow(row);
            fx cz = fx::zero();

            // Modifier origin uses per-cell center.
            fx ox = worldX + fi(kCellSize/2);
            fx oy = worldY + fi(kCellSize/2);

            switch (sid) {
                case ShapeId::Square:
                    addCube(rl, {worldX, worldY, cz}, kGreen);
                    break;

                case ShapeId::RightTri:
                    addRightTriPrism(rl, {worldX, worldY, cz}, kGreen, mid, {ox, oy, cz});
                    break;

                case ShapeId::HalfSpike:
                    addSquarePyramid(rl, {worldX, worldY, cz}, kGreen, mid, fx::half(), {ox, oy, cz});
                    break;

                case ShapeId::FullSpike:
                    addSquarePyramid(rl, {worldX, worldY, cz}, kGreen, mid, fx::one(), {ox, oy, cz});
                    break;

                default:
                    break;
            }
        }
    }

    // ---- Portal marker cubes ----
    const LevelHeaderV1& h = game.levelHeader();
    const int portalCol = portal_abs_x(h);
    const int py = (int)h.portalY;

    if (portalCol >= col0 && portalCol < col1) {
        updatePortalRays(dtCache_);
        const fx px = worldXForColumn(portalCol, scrollX);
        const fx cz = fx::zero();

        for (int dy = -1; dy <= 1; ++dy) {
            int row = py + dy;
            if (row < 0) row = 0;
            if (row > (kLevelHeight - 1)) row = (kLevelHeight - 1);

            const fx pyWorld = worldYForRow(row);
            addCube(rl, {px, pyWorld, cz}, kPurple);
        }
        drawPortalRays(rl, game, scrollX, kWire);
    }

        // ---- Ship / explosion + trail ----
    const Vec3fx shipPos{ game.shipRenderX(), game.ship().y, fi(kCellSize/2) };

    if (game.state() != RunState::Dead) {
        // Trail is drawn first so the ship sits on top.
        trailDraw(rl, scrollX, kCyan);

        // Trail samples are stored in level-space so they drift left as scrollX advances.
        // Ship level-space X is scrollX plus any fly-out offset.
        const fx shipLevelX = scrollX + (game.shipRenderX() - fx::fromInt(kShipFixedX));
        trailPushLevelPoint(shipLevelX, game.ship().y, fi(kCellSize/2));

        addShip(rl, shipPos, kShip, game.ship().y, game.ship().vy);
    } else {
        drawExplosion(rl, kShip);
    }
}

void Renderer::buildOverlay(RenderList& rl, const Game& game) const {
    static constexpr uint16_t kHud = 0xFFFF; // white

    const Hud& hud = game.hud();

    addText(rl, hud.controlsHint(), 4, 4, kHud);

    const int levelW = hud.levelLabel().width();
    if (levelW > 0) {
        addText(rl, hud.levelLabel(), int16_t((320 - levelW) / 2), 4, kHud);
    }

    const int progressW = hud.progress().width();

    // Top-right progress bar
    static constexpr int kBarW = 64;
    static constexpr int kBarH = 8;
    const int barX = 320 - 4 - progressW - 6 - kBarW;
    const int barY = 4;

    const int pct = game.progressPercent();
    int fillW = (pct * (kBarW - 2)) / 100;
    if (fillW < 0) fillW = 0;
    if (fillW > (kBarW - 2)) fillW = (kBarW - 2);

    // Outer rectangle
    rl.addLine((int16_t)barX,             (int16_t)barY,             (int16_t)(barX + kBarW - 1), (int16_t)barY,             kHud);
    rl.addLine((int16_t)(barX + kBarW - 1), (int16_t)barY,           (int16_t)(barX + kBarW - 1), (int16_t)(barY + kBarH - 1), kHud);
    rl.addLine((int16_t)(barX + kBarW - 1), (int16_t)(barY + kBarH - 1), (int16_t)barX,           (int16_t)(barY + kBarH - 1), kHud);
    rl.addLine((int16_t)barX,             (int16_t)(barY + kBarH - 1), (int16_t)barX,             (int16_t)barY,             kHud);

    // Filled portion
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
        addText(rl, hud.progress(), int16_t(320 - 4 - progressW), 4, kHud);
    }

    if (hud.eventVisible()) {
        const uint16_t eventColor = hud.eventColor();
        if (eventColor != 0) {
            addText(rl, hud.eventText(), 4, int16_t(320 - 12), eventColor);
        }
    }
}

} // namespace gv