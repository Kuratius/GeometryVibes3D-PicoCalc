#pragma once

#include "platform/IPlatform.hpp"
#include "game/Game.hpp"
#include "render/Renderer.hpp"
#include "render/RenderList.hpp"
#include "render/Text.hpp"
#include "TitleScreen.hpp"
#include "StatusOverlay.hpp"
#include <cstddef>

namespace gv {

class App {
public:
    int run(IPlatform& platform);

private:
    enum class AppState : uint8_t {
        TitleScreen,
        LevelSelect,
        Playing
    };

    struct LevelEntry {
        const char* name;
        const char* path;
    };

private:
    void init(IPlatform& platform);
    void tick(const InputState& in, uint32_t dtUs);

    void buildLevelSelect(RenderList& rl) const;
    bool loadSelectedLevel();

    const RenderList& renderList() const { return rl; }

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
    IPlatform* plat = nullptr;
    Game game;
    Renderer renderer;
    RenderList rl;
    
    AppState appState_ = AppState::TitleScreen;
    std::size_t selectedLevel_ = 0;
    
    Text menuTitle_{ "SELECT LEVEL" };
    Text levelTexts_[kLevelCount]{};
    
    int w{}, h{};
    StatusOverlay statusOverlay_;
    TitleScreen titleScreen_;
};

} // namespace gv