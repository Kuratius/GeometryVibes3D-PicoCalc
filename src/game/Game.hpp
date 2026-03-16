#pragma once
#include <cstdint>
#include <array>
#include "core/Fixed.hpp"
#include "core/StaticVector.hpp"
#include "game/Level.hpp"
#include "game/InputState.hpp"
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

#ifdef GV3D_TESTING
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
#endif

struct LoadedAnimGroupDef {
    AnimGroupDefHeader hdr{};
    uint16_t firstPrimitive = 0;
    uint16_t firstStep = 0;
};

class Game {
public:
    static constexpr std::size_t kMaxAnimGroupDefs = 8;
    static constexpr std::size_t kMaxAnimPrimitives = 64;
    static constexpr std::size_t kMaxAnimSteps = 128;

public:
    void reset();

    void setFileSystem(IFileSystem* fs) { fs_ = fs; }

    bool loadLevel(const char* path);
    void unloadLevel();
    bool hasLevel() const { return file_ != nullptr; }
    const LevelHeader& levelHeader() const { return levelHdr; }

    bool readLevelColumn(uint16_t i, Column56& out) const;

    void update(const InputState& in, fx dt);

    void pause();
    void resume();

    const ShipState& ship() const { return shipState; }

    RunState state() const { return runState; }
    bool paused() const { return runState == RunState::Paused; }
    bool finished() const { return runState == RunState::FinishedFlyOut; }

    fx shipRenderX() const { return fx::fromInt(40) + flyOutX; }
    fx scrollX() const { return xScroll; }
    bool finishedScroll() const { return finished_; }

    bool collided() const { return hit; }
    void clearCollision() { hit = false; }

#ifdef GV3D_TESTING
    static constexpr std::size_t kCollisionDebugCap = 128;

    const StaticVector<CollisionDebugPrimitive, kCollisionDebugCap>& collisionDebugPrimitives() const {
        return collisionDebugPrimitives_;
    }

    void clearCollisionDebugPrimitives() const {
        collisionDebugPrimitives_.clear();
    }
#endif

    int progressPercent() const;

    Hud& hud() { return hud_; }
    const Hud& hud() const { return hud_; }

    const StaticVector<LoadedAnimGroupDef, kMaxAnimGroupDefs>& animGroupDefs() const { return animGroupDefs_; }
    const StaticVector<AnimPrimitiveDef, kMaxAnimPrimitives>& animPrimitives() const { return animPrimitives_; }
    const StaticVector<AnimStepDef, kMaxAnimSteps>& animSteps() const { return animSteps_; }

    uint32_t animTimeMs() const { return animTimeMs_; }

    fx animGroupAngleTurns(uint8_t defIndex) const {
        return (defIndex < animGroupDefs_.size()) ? animGroupAngleTurns_[defIndex] : fx::zero();
    }

    fx animGroupScale(uint8_t defIndex) const {
        return (defIndex < animGroupDefs_.size()) ? animGroupScale_[defIndex] : fx::one();
    }

private:
    bool loadAnimDefs();
    void clearAnimDefs();
    void updateAnimCache();

    bool checkPortalReached(fx shipY) const;
    bool checkCollisionAt(fx shipY) const;
    static bool collideCell(ObstacleId sid, ShapeMod mid, fx lx, fx ly, fx shipVy);

private:
    ShipState shipState{};
    RunState runState = RunState::WaitingToStart;
    RunState resumeState = RunState::WaitingToStart;
    fx xScroll{};
    fx flyOutX{};
    bool finished_ = false;
    bool hit = false;

    IFileSystem* fs_ = nullptr;
    IFile* file_ = nullptr;
    LevelHeader levelHdr{};
    Hud hud_{};

    StaticVector<LoadedAnimGroupDef, kMaxAnimGroupDefs> animGroupDefs_{};
    StaticVector<AnimPrimitiveDef, kMaxAnimPrimitives> animPrimitives_{};
    StaticVector<AnimStepDef, kMaxAnimSteps> animSteps_{};

    uint32_t animTimeMs_ = 0;
    std::array<fx, kMaxAnimGroupDefs> animGroupAngleTurns_{};
    std::array<fx, kMaxAnimGroupDefs> animGroupScale_{};

#ifdef GV3D_TESTING
    mutable StaticVector<CollisionDebugPrimitive, kCollisionDebugCap> collisionDebugPrimitives_{};
#endif
};

} // namespace gv