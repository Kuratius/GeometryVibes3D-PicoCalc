#pragma once
#include <cstdint>
#include "render/Fixed.hpp"
#include "game/Level.hpp"
#include "game/Input.hpp"
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

class Game {
public:
    void reset();

    void setFileSystem(IFileSystem* fs) { fs_ = fs; }

    bool loadLevel(const char* path);
    void unloadLevel();
    bool hasLevel() const { return file_ != nullptr; }
    const LevelHeaderV1& levelHeader() const { return levelHdr; }

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

    int progressPercent() const;

    Hud& hud() { return hud_; }
    const Hud& hud() const { return hud_; }

private:
    bool checkPortalReached(fx shipY) const;
    bool checkCollisionAt(fx shipY) const;
    static bool collideCell(ShapeId sid, ModId mid, fx lx, fx ly, fx lz, fx r);

    ShipState shipState{};
    RunState runState = RunState::WaitingToStart;
    RunState resumeState = RunState::WaitingToStart;
    fx xScroll{};
    fx flyOutX{};
    bool finished_ = false;
    bool hit = false;

    IFileSystem* fs_ = nullptr;
    IFile* file_ = nullptr;
    LevelHeaderV1 levelHdr{};
    Hud hud_{};
};

} // namespace gv