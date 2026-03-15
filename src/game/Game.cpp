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

static inline gv::fx shipWorldZ() { return gv::fx::fromInt(gv::kCellSize / 2); }
static inline gv::fx shipRadius() { return gv::fx::fromInt(gv::kCellSize / 4); }

static inline gv::fx halfCellFx() { return gv::fx::fromInt(gv::kCellSize / 2); }

static inline void applyGroupOffsetToAnchor(gv::AnimGroupOffsetMod gm, gv::fx& anchorX, gv::fx& anchorY) {
    const gv::fx half = halfCellFx();

    if (gm == gv::AnimGroupOffsetMod::ShiftRight ||
        gm == gv::AnimGroupOffsetMod::ShiftDownRight) {
        anchorX = anchorX + half;
    }

    // World Y is positive upward, so "down" is negative Y.
    if (gm == gv::AnimGroupOffsetMod::ShiftDown ||
        gm == gv::AnimGroupOffsetMod::ShiftDownRight) {
        anchorY = anchorY - half;
    }
}

static inline void turnsToSinCos(gv::fx turns, gv::fx& c, gv::fx& s) {
    const double t = double(turns.raw()) / double(1 << gv::fx::SHIFT);
    const double a = t * 6.28318530717958647692;

    c = gv::fx::fromRaw(int32_t(std::lround(std::cos(a) * double(1 << gv::fx::SHIFT))));
    s = gv::fx::fromRaw(int32_t(std::lround(std::sin(a) * double(1 << gv::fx::SHIFT))));
}

// Inverse of the forward XY rotation used by SceneBuilder.
static inline void inverseRotatePointAround(gv::fx& x, gv::fx& y,
                                            gv::fx ox, gv::fx oy,
                                            gv::fx c, gv::fx s) {
    const gv::fx dx = x - ox;
    const gv::fx dy = y - oy;

    // Inverse rotation by angle A:
    // [ c  s]
    // [-s  c]
    const gv::fx ndx = dx * c + dy * s;
    const gv::fx ndy = -dx * s + dy * c;

    x = ox + ndx;
    y = oy + ndy;
}

// Conservative per-def search radius in half-cell units.
// We bound each primitive by its 1x1 cell box around its center,
// so corners are (hx±1, hy±1) in half-cell coordinates.
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

                // Include anchor->pivot offset implicitly by measuring from pivot-centered primitive geometry,
                // then add a small extra cell for safety.
                if (distCeil > maxRadiusHalfCells) {
                    maxRadiusHalfCells = distCeil;
                }
            }
        }
    }

    // half-cells -> cells, with a bit of extra slack
    return (maxRadiusHalfCells + 1) / 2 + 1;
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
}

