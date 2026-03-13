#pragma once

#include "platform/IPlatform.hpp"
#include "game/Game.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlay.hpp"
#include "IAppState.hpp"
#include "TitleState.hpp"
#include "LevelSelectState.hpp"
#include "PlayingState.hpp"
#include "StatusOverlayView.hpp"
#include <cstddef>
#include <cstdint>

namespace gv {

class App {
public:
    struct LevelEntry {
        const char* name;
        const char* path;
    };

public:
    int run(IPlatform& platform);

    IPlatform& platform() { return *plat_; }
    const IPlatform& platform() const { return *plat_; }

    Game& game() { return game_; }
    const Game& game() const { return game_; }

    StatusOverlay& statusOverlay() { return statusOverlay_; }
    const StatusOverlay& statusOverlay() const { return statusOverlay_; }

    int displayWidth() const { return w_; }
    int displayHeight() const { return h_; }

    std::size_t levelCount() const { return kLevelCount; }
    const char* levelName(std::size_t i) const;
    const char* levelPath(std::size_t i) const;

    std::size_t selectedLevel() const { return selectedLevel_; }
    void setSelectedLevel(std::size_t i) {
        if (i < kLevelCount) {
            selectedLevel_ = i;
        }
    }

    void changeState(IAppState& next);
    void showTitle();
    void showLevelSelect();
    void showPlaying();

    bool startLevel(std::size_t levelIndex);

private:
    void init(IPlatform& platform);
    InputState pollInput() const;

private:
    static constexpr LevelEntry kLevels[] = {
        { "Level 1", "levels/L01.BIN" },
        { "Level 2", "levels/L02.BIN" },
        { "Level 3", "levels/L03.BIN" },
        { "Level 4", "levels/L04.BIN" },
        { "Level 5", "levels/L05.BIN" }
    };
    static constexpr std::size_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

private:
    IPlatform* plat_ = nullptr;

    Game game_{};
    RenderList frame_{};
    StatusOverlay statusOverlay_{};

    LevelSelectState levelSelectState_{};
    PlayingState playingState_{};
    
    IAppState* currentState_ = nullptr;
    
    int w_ = 0;
    int h_ = 0;
    std::size_t selectedLevel_ = 0;
    
    TitleState titleState_{};
};

} // namespace gv