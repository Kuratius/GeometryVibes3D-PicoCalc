#pragma once

#include <array>
#include <cstdint>

#include "core/Fixed.hpp"
#include "core/StaticVector.hpp"
#include "game/InputState.hpp"
#include "game/Level.hpp"
#include "platform/IFileSystem.hpp"
#include "Hud.hpp"

namespace gv {

struct ShipState {
    fx y{};
    fx vy{};
};

enum class RunState : uint8_t {
    WaitingToStart,
    Running,
    Paused,
    FinishedFlyOut,
    Dead
};

struct CollisionDebugPrimitive {
    ObstacleId sid = ObstacleId::Empty;
    ShapeMod mod = ShapeMod::None;

    fx worldOriginX{};
    fx worldOriginY{};
    fx worldOriginZ{};

    fx primitiveCenterX{};
    fx primitiveCenterY{};
    fx primitiveCenterZ{};

    bool animated = false;

    fx groupPivotX{};
    fx groupPivotY{};
    fx groupPivotZ{};

    fx groupCos = fx::one();
    fx groupSin = fx::zero();
    fx groupScale = fx::one();
};

struct LoadedAnimGroupDef {
    AnimGroupDefHeader hdr{};
    uint16_t firstPrimitive = 0;
    uint16_t firstStep = 0;
};

struct LevelStar {
    uint16_t col = 0;
    uint8_t row = 0;
    OffsetMod offset = OffsetMod::None;
    uint8_t index = 0;
};

class Game {
public:
    // Animation / content caps
    static constexpr std::size_t kMaxAnimGroupDefs = 8;
    static constexpr std::size_t kMaxAnimPrimitives = 64;
    static constexpr std::size_t kMaxAnimSteps = 128;
    static constexpr std::size_t kMaxLevelStars = 3;

    // Debug caps
    static constexpr std::size_t kCollisionDebugCap = 128;

public:
    // Lifecycle
    void reset();
    bool loadLevel(const char* path);
    void unloadLevel();

    // Dependencies / external setup
    void setFileSystem(IFileSystem* fs) { fs_ = fs; }
    void setLevelIndex(std::size_t levelIndex) { levelIndex_ = levelIndex; }
    void setCollectedStarsMask(uint8_t mask) { collectedStarsMask_ = uint8_t(mask & 0x07u); }

    // Level state / data access
    bool hasLevel() const { return file_ != nullptr; }
    const LevelHeader& levelHeader() const { return levelHdr_; }
    bool readLevelColumn(uint16_t i, Column56& out) const;

    // Simulation
    void update(const InputState& in, fx dt);
    void pause();
    void resume();

    // Run state
    RunState state() const { return runState_; }
    bool paused() const { return runState_ == RunState::Paused; }
    bool finished() const { return runState_ == RunState::FinishedFlyOut; }
    bool finishedScroll() const { return finished_; }

    // Ship / scrolling
    const ShipState& ship() const { return shipState_; }
    fx shipRenderX() const { return fx::fromInt(40) + flyOutX_; }
    fx scrollX() const { return xScroll_; }

    // Collision state / debug
    bool collided() const { return hit_; }
    void clearCollision() { hit_ = false; }

    bool collisionHighlightEnabled() const { return collisionHighlightEnabled_; }
    void setCollisionHighlightEnabled(bool enabled) { collisionHighlightEnabled_ = enabled; }

    const StaticVector<CollisionDebugPrimitive, kCollisionDebugCap>&
    collisionDebugPrimitives() const {
        return collisionDebugPrimitives_;
    }

    void clearCollisionDebugPrimitives() const {
        collisionDebugPrimitives_.clear();
    }

    // Progress / HUD
    uint8_t progressPercent() const;
    Hud& hud() { return hud_; }
    const Hud& hud() const { return hud_; }

