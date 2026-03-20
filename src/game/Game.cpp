#include "Game.hpp"
#include "core/Config.hpp"
#include "game/Playfield.hpp"
#include "game/LevelMath.hpp"
#include <array>
#include <cmath>
#include <string>

namespace {

static constexpr const char* kControlsHintStart    = "START[SPACE]";
static constexpr const char* kControlsHintPause    = "PAUSE[ESC]";
static constexpr const char* kControlsHintContinue = "CONTINUE[SPACE]";

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline gv::fx halfCellFx() { return gv::fx::fromInt(gv::kCellSize / 2); }

static inline gv::fx q7ScaleToFx(uint8_t q7) {
    return gv::fx::fromRatio(int(q7), 128);
}

static inline void applyGroupOffsetToAnchor(gv::OffsetMod gm, gv::fx& anchorX, gv::fx& anchorY) {
    const gv::fx half = halfCellFx();

    if (gm == gv::OffsetMod::ShiftRight ||
        gm == gv::OffsetMod::ShiftDownRight) {
        anchorX = anchorX + half;
    }

    if (gm == gv::OffsetMod::ShiftDown ||
        gm == gv::OffsetMod::ShiftDownRight) {
        anchorY = anchorY - half;
    }
}

static inline void turnsToSinCos(gv::fx turns, gv::fx& c, gv::fx& s) {
    const double t = double(turns.raw()) / double(1 << gv::fx::SHIFT);
    const double a = t * 6.28318530717958647692;

    c = gv::fx::fromRaw(int32_t(std::lround(std::cos(a) * double(1 << gv::fx::SHIFT))));
    s = gv::fx::fromRaw(int32_t(std::lround(std::sin(a) * double(1 << gv::fx::SHIFT))));
}

static inline void inverseRotatePointAround(gv::fx& x, gv::fx& y,
                                            gv::fx ox, gv::fx oy,
                                            gv::fx c, gv::fx s) {
    const gv::fx dx = x - ox;
    const gv::fx dy = y - oy;

    const gv::fx ndx = dx * c + dy * s;
    const gv::fx ndy = -dx * s + dy * c;

    x = ox + ndx;
    y = oy + ndy;
}

static inline void inverseScalePointAround(gv::fx& x, gv::fx& y,
                                           gv::fx ox, gv::fx oy,
                                           gv::fx scale) {
    if (scale.raw() == 0) return;
    x = ox + (x - ox) / scale;
    y = oy + (y - oy) / scale;
}

static inline int computeAnimSearchPadCells(const gv::Game& game) {
    int maxRadiusHalfCells = 0;

    for (std::size_t defIndex = 0; defIndex < game.animGroupDefs().size(); ++defIndex) {
        const gv::LoadedAnimGroupDef& def = game.animGroupDefs()[defIndex];

        for (uint8_t i = 0; i < def.hdr.primitiveCount; ++i) {
            const gv::AnimPrimitiveDef& p = game.animPrimitives()[def.firstPrimitive + i];

            const int corners[4][2] = {
                { int(p.hx) - 1, int(p.hy) - 1 },
                { int(p.hx) + 1, int(p.hy) - 1 },
                { int(p.hx) - 1, int(p.hy) + 1 },
                { int(p.hx) + 1, int(p.hy) + 1 },
            };

            for (int k = 0; k < 4; ++k) {
                const int dx = corners[k][0] - int(def.hdr.pivotHx);
                const int dy = corners[k][1] - int(def.hdr.pivotHy);
                const double dist = std::sqrt(double(dx * dx + dy * dy));
                const int distCeil = int(std::ceil(dist));

                if (distCeil > maxRadiusHalfCells) {
                    maxRadiusHalfCells = distCeil;
                }
            }
        }
    }

    return (maxRadiusHalfCells + 1) / 2 + 1;
}

struct Pt2fx {
    gv::fx x{};
    gv::fx y{};
};

struct Poly2fx {
    Pt2fx v[4]{};
    int count = 0;
};

static inline gv::fx cross2(const Pt2fx& a, const Pt2fx& b, const Pt2fx& c) {
    const gv::fx abx = b.x - a.x;
    const gv::fx aby = b.y - a.y;
    const gv::fx acx = c.x - a.x;
    const gv::fx acy = c.y - a.y;
    return abx * acy - aby * acx;
}

static inline bool onSegment(const Pt2fx& a, const Pt2fx& b, const Pt2fx& p) {
    const gv::fx minX = gv::min(a.x, b.x);
    const gv::fx maxX = gv::max(a.x, b.x);
    const gv::fx minY = gv::min(a.y, b.y);
    const gv::fx maxY = gv::max(a.y, b.y);

    return (p.x >= minX && p.x <= maxX &&
            p.y >= minY && p.y <= maxY);
}

static inline int orientSign(const Pt2fx& a, const Pt2fx& b, const Pt2fx& c) {
    const gv::fx cr = cross2(a, b, c);
    if (cr.raw() > 0) return 1;
    if (cr.raw() < 0) return -1;
    return 0;
}

static inline bool segmentsIntersect(const Pt2fx& a, const Pt2fx& b,
                                     const Pt2fx& c, const Pt2fx& d) {
    const int o1 = orientSign(a, b, c);
    const int o2 = orientSign(a, b, d);
    const int o3 = orientSign(c, d, a);
    const int o4 = orientSign(c, d, b);

    if (o1 != o2 && o3 != o4) {
        return true;
    }

    if (o1 == 0 && onSegment(a, b, c)) return true;
    if (o2 == 0 && onSegment(a, b, d)) return true;
    if (o3 == 0 && onSegment(c, d, a)) return true;
    if (o4 == 0 && onSegment(c, d, b)) return true;

    return false;
}

static inline bool pointInConvexPoly(const Pt2fx& p, const Poly2fx& poly) {
    bool sawPos = false;
    bool sawNeg = false;

    for (int i = 0; i < poly.count; ++i) {
        const Pt2fx& a = poly.v[i];
        const Pt2fx& b = poly.v[(i + 1) % poly.count];
        const gv::fx cr = cross2(a, b, p);

        if (cr.raw() > 0) sawPos = true;
        if (cr.raw() < 0) sawNeg = true;

        if (sawPos && sawNeg) {
            return false;
        }
    }

    return true;
}

static inline bool convexPolysOverlap(const Poly2fx& a, const Poly2fx& b) {
    for (int i = 0; i < a.count; ++i) {
        const Pt2fx& a0 = a.v[i];
        const Pt2fx& a1 = a.v[(i + 1) % a.count];

        for (int j = 0; j < b.count; ++j) {
            const Pt2fx& b0 = b.v[j];
            const Pt2fx& b1 = b.v[(j + 1) % b.count];

            if (segmentsIntersect(a0, a1, b0, b1)) {
                return true;
            }
        }
    }

    if (pointInConvexPoly(a.v[0], b)) return true;
    if (pointInConvexPoly(b.v[0], a)) return true;

    return false;
}

static inline void rotatePointAround(Pt2fx& p, gv::fx ox, gv::fx oy, gv::fx c, gv::fx s) {
    const gv::fx dx = p.x - ox;
    const gv::fx dy = p.y - oy;
    p.x = ox + dx * c - dy * s;
    p.y = oy + dx * s + dy * c;
}

static inline void applyShapeModForward(Pt2fx& p, gv::ShapeMod mod, gv::fx ox, gv::fx oy) {
    gv::fx dx = p.x - ox;
    gv::fx dy = p.y - oy;

    switch (mod) {
        case gv::ShapeMod::None:
            break;

        case gv::ShapeMod::RotLeft: {
            gv::fx ndx = -dy;
            gv::fx ndy =  dx;
            dx = ndx;
            dy = ndy;
        } break;

        case gv::ShapeMod::RotRight: {
            gv::fx ndx =  dy;
            gv::fx ndy = -dx;
            dx = ndx;
            dy = ndy;
        } break;

        case gv::ShapeMod::Invert:
            dx = -dx;
            dy = -dy;
            break;
    }

    p.x = ox + dx;
    p.y = oy + dy;
}

static inline void buildPrimitivePoly2D(gv::ObstacleId sid, gv::ShapeMod mod, Poly2fx& poly) {
    using gv::fx;
    const fx k = fx::fromInt(gv::kCellSize);
    const fx half = fx::fromInt(gv::kCellSize / 2);

    poly.count = 0;

    switch (sid) {
        case gv::ObstacleId::Square:
            poly.count = 4;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { k,          k };
            poly.v[3] = { fx::zero(), k };
            break;

        case gv::ObstacleId::RightTri:
            poly.count = 3;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { k,          k };
            break;

        case gv::ObstacleId::HalfSpike:
            poly.count = 3;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { half,       half };
            break;

        case gv::ObstacleId::FullSpike:
            poly.count = 3;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { half,       k };
            break;

        default:
            return;
    }

    for (int i = 0; i < poly.count; ++i) {
        applyShapeModForward(poly.v[i], mod, half, half);
    }
}

static inline void buildShipPoly2D(gv::fx shipCenterX, gv::fx shipCenterY, gv::fx shipVy, Poly2fx& poly) {
    using gv::fx;

    const fx pad = fx::fromRatio(3, 2);
    const fx halfW = fx::fromInt(gv::kCellSize / 4) - pad;
    const fx halfLen   = fx::fromInt(gv::kCellSize) * fx::fromRatio(9, 20) - pad;

    poly.count = 3;

    poly.v[0] = { shipCenterX + halfLen, shipCenterY };
    poly.v[1] = { shipCenterX - halfLen, shipCenterY + halfW };
    poly.v[2] = { shipCenterX - halfLen, shipCenterY - halfW };

    gv::fx c = fx::one();
    gv::fx s = fx::zero();

    if (shipVy.raw() > 0) {
        constexpr fx cos45 = fx::fromRaw(46341);
        c = cos45;
        s = cos45;
    } else if (shipVy.raw() < 0) {
        constexpr fx cos45 = fx::fromRaw(46341);
        c = cos45;
        s = -cos45;
    }

    for (int i = 0; i < poly.count; ++i) {
        rotatePointAround(poly.v[i], shipCenterX, shipCenterY, c, s);
    }
}

} // anon