void Game::updateAnimCache() {
    for (std::size_t defIndex = 0; defIndex < animGroupDefs_.size(); ++defIndex) {
        const LoadedAnimGroupDef& def = animGroupDefs_[defIndex];
        const uint16_t totalMs = def.hdr.totalDurationMs;

        if (totalMs == 0 || def.hdr.stepCount == 0) {
            animGroupAngleTurns_[defIndex] = fx::zero();
            continue;
        }

        const uint32_t localMs = animTimeMs_ % uint32_t(totalMs);

        fx angle = fx::zero();
        uint32_t walked = 0;

        for (uint8_t i = 0; i < def.hdr.stepCount; ++i) {
            const AnimStepDef& step = animSteps_[def.firstStep + i];
            const uint32_t dur = step.durationMs;

            if (localMs < (walked + dur)) {
                if (step.type == uint8_t(AnimStepType::Rotate)) {
                    const uint32_t stepMs = localMs - walked;
                    const fx u = fx::fromRatio(int(stepMs), int(dur));
                    const fx delta =
                        fx::fromInt(int(step.deltaQuarterTurns)) * fx::fromRatio(1, 4);
                    angle = angle + delta * u;
                }
                break;
            }

            if (step.type == uint8_t(AnimStepType::Rotate)) {
                angle = angle + fx::fromInt(int(step.deltaQuarterTurns)) * fx::fromRatio(1, 4);
            }

            walked += dur;
        }

        animGroupAngleTurns_[defIndex] = -angle;
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
            if (s.type != uint8_t(AnimStepType::Rotate) &&
                s.type != uint8_t(AnimStepType::Hold)) {
                return false;
            }

            if (s.durationMs == 0) {
                return false;
            }

            if (s.type == uint8_t(AnimStepType::Rotate) &&
                s.deltaQuarterTurns == 0) {
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
    if (levelHdr.version != 2) { unloadLevel(); return false; }
    if (levelHdr.width == 0) { unloadLevel(); return false; }
    if (levelHdr.animDefCount > kMaxAnimGroupDefs) { unloadLevel(); return false; }
    if (levelHdr.levelDataOffset < sizeof(LevelHeader)) { unloadLevel(); return false; }

    if (!loadAnimDefs()) {
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
    const fx sz = shipWorldZ();
    const fx r  = shipRadius();

    // ---- Static obstacle neighborhood ----
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

    // ---- Animated group neighborhood ----
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
            if (sid == ObstacleId::Empty) continue;

            // ---- Animated groups ----
            if (is_anim_group(sid)) {
                if (row < animRow0 || row > animRow1) {
                    continue;
                }

                const uint8_t defIndex = anim_group_index(sid);
                if (defIndex >= animGroupDefs_.size()) {
                    continue;
                }

                const LoadedAnimGroupDef& def = animGroupDefs_[defIndex];
                const AnimGroupOffsetMod gm = col.groupMod(row);

                fx anchorX = cellCenterX;
                fx anchorY = worldYForRow(row) + halfCellFx();
                applyGroupOffsetToAnchor(gm, anchorX, anchorY);

                const fx angleTurns = animGroupAngleTurns_[defIndex];
                fx gc = fx::one();
                fx gs = fx::zero();
                turnsToSinCos(angleTurns, gc, gs);

                const fx groupPivotX = anchorX + mulInt(halfCellFx(), int(def.hdr.pivotHx));
                const fx groupPivotY = anchorY - mulInt(halfCellFx(), int(def.hdr.pivotHy));

                // Move ship center into the group's unrotated space.
                fx sxLocal = sx;
                fx syLocal = sy;
                inverseRotatePointAround(sxLocal, syLocal, groupPivotX, groupPivotY, gc, gs);

                for (uint8_t i = 0; i < def.hdr.primitiveCount; ++i) {
                    const AnimPrimitiveDef& p = animPrimitives_[def.firstPrimitive + i];

                    const fx primCenterX = anchorX + mulInt(halfCellFx(), int(p.hx));
                    const fx primCenterY = anchorY - mulInt(halfCellFx(), int(p.hy));

                    const fx primOriginX = primCenterX - halfCellFx();
                    const fx primOriginY = primCenterY - halfCellFx();

                    const fx lx = sxLocal - primOriginX;
                    const fx ly = syLocal - primOriginY;

                    if (collideCell(p.obstacle, p.mod, lx, ly, sz, r)) {
                        return true;
                    }
                }

                continue;
            }

            // ---- Static primitives ----
            if (c < staticColA || c > staticColB) {
                continue;
            }
            if (row < staticRowA || row > staticRowB) {
                continue;
            }

            const ShapeMod mid = col.shapeMod(row);

            const fx rowY0 = worldYForRow(row);
            const fx ly = sy - rowY0;
            const fx lz = sz;

            if (collideCell(sid, mid, staticLx, ly, lz, r)) {
                return true;
            }
        }
    }

    return false;
}

bool Game::collideCell(ObstacleId sid, ShapeMod mid, fx lx, fx ly, fx lz, fx r) {
    const fx k = fx::fromInt(kCellSize);

    if (lx < -r || lx > k + r) return false;
    if (ly < -r || ly > k + r) return false;
    if (lz < -r || lz > k + r) return false;

    if (sid == ObstacleId::Square) {
        return true;
    }

    fx x = lx;
    fx y = ly;
    const fx ox = fx::fromInt(kCellSize / 2);
    const fx oy = fx::fromInt(kCellSize / 2);
    unapplyMod2(mid, ox, oy, x, y);

    const fx z = lz;

    if (sid == ObstacleId::RightTri) {
        if (z < -r || z > k + r) return false;
        return y <= (x + r);
    }

    if (sid == ObstacleId::FullSpike || sid == ObstacleId::HalfSpike) {
        const fx apexScale = (sid == ObstacleId::FullSpike) ? fx::one() : fx::half();
        const fx apexY = apexScale * k;

        if (apexY.raw() <= 0) return false;
        if (y < -r || y > apexY + r) return false;

        fx t = (apexY - y) / apexY;

        const fx half = fx::fromInt(kCellSize / 2);
        fx extent = half * t;

        const fx cx = half;
        const fx cz = half;

        fx dx = x - cx;
        fx dz = z - cz;

        if (dx < -(extent + r) || dx > (extent + r)) return false;
        if (dz < -(extent + r) || dz > (extent + r)) return false;

        return true;
    }

    return false;
}

} // namespace gv