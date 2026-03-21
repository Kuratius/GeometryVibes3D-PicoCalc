#include "Game.hpp"
#include "core/Config.hpp"
#include "game/Playfield.hpp"
#include "game/LevelMath.hpp"
#include "game/CollisionMath2D.hpp"
#include <array>
#include <cmath>
#include <cstdio>
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

} // anon

namespace gv {

uint8_t Game::progressPercent() const {
    if (!hasLevel()) return 0;

    const int width = int(levelHdr_.width);
    if (width <= 0) return 0;

    const int startCol = clampi(kStartColumn, 0, width - 1);

    int portalX = (width - 1) + int(levelHdr_.portalDx);
    if (portalX < 0) portalX = 0;
    if (portalX >= width) portalX = width - 1;

    const int denomCols = portalX - startCol;
    if (denomCols <= 0) return 100;

    const int denom = denomCols * kCellSize;

    int numer = xScroll_.toInt() - startCol * kCellSize;
    if (numer < 0) numer = 0;
    if (numer > denom) numer = denom;

    return static_cast<uint8_t>((numer * 100) / denom);
}

void Game::reset() {
    setDifficulty(difficulty_);

    shipState_.y  = fx::fromInt(0);
    shipState_.vy = fx::fromInt(0);

    runState_ = RunState::WaitingToStart;
    resumeState_ = RunState::WaitingToStart;
    flyOutX_ = fx::zero();
    hit_ = false;

    xScroll_ = fx::zero();
    finished_ = false;

    hud_.clear();
    hud_.setControlsHint(kControlsHintStart);
    unloadLevel();

    if (collisionHighlightEnabled_) {
        collisionDebugPrimitives_.clear();
    }
}

void Game::setDifficulty(Difficulty difficulty) {
    difficulty_ = difficulty;

    switch (difficulty_) {
        case Difficulty::Rookie:
            scrollSpeed_ = kScrollSpeedRookie;
            flyOutSpeed_ = kFlyOutSpeedRookie;
            break;

        case Difficulty::Pro:
            scrollSpeed_ = kScrollSpeedPro;
            flyOutSpeed_ = kFlyOutSpeedPro;
            break;

        case Difficulty::Expert:
            scrollSpeed_ = kScrollSpeedExpert;
            flyOutSpeed_ = kFlyOutSpeedExpert;
            break;

        default:
            difficulty_ = Difficulty::Rookie;
            scrollSpeed_ = kScrollSpeedRookie;
            flyOutSpeed_ = kFlyOutSpeedRookie;
            break;
    }
}

bool Game::loadLevel(const char* path) {
    unloadLevel();

    if (!fs_) return false;

    file_ = fs_->openRead(path);
    if (!file_) return false;

    size_t got = 0;
    if (!file_->seek(0)) { unloadLevel(); return false; }
    if (!file_->read(&levelHdr_, sizeof(LevelHeader), got) || got != sizeof(LevelHeader)) {
        unloadLevel();
        return false;
    }

    if (std::memcmp(levelHdr_.magic, "GVL", 3) != 0) { unloadLevel(); return false; }
    if (levelHdr_.version != 3) { unloadLevel(); return false; }
    if (levelHdr_.width == 0) { unloadLevel(); return false; }
    if (levelHdr_.animDefCount > kMaxAnimGroupDefs) { unloadLevel(); return false; }
    if (levelHdr_.levelDataOffset < sizeof(LevelHeader)) { unloadLevel(); return false; }

    if (!loadAnimDefs()) {
        unloadLevel();
        return false;
    }

    finished_ = false;
    hit_ = false;
    shipState_.vy = fx::zero();
    flyOutX_ = fx::zero();
    runState_ = RunState::WaitingToStart;
    resumeState_ = RunState::WaitingToStart;

    const int startRow = clampi(kStartRow, 0, kLevelHeight - 1);
    const int startCol = clampi(kStartColumn, 0, int(levelHdr_.width) - 1);

    const fx rowY0 = worldYForRow(startRow);
    shipState_.y = rowY0 + fx::fromInt(kCellSize / 2);

    xScroll_ = fx::fromInt(startCol * kCellSize);

    char buf[Text::CHAR_CAP + 1]{};
    std::snprintf(buf, sizeof(buf), "File loaded: %s", path ? path : "");
    hud_.setEvent(buf);
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
    std::memset(&levelHdr_, 0, sizeof(levelHdr_));
    clearAnimDefs();
    clearStars();

    if (collisionHighlightEnabled_) {
        collisionDebugPrimitives_.clear();
    }
}

bool Game::readLevelColumn(uint16_t i, Column56& out) const {
    if (!file_) return false;
    if (i >= levelHdr_.width) return false;

    const size_t offset = size_t(levelHdr_.levelDataOffset) + size_t(i) * size_t(kColumnBytes);
    if (!file_->seek(offset)) return false;

    size_t got = 0;
    if (!file_->read(out.b, kColumnBytes, got)) return false;
    return got == kColumnBytes;
}

void Game::update(const InputState& in, fx dt) {
    hud_.update(dt);
    hud_.setProgressPercent(progressPercent());

    if (runState_ == RunState::Paused) {
        shipState_.vy = fx::zero();
        return;
    }

    const fx halfH = playHalfH();

    if (runState_ == RunState::WaitingToStart) {
        shipState_.vy = fx::zero();
        shipState_.y = clamp(shipState_.y, -halfH, halfH);
        updateAnimCache();

        if (in.thrustPressed) {
            runState_ = RunState::Running;
            hud_.setControlsHint(kControlsHintPause);
        }
        return;
    }

    if (runState_ == RunState::FinishedFlyOut) {
        if (finished_) return;

        xScroll_ = xScroll_ + scrollSpeed_ * dt;
        flyOutX_ = flyOutX_ + flyOutSpeed_ * dt;

        const uint32_t dtMs = uint32_t(gv::mulInt(dt, 1000).roundToInt());
        animTimeMs_ += dtMs;
        updateAnimCache();

        if (flyOutX_ >= kFlyOutCells) {
            finished_ = true;
        }
        return;
    }

    const fx speedY = scrollSpeed_;
    shipState_.vy = in.thrust ? speedY : -speedY;
    shipState_.y = shipState_.y + shipState_.vy * dt;

    shipState_.y = clamp(shipState_.y, -halfH, halfH);

    if (runState_ == RunState::Dead) return;

    xScroll_ = xScroll_ + scrollSpeed_ * dt;

    const uint32_t dtMs = uint32_t(gv::mulInt(dt, 1000).roundToInt());
    animTimeMs_ += dtMs;
    updateAnimCache();

    if (checkPortalReached(shipState_.y)) {
        runState_ = RunState::FinishedFlyOut;
        shipState_.vy = fx::zero();
        return;
    }

    if (!hit_ && hasLevel()) {
        hit_ = checkCollisionAt(shipState_.y);
        if (hit_) {
            runState_ = RunState::Dead;
            hud_.setControlsHint(kControlsHintContinue);
            hud_.setEventMessage("Game Over!", false);
            finished_ = false;
            return;
        }
    }
}

void Game::pause() {
    if (runState_ == RunState::Paused) return;
    if (runState_ == RunState::WaitingToStart) return;

    resumeState_ = runState_;
    runState_ = RunState::Paused;
    shipState_.vy = fx::zero();

    hud_.setControlsHint(kControlsHintContinue);
    hud_.setEventMessage("Game Paused!", false);
}

void Game::resume() {
    if (runState_ != RunState::Paused) return;

    runState_ = resumeState_;
    hud_.setControlsHint(kControlsHintPause);
    hud_.clearEvent();
}

bool Game::loadAnimDefs() {
    clearAnimDefs();

    if (!file_) return false;
    if (levelHdr_.animDefCount == 0) return true;

    if (levelHdr_.animDefCount > kMaxAnimGroupDefs) {
        return false;
    }

    if (levelHdr_.levelDataOffset < sizeof(LevelHeader)) {
        return false;
    }

    std::array<AnimPrimitiveDef, kMaxAnimPrimitives> primTmp{};
    std::array<AnimStepDef, kMaxAnimSteps> stepTmp{};

    size_t defOffset = sizeof(LevelHeader);

    for (uint8_t defIndex = 0; defIndex < levelHdr_.animDefCount; ++defIndex) {
        if (!file_->seek(defOffset)) return false;

        AnimGroupDefHeader hdr{};
        size_t got = 0;
        if (!file_->read(&hdr, sizeof(hdr), got) || got != sizeof(hdr)) {
            return false;
        }

        const uint16_t defSize = anim_group_def_size_bytes(hdr);
        if (defSize < sizeof(AnimGroupDefHeader)) return false;
        if (defOffset + defSize > levelHdr_.levelDataOffset) return false;

        if (hdr.primitiveCount > levelHdr_.animDefMaxPrimitiveCount) return false;
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
    return defOffset == levelHdr_.levelDataOffset;
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

void Game::clearStars() {
    collectedStarsMask_ = 0;
    newlyCollectedStarsMask_ = 0;
}

bool Game::tryGetStarIndex(uint16_t targetCol, uint8_t targetRow, uint8_t& outIndex) const {
    if (!hasLevel()) return false;
    if (targetCol >= levelHdr_.width) return false;
    if (targetRow >= kLevelHeight) return false;

    uint8_t index = 0;
    Column56 col{};

    for (uint16_t c = 0; c <= targetCol; ++c) {
        if (!readLevelColumn(c, col)) {
            return false;
        }

        for (uint8_t row = 0; row < kLevelHeight; ++row) {
            if (col.obstacle(row) != ObstacleId::Star) {
                continue;
            }

            if (c == targetCol && row == targetRow) {
                outIndex = index;
                return true;
            }

            ++index;
        }
    }

    return false;
}

void Game::applyOffsetToAnchor(OffsetMod om, fx& anchorX, fx& anchorY) {
    ::applyGroupOffsetToAnchor(om, anchorX, anchorY);
}

bool Game::checkPortalReached(fx shipY) const {
    if (!hasLevel()) return false;

    const int width = int(levelHdr_.width);
    if (width <= 0) return false;

    const int portalX = (width - 1) + int(levelHdr_.portalDx);
    if (portalX < 0 || portalX >= width) return false;

    const int shipCol = xScroll_.toInt() / kCellSize;
    if (shipCol < portalX) return false;

    const int shipRow = rowFromWorldY(shipY);
    const int py = 4;

    return (shipRow >= py - 1) && (shipRow <= py + 1);
}

bool Game::starOverlapsShip(gv::fx shipX, gv::fx shipY, gv::fx centerX, gv::fx centerY) const {
    const gv::fx dx = shipX - centerX;
    const gv::fx dy = shipY - centerY;

    const gv::fx shipRadius = gv::fx::fromInt(gv::kCellSize / 4);
    const gv::fx starRadius = gv::fx::fromInt(gv::kCellSize / 2);
    const gv::fx sumRadius = shipRadius + starRadius;

    return (dx * dx + dy * dy) <= (sumRadius * sumRadius);
}

bool Game::checkAnimatedCollisionCell(const Column56& col, int row, fx sx, fx sy, fx cellCenterX) const {
    const ObstacleId sid = col.obstacle(row);
    if (!is_anim_group(sid)) {
        return false;
    }

    const uint8_t defIndex = anim_group_index(sid);
    if (defIndex >= animGroupDefs_.size()) {
        return false;
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

        if (collideCell(p.obstacle, p.mod, lx, ly, shipState_.vy)) {
            return true;
        }
    }

    return false;
}

bool Game::checkStaticCollisionCell(
    ObstacleId sid,
    ShapeMod mid,
    fx worldX,
    int row,
    fx staticLx,
    fx sy
) const {
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

    return collideCell(sid, mid, staticLx, ly, shipState_.vy);
}

bool Game::checkCollisionAt(fx shipY) {
    const fx sy = shipY;
    const fx sx = fx::fromInt(kShipFixedX);
    const fx r = fx::fromInt(kCellSize / 4);

    if (collisionHighlightEnabled_) {
        collisionDebugPrimitives_.clear();
    }

    const fx x0 = xScroll_ - r;
    const fx x1 = xScroll_ + r;

    int staticColA = x0.toInt() / kCellSize;
    int staticColB = x1.toInt() / kCellSize;

    if (staticColA < 0) staticColA = 0;
    if (staticColB < 0) staticColB = 0;

    const int maxCol = int(levelHdr_.width) - 1;
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
        if (!readLevelColumn((uint16_t)c, col)) {
            continue;
        }

        const fx staticLx = localXInColumn(xScroll_, c);
        const fx worldX = worldXForColumn(c, xScroll_);
        const fx cellCenterX = worldX + halfCellFx();

        for (int row = 0; row < kLevelHeight; ++row) {
            const ObstacleId sid = col.obstacle(row);
            if (sid == ObstacleId::Empty) {
                continue;
            }

            if (sid == ObstacleId::Star) {
                uint8_t starIndex = 0;
                if (!tryGetStarIndex(uint16_t(c), uint8_t(row), starIndex)) {
                    continue;
                }

                if (starCollected(starIndex)) {
                    continue;
                }

                if (c < staticColA || c > staticColB) {
                    continue;
                }
                if (row < staticRowA || row > staticRowB) {
                    continue;
                }

                fx centerX = worldXForColumn(c, xScroll_) + halfCellFx();
                fx centerY = worldYForRow(row) + halfCellFx();
                applyOffsetToAnchor(col.offsetMod(row), centerX, centerY);

                if (starOverlapsShip(sx, sy, centerX, centerY)) {
                    const uint8_t bit = uint8_t(1u << starIndex);
                    collectedStarsMask_ = uint8_t(collectedStarsMask_ | bit);
                    newlyCollectedStarsMask_ = uint8_t(newlyCollectedStarsMask_ | bit);
                    hud_.setEventMessage("Star collected!", false);
                }

                continue;
            }

            if (is_anim_group(sid)) {
                if (row < animRow0 || row > animRow1) {
                    continue;
                }

                if (checkAnimatedCollisionCell(col, row, sx, sy, cellCenterX)) {
                    return true;
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
            if (checkStaticCollisionCell(sid, mid, worldX, row, staticLx, sy)) {
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