    // Animation data / cache
    const StaticVector<LoadedAnimGroupDef, kMaxAnimGroupDefs>& animGroupDefs() const {
        return animGroupDefs_;
    }

    const StaticVector<AnimPrimitiveDef, kMaxAnimPrimitives>& animPrimitives() const {
        return animPrimitives_;
    }

    const StaticVector<AnimStepDef, kMaxAnimSteps>& animSteps() const {
        return animSteps_;
    }

    uint32_t animTimeMs() const { return animTimeMs_; }

    fx animGroupAngleTurns(uint8_t defIndex) const {
        return (defIndex < animGroupDefs_.size())
            ? animGroupAngleTurns_[defIndex]
            : fx::zero();
    }

    fx animGroupScale(uint8_t defIndex) const {
        return (defIndex < animGroupDefs_.size())
            ? animGroupScale_[defIndex]
            : fx::one();
    }

    // Level progression / stars
    std::size_t levelIndex() const { return levelIndex_; }

    std::size_t starCount() const { return levelStars_.size(); }
    const StaticVector<LevelStar, kMaxLevelStars>& levelStars() const {
        return levelStars_;
    }

    uint8_t collectedStarsMask() const { return collectedStarsMask_; }
    uint8_t newlyCollectedStarsMask() const { return newlyCollectedStarsMask_; }
    void clearNewlyCollectedStars() { newlyCollectedStarsMask_ = 0; }

    bool starCollected(uint8_t starIndex) const {
        return starIndex < 3 && ((collectedStarsMask_ >> starIndex) & 1u) != 0;
    }

private:
    // Animation loading / cache
    bool loadAnimDefs();
    void clearAnimDefs();
    void updateAnimCache();

    // Star loading / collection
    void clearStars();
    bool loadStarsFromLevel();
    static void applyOffsetToAnchor(OffsetMod om, fx& anchorX, fx& anchorY);
    void checkStarPickup();

    // Collision / gameplay tests
    bool checkPortalReached(fx shipY) const;
    bool checkCollisionAt(fx shipY) const;

    bool checkAnimatedCollisionCell(const Column56& col, int row, fx sx, fx sy, fx cellCenterX) const;
    bool checkStaticCollisionCell(ObstacleId sid, ShapeMod mid, fx worldX, int row, fx staticLx, fx sy) const;

    static bool collideCell(ObstacleId sid, ShapeMod mid, fx lx, fx ly, fx shipVy);

private:
    // Runtime simulation state
    ShipState shipState_{};
    RunState runState_ = RunState::WaitingToStart;
    RunState resumeState_ = RunState::WaitingToStart;
    fx xScroll_{};
    fx flyOutX_{};
    bool finished_ = false;
    bool hit_ = false;

    // External resources / loaded level
    IFileSystem* fs_ = nullptr;
    IFile* file_ = nullptr;
    LevelHeader levelHdr_{};
    std::size_t levelIndex_ = 0;

    // HUD
    Hud hud_{};

    // Animation content and cache
    StaticVector<LoadedAnimGroupDef, kMaxAnimGroupDefs> animGroupDefs_{};
    StaticVector<AnimPrimitiveDef, kMaxAnimPrimitives> animPrimitives_{};
    StaticVector<AnimStepDef, kMaxAnimSteps> animSteps_{};

    uint32_t animTimeMs_ = 0;
    std::array<fx, kMaxAnimGroupDefs> animGroupAngleTurns_{};
    std::array<fx, kMaxAnimGroupDefs> animGroupScale_{};

    // Star progression
    StaticVector<LevelStar, kMaxLevelStars> levelStars_{};
    uint8_t collectedStarsMask_ = 0;
    uint8_t newlyCollectedStarsMask_ = 0;

    // Collision debug
    bool collisionHighlightEnabled_ = false;
    mutable StaticVector<CollisionDebugPrimitive, kCollisionDebugCap>
        collisionDebugPrimitives_{};
};

} // namespace gv