#pragma once

#include "platform/IPlatform.hpp"
#include "game/Game.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlay.hpp"
#include "IAppState.hpp"
#include "SaveData.hpp"

#include "TitleState.hpp"
#include "HomeMenuState.hpp"
#include "NewGameState.hpp"
#include "SavedGamesState.hpp"
#include "OptionsState.hpp"
#include "LevelSelectState.hpp"
#include "PlayingState.hpp"

#include <cstddef>
#include <cstdint>

//#define GV3D_TESTING  // Un-comment to unlock all levels

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

    SaveData& saveData() { return saveData_; }
    const SaveData& saveData() const { return saveData_; }

    int displayWidth() const { return w_; }
    int displayHeight() const { return h_; }

    std::size_t levelCount() const { return kLevelCount; }
    std::size_t unlockedLevelCount() const;
    bool isLevelUnlocked(std::size_t i) const;
    const char* levelName(std::size_t i) const;
    const char* levelPath(std::size_t i) const;
    int levelPercentComplete(std::size_t levelIndex) const;

    std::size_t selectedLevel() const { return selectedLevel_; }
    void setSelectedLevel(std::size_t i) {
        if (i < kLevelCount) {
            selectedLevel_ = i;
        }
    }

    bool hasAnySave() const { return saveData_.entryCount() > 0; }
    bool hasMultipleSaves() const { return saveData_.entryCount() > 1; }
    bool hasActiveSave() const {
        return activeSaveIndex_ != SaveData::kNoSelection &&
               activeSaveIndex_ < saveData_.entryCount();
    }

    uint8_t activeSaveIndex() const { return activeSaveIndex_; }

    bool loadSaves();
    bool saveSaves() const;

    bool createNewSave(const char* name);
    bool selectSave(std::size_t index);
    bool deleteSave(std::size_t index);

    bool canContinue() const;
    bool continueGame();

    void syncRuntimeProgressFromActiveSave();
    bool saveCurrentProgress();
    bool saveLevelCompletion(std::size_t levelIndex);

    uint8_t collectedStarsForLevel(std::size_t levelIndex) const;
    bool collectStar(std::size_t levelIndex, uint8_t starIndex);

    void changeState(IAppState& next);
    void showTitle();
    void showHomeMenu();
    void showNewGame();
    void showSavedGames();
    void showOptions();
    void showLevelSelect();
    void showPlaying();

    bool startLevel(std::size_t levelIndex);

private:
    void init(IPlatform& platform);
    InputState pollInput() const;
    void clearActiveSaveState();

private:
    static constexpr LevelEntry kLevels[] = {
        { "Level 1 ", "levels/L01.BIN" },
        { "Level 2 ", "levels/L02.BIN" },
        { "Level 3 ", "levels/L03.BIN" },
        { "Level 4 ", "levels/L04.BIN" },
        { "Level 5 ", "levels/L05.BIN" },
        { "Level 6 ", "levels/L06.BIN" },
        { "Level 7 ", "levels/L07.BIN" },
        { "Level 8 ", "levels/L08.BIN" },
        { "Level 9 ", "levels/L09.BIN" },
        { "Level 10", "levels/L10.BIN" }
    };
    static constexpr std::size_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

private:
    // Platform / frame state
    IPlatform* plat_ = nullptr;
    int w_ = 0;
    int h_ = 0;
    
    // Core app data
    Game game_{};
    StatusOverlay statusOverlay_{};
    SaveData saveData_{};
    
    // Runtime save/progression state
    std::size_t selectedLevel_ = 0;
    std::size_t unlockedLevelCount_ = 1;
    uint8_t activeSaveIndex_ = SaveData::kNoSelection;
    
    // Per-frame render scratch
    RenderList frame_{};

    // App state machine
    TitleState titleState_{};
    HomeMenuState homeMenuState_{};
    NewGameState newGameState_{};
    SavedGamesState savedGamesState_{};
    OptionsState optionsState_{};
    LevelSelectState levelSelectState_{};
    PlayingState playingState_{};
    IAppState* currentState_ = nullptr;
};

} // namespace gv