namespace gv {

int Game::progressPercent() const {
    if (!hasLevel()) return 0;

    const int width = int(levelHdr.width);
    if (width <= 0) return 0;

    const int startCol = clampi(kStartColumn, 0, width - 1);

    int portalX = (width - 1) + int(levelHdr.portalDx);
    if (portalX < 0) portalX = 0;
    if (portalX >= width) portalX = width - 1;

    const int denomCols = portalX - startCol;
    if (denomCols <= 0) return 100;

    const int denom = denomCols * kCellSize;

    int numer = xScroll.toInt() - startCol * kCellSize;
    if (numer < 0) numer = 0;
    if (numer > denom) numer = denom;

    return (numer * 100) / denom;
}

bool Game::checkPortalReached(fx shipY) const {
    if (!hasLevel()) return false;

    const int width = int(levelHdr.width);
    if (width <= 0) return false;

    const int portalX = (width - 1) + int(levelHdr.portalDx);
    if (portalX < 0 || portalX >= width) return false;

    const int shipCol = xScroll.toInt() / kCellSize;
    if (shipCol < portalX) return false;

    const int shipRow = rowFromWorldY(shipY);
    const int py = 4;

    return (shipRow >= py - 1) && (shipRow <= py + 1);
}

void Game::clearAnimDefs() {
    animGroupDefs_.clear();
    animPrimitives_.clear();
    animSteps_.clear();
    animTimeMs_ = 0;

    for (auto& a : animGroupAngleTurns_) {
        a = fx::zero();
    }
    for (auto& s : animGroupScale_) {
        s = fx::one();
    }
}

void Game::clearStars() {
    levelStars_.clear();
    collectedStarsMask_ = 0;
    newlyCollectedStarsMask_ = 0;
}

void Game::applyOffsetToAnchor(OffsetMod om, fx& anchorX, fx& anchorY) {
    ::applyGroupOffsetToAnchor(om, anchorX, anchorY);
}

bool Game::loadStarsFromLevel() {
    clearStars();
    if (!hasLevel()) return false;

    Column56 col{};
    for (uint16_t c = 0; c < levelHdr.width; ++c) {
        if (!readLevelColumn(c, col)) {
            return false;
        }

        for (int row = 0; row < kLevelHeight; ++row) {
            if (col.obstacle(row) != ObstacleId::Star) {
                continue;
            }

            if (levelStars_.size() >= kMaxLevelStars) {
                continue;
            }

            LevelStar star{};
            star.col = c;
            star.row = uint8_t(row);
            star.offset = col.offsetMod(row);
            star.index = uint8_t(levelStars_.size());

            if (!levelStars_.push_back(star)) {
                return false;
            }
        }
    }

    return true;
}

void Game::updateAnimCache() {
    for (std::size_t defIndex = 0; defIndex < animGroupDefs_.size(); ++defIndex) {
        const LoadedAnimGroupDef& def = animGroupDefs_[defIndex];
        const uint16_t totalMs = def.hdr.totalDurationMs;

        if (totalMs == 0 || def.hdr.stepCount == 0) {
            animGroupAngleTurns_[defIndex] = fx::zero();
            animGroupScale_[defIndex] = fx::fromRatio(int(def.hdr.baseScaleQ7), 128);
            continue;
        }

        const uint32_t localMs = animTimeMs_ % uint32_t(totalMs);

        fx angle = fx::zero();
        fx scale = fx::fromRatio(int(def.hdr.baseScaleQ7), 128);
        uint32_t walked = 0;

        for (uint8_t i = 0; i < def.hdr.stepCount; ++i) {
            const AnimStepDef& step = animSteps_[def.firstStep + i];
            const uint32_t dur = step.durationMs;
            const fx targetScale = q7ScaleToFx(step.targetScaleQ7);

            if (localMs < (walked + dur)) {
                const uint32_t stepMs = localMs - walked;
                const fx u = fx::fromRatio(int(stepMs), int(dur));

                const fx deltaAngle =
                    fx::fromInt(int(step.deltaQuarterTurns)) * fx::fromRatio(1, 4);

                angle = angle + deltaAngle * u;
                scale = lerp(scale, targetScale, u);
                break;
            }

            angle = angle + fx::fromInt(int(step.deltaQuarterTurns)) * fx::fromRatio(1, 4);
            scale = targetScale;
            walked += dur;
        }

        animGroupAngleTurns_[defIndex] = -angle;
        animGroupScale_[defIndex] = scale;
    }
}

void Game::reset() {
    shipState.y  = fx::fromInt(0);
    shipState.vy = fx::fromInt(0);

    runState = RunState::WaitingToStart;
    resumeState = RunState::WaitingToStart;
    flyOutX = fx::zero();
    hit = false;

    xScroll  = fx::zero();
    finished_ = false;

    hud_.clear();
    hud_.setControlsHint(kControlsHintStart);
    unloadLevel();

    if (collisionHighlightEnabled_) {
        collisionDebugPrimitives_.clear();
    }
}

bool Game::loadAnimDefs() {
    clearAnimDefs();

    if (!file_) return false;
    if (levelHdr.animDefCount == 0) return true;

    if (levelHdr.animDefCount > kMaxAnimGroupDefs) {
        return false;
    }

    if (levelHdr.levelDataOffset < sizeof(LevelHeader)) {
        return false;
    }

    std::array<AnimPrimitiveDef, kMaxAnimPrimitives> primTmp{};
    std::array<AnimStepDef, kMaxAnimSteps> stepTmp{};

    size_t defOffset = sizeof(LevelHeader);

    for (uint8_t defIndex = 0; defIndex < levelHdr.animDefCount; ++defIndex) {
        if (!file_->seek(defOffset)) return false;

        AnimGroupDefHeader hdr{};
        size_t got = 0;
        if (!file_->read(&hdr, sizeof(hdr), got) || got != sizeof(hdr)) {
            return false;
        }

        const uint16_t defSize = anim_group_def_size_bytes(hdr);
        if (defSize < sizeof(AnimGroupDefHeader)) return false;
        if (defOffset + defSize > levelHdr.levelDataOffset) return false;

        if (hdr.primitiveCount > levelHdr.animDefMaxPrimitiveCount) return false;
        if (hdr.primitiveCount > primTmp.size()) return false;
        if (hdr.stepCount > stepTmp.size()) return false;
        if (hdr.baseScaleQ7 == 0) return false;

        const size_t primOffset = defOffset + sizeof(AnimGroupDefHeader);
        const size_t primBytes = size_t(hdr.primitiveCount) * sizeof(AnimPrimitiveDef);

        if (hdr.primitiveCount > 0) {
            if (!file_->seek(primOffset)) return false;
            if (!file_->read(primTmp.data(), primBytes, got) || got != primBytes) {
                return false;
            }
        }

        const size_t stepOffset = primOffset + primBytes;
        const size_t stepBytes = size_t(hdr.stepCount) * sizeof(AnimStepDef);

        if (hdr.stepCount > 0) {
            if (!file_->seek(stepOffset)) return false;
            if (!file_->read(stepTmp.data(), stepBytes, got) || got != stepBytes) {
                return false;
            }
        }

        uint32_t computedDuration = 0;

        for (uint8_t i = 0; i < hdr.primitiveCount; ++i) {
            const AnimPrimitiveDef& p = primTmp[i];
            if (!is_anim_primitive_obstacle(p.obstacle)) {
                return false;
            }
        }

        for (uint8_t i = 0; i < hdr.stepCount; ++i) {
            const AnimStepDef& s = stepTmp[i];

            if (s.durationMs == 0) {
                return false;
            }
            if (s.targetScaleQ7 == 0) {
                return false;
            }

            computedDuration += s.durationMs;
        }

        if (animPrimitives_.size() + hdr.primitiveCount > kMaxAnimPrimitives) {
            return false;
        }
        if (animSteps_.size() + hdr.stepCount > kMaxAnimSteps) {
            return false;
        }

        LoadedAnimGroupDef loaded{};
        loaded.hdr = hdr;
        loaded.firstPrimitive = uint16_t(animPrimitives_.size());
        loaded.firstStep = uint16_t(animSteps_.size());

        if (loaded.hdr.totalDurationMs == 0) {
            loaded.hdr.totalDurationMs =
                (computedDuration > 0xFFFFu) ? 0xFFFFu : uint16_t(computedDuration);
        }

        if (!animGroupDefs_.push_back(loaded)) {
            return false;
        }

        for (uint8_t i = 0; i < hdr.primitiveCount; ++i) {
            if (!animPrimitives_.push_back(primTmp[i])) {
                return false;
            }
        }

        for (uint8_t i = 0; i < hdr.stepCount; ++i) {
            if (!animSteps_.push_back(stepTmp[i])) {
                return false;
            }
        }

        defOffset += defSize;
    }

    updateAnimCache();
    return defOffset == levelHdr.levelDataOffset;
}

bool Game::loadLevel(const char* path) {
    unloadLevel();

    if (!fs_) return false;

    file_ = fs_->openRead(path);
    if (!file_) return false;

    size_t got = 0;
    if (!file_->seek(0)) { unloadLevel(); return false; }
    if (!file_->read(&levelHdr, sizeof(LevelHeader), got) || got != sizeof(LevelHeader)) {
        unloadLevel();
        return false;
    }

    if (std::memcmp(levelHdr.magic, "GVL", 3) != 0) { unloadLevel(); return false; }
    if (levelHdr.version != 3) { unloadLevel(); return false; }
    if (levelHdr.width == 0) { unloadLevel(); return false; }
    if (levelHdr.animDefCount > kMaxAnimGroupDefs) { unloadLevel(); return false; }
    if (levelHdr.levelDataOffset < sizeof(LevelHeader)) { unloadLevel(); return false; }

    if (!loadAnimDefs()) {
        unloadLevel();
        return false;
    }

    if (!loadStarsFromLevel()) {
        unloadLevel();
        return false;
    }

    finished_ = false;
    hit = false;
    shipState.vy = fx::zero();
    flyOutX = fx::zero();
    runState = RunState::WaitingToStart;
    resumeState = RunState::WaitingToStart;

    const int startRow = clampi(kStartRow, 0, kLevelHeight - 1);
    const int startCol = clampi(kStartColumn, 0, int(levelHdr.width) - 1);

    const fx rowY0 = worldYForRow(startRow);
    shipState.y = rowY0 + fx::fromInt(kCellSize / 2);

    xScroll = fx::fromInt(startCol * kCellSize);

    hud_.setEvent((std::string("File loaded: ") + path).c_str());
    hud_.setLevelLabel(path);
    hud_.setProgressPercent(0);
    hud_.setControlsHint(kControlsHintStart);

    return true;
}

void Game::unloadLevel() {
    if (file_) {
        file_->close();
        file_ = nullptr;
    }
    std::memset(&levelHdr, 0, sizeof(levelHdr));
    clearAnimDefs();
    clearStars();

    if (collisionHighlightEnabled_) {
        collisionDebugPrimitives_.clear();
    }
}

bool Game::readLevelColumn(uint16_t i, Column56& out) const {
    if (!file_) return false;
    if (i >= levelHdr.width) return false;

    const size_t offset = size_t(levelHdr.levelDataOffset) + size_t(i) * size_t(kColumnBytes);
    if (!file_->seek(offset)) return false;

    size_t got = 0;
    if (!file_->read(out.b, kColumnBytes, got)) return false;
    return got == kColumnBytes;
}

void Game::checkStarPickup() {
    if (runState == RunState::Dead) return;
    if (!hasLevel()) return;
    if (levelStars_.empty()) return;

    const fx shipX = fx::fromInt(kShipFixedX);
    const fx shipY = shipState.y;
    const fx shipHalfW = fx::fromInt(kCellSize / 4);
    const fx shipHalfH = fx::fromInt(kCellSize / 4);

    static constexpr uint32_t kStarSpinPeriodMs = 2400;

    for (const auto& star : levelStars_) {
        if (starCollected(star.index)) {
            continue;
        }

        fx centerX = worldXForColumn(star.col, xScroll) + halfCellFx();
        fx centerY = worldYForRow(star.row) + halfCellFx();
        applyOffsetToAnchor(star.offset, centerX, centerY);

        const uint32_t phaseMs =
            (animTimeMs_ + uint32_t(star.index) * (kStarSpinPeriodMs / 3)) % kStarSpinPeriodMs;
        const fx turns = fx::fromRatio(int(phaseMs), int(kStarSpinPeriodMs));

        fx c = fx::one();
        fx s = fx::zero();
        turnsToSinCos(turns, c, s);

        const fx halfH = fx::fromRatio(kCellSize * 4, 10);
        fx halfW = halfH * abs(c);
        if (halfW < fx::fromRatio(1, 4)) {
            halfW = fx::fromRatio(1, 4);
        }

        const fx dx = abs(shipX - centerX);
        const fx dy = abs(shipY - centerY);

        if (dx <= (shipHalfW + halfW) && dy <= (shipHalfH + halfH)) {
            const uint8_t bit = uint8_t(1u << star.index);
            collectedStarsMask_ = uint8_t(collectedStarsMask_ | bit);
            newlyCollectedStarsMask_ = uint8_t(newlyCollectedStarsMask_ | bit);
            hud_.setEventMessage("Star collected!", false);
        }
    }
}

void Game::update(const InputState& in, fx dt) {
    hud_.update(dt);
    hud_.setProgressPercent(progressPercent());

    if (runState == RunState::Paused) {
        shipState.vy = fx::zero();
        return;
    }

    const fx halfH = playHalfH();

    if (runState == RunState::WaitingToStart) {
        shipState.vy = fx::zero();
        shipState.y = clamp(shipState.y, -halfH, halfH);
        updateAnimCache();

        if (in.thrustPressed) {
            runState = RunState::Running;
            hud_.setControlsHint(kControlsHintPause);
        }
        return;
    }

    if (runState == RunState::FinishedFlyOut) {
        if (finished_) return;

        xScroll = xScroll + kScrollSpeed * dt;
        flyOutX = flyOutX + kFlyOutSpeed * dt;

        const uint32_t dtMs = uint32_t(gv::mulInt(dt, 1000).roundToInt());
        animTimeMs_ += dtMs;
        updateAnimCache();

        checkStarPickup();

        if (flyOutX >= kFlyOutCells) {
            finished_ = true;
        }
        return;
    }

    const fx speedY = kScrollSpeed;
    shipState.vy = in.thrust ? speedY : -speedY;
    shipState.y = shipState.y + shipState.vy * dt;

    shipState.y = clamp(shipState.y, -halfH, halfH);

    if (runState == RunState::Dead) return;

    xScroll = xScroll + kScrollSpeed * dt;

    const uint32_t dtMs = uint32_t(gv::mulInt(dt, 1000).roundToInt());
    animTimeMs_ += dtMs;
    updateAnimCache();

    checkStarPickup();

    if (checkPortalReached(shipState.y)) {
        runState = RunState::FinishedFlyOut;
        shipState.vy = fx::zero();
        return;
    }

    if (!hit && hasLevel()) {
        hit = checkCollisionAt(shipState.y);
        if (hit) {
            runState = RunState::Dead;
            hud_.setControlsHint(kControlsHintContinue);
            hud_.setEventMessage("Game Over!", false);
            finished_ = false;
            return;
        }
    }
}

void Game::pause() {
    if (runState == RunState::Paused) return;
    if (runState == RunState::WaitingToStart) return;

    resumeState = runState;
    runState = RunState::Paused;
    shipState.vy = fx::zero();

    hud_.setControlsHint(kControlsHintContinue);
    hud_.setEventMessage("Game Paused!", false);
}

void Game::resume() {
    if (runState != RunState::Paused) return;

    runState = resumeState;
    hud_.setControlsHint(kControlsHintPause);
    hud_.clearEvent();
}

bool Game::checkCollisionAt(fx shipY) const {
    const fx sy = shipY;
    const fx sx = fx::fromInt(kShipFixedX);
    const fx r = fx::fromInt(kCellSize / 4);

    if (collisionHighlightEnabled_) {
        collisionDebugPrimitives_.clear();
    }

    const fx x0 = xScroll - r;
    const fx x1 = xScroll + r;

    int staticColA = x0.toInt() / kCellSize;
    int staticColB = x1.toInt() / kCellSize;

    if (staticColA < 0) staticColA = 0;
    if (staticColB < 0) staticColB = 0;

    const int maxCol = int(levelHdr.width) - 1;
    if (staticColA > maxCol) staticColA = maxCol;
    if (staticColB > maxCol) staticColB = maxCol;

    const fx yLow  = sy - r;
    const fx yHigh = sy + r;

    int staticRowA = rowFromWorldY(yHigh);
    int staticRowB = rowFromWorldY(yLow);
    if (staticRowA > staticRowB) {
        const int t = staticRowA;
        staticRowA = staticRowB;
        staticRowB = t;
    }

    const int animPadCells = computeAnimSearchPadCells(*this);

    int animCol0 = staticColA - animPadCells;
    int animCol1 = staticColB + animPadCells;
    if (animCol0 < 0) animCol0 = 0;
    if (animCol1 > maxCol) animCol1 = maxCol;

    int animRow0 = staticRowA - animPadCells;
    int animRow1 = staticRowB + animPadCells;
    if (animRow0 < 0) animRow0 = 0;
    if (animRow1 > (kLevelHeight - 1)) animRow1 = (kLevelHeight - 1);

    Column56 col{};
    for (int c = animCol0; c <= animCol1; ++c) {
        if (!readLevelColumn((uint16_t)c, col)) continue;

        const fx staticLx = localXInColumn(xScroll, c);
        const fx worldX = worldXForColumn(c, xScroll);
        const fx cellCenterX = worldX + halfCellFx();

        for (int row = 0; row < kLevelHeight; ++row) {
            const ObstacleId sid = col.obstacle(row);
            if (sid == ObstacleId::Empty || sid == ObstacleId::Star) continue;

            if (is_anim_group(sid)) {
                if (row < animRow0 || row > animRow1) {
                    continue;
                }

                const uint8_t defIndex = anim_group_index(sid);
                if (defIndex >= animGroupDefs_.size()) {
                    continue;
                }

                const LoadedAnimGroupDef& def = animGroupDefs_[defIndex];
                const OffsetMod gm = col.offsetMod(row);

                fx anchorX = cellCenterX;
                fx anchorY = worldYForRow(row) + halfCellFx();
                applyGroupOffsetToAnchor(gm, anchorX, anchorY);

                const fx angleTurns = animGroupAngleTurns_[defIndex];
                const fx groupScale = animGroupScale_[defIndex];

                fx gc = fx::one();
                fx gs = fx::zero();
                turnsToSinCos(angleTurns, gc, gs);

                const fx groupPivotX = anchorX + mulInt(halfCellFx(), int(def.hdr.pivotHx));
                const fx groupPivotY = anchorY - mulInt(halfCellFx(), int(def.hdr.pivotHy));

                fx sxLocal = sx;
                fx syLocal = sy;
                inverseRotatePointAround(sxLocal, syLocal, groupPivotX, groupPivotY, gc, gs);
                inverseScalePointAround(sxLocal, syLocal, groupPivotX, groupPivotY, groupScale);

                for (uint8_t i = 0; i < def.hdr.primitiveCount; ++i) {
                    const AnimPrimitiveDef& p = animPrimitives_[def.firstPrimitive + i];

                    if (p.obstacle == ObstacleId::Star) {
                        continue;
                    }

                    const fx primCenterX = anchorX + mulInt(halfCellFx(), int(p.hx));
                    const fx primCenterY = anchorY - mulInt(halfCellFx(), int(p.hy));

                    const fx primOriginX = primCenterX - halfCellFx();
                    const fx primOriginY = primCenterY - halfCellFx();

                    if (collisionHighlightEnabled_) {
                        (void)collisionDebugPrimitives_.push_back(CollisionDebugPrimitive{
                            p.obstacle,
                            p.mod,
                            primOriginX,
                            primOriginY,
                            fx::zero(),
                            primCenterX,
                            primCenterY,
                            fx::fromInt(kCellSize / 2),
                            true,
                            groupPivotX,
                            groupPivotY,
                            fx::fromInt(kCellSize / 2),
                            gc,
                            gs,
                            groupScale
                        });
                    }

                    const fx lx = sxLocal - primOriginX;
                    const fx ly = syLocal - primOriginY;

                    if (collideCell(p.obstacle, p.mod, lx, ly, shipState.vy)) {
                        return true;
                    }
                }

                continue;
            }

            if (c < staticColA || c > staticColB) {
                continue;
            }
            if (row < staticRowA || row > staticRowB) {
                continue;
            }

            const ShapeMod mid = col.shapeMod(row);

            const fx rowY0 = worldYForRow(row);
            const fx ly = sy - rowY0;

            if (collisionHighlightEnabled_) {
                (void)collisionDebugPrimitives_.push_back(CollisionDebugPrimitive{
                    sid,
                    mid,
                    worldX,
                    rowY0,
                    fx::zero(),
                    worldX + halfCellFx(),
                    rowY0 + halfCellFx(),
                    fx::fromInt(kCellSize / 2),
                    false,
                    fx::zero(),
                    fx::zero(),
                    fx::zero(),
                    fx::one(),
                    fx::zero(),
                    fx::one()
                });
            }

            if (collideCell(sid, mid, staticLx, ly, shipState.vy)) {
                return true;
            }
        }
    }

    return false;
}

bool Game::collideCell(ObstacleId sid, ShapeMod mid, fx lx, fx ly, fx shipVy) {
    Poly2fx primitive{};
    buildPrimitivePoly2D(sid, mid, primitive);
    if (primitive.count < 3) {
        return false;
    }

    Poly2fx ship{};
    buildShipPoly2D(lx, ly, shipVy, ship);

    return convexPolysOverlap(ship, primitive);
}

} // namespace gv