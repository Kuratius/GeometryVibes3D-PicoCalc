#include "App.hpp"
#include "platform/Keys.hpp"
#include <cstdio>

namespace gv {

namespace {

static constexpr uint32_t kFrameUs = 33333; // ~30 FPS
static constexpr uint32_t kBatteryPollUs = 2000000; // 2 seconds

struct BatteryFooterCache {
    uint32_t accumUs = kBatteryPollUs; // force immediate first update
    uint8_t level = 0;
    bool charging = false;
    bool valid = false;
};

void updateBatteryFooter(StatusOverlay& overlay,
                         const IPlatform& platform,
                         BatteryFooterCache& cache,
                         uint32_t dtUs) {
    cache.accumUs += dtUs;
    if (cache.valid && cache.accumUs < kBatteryPollUs) {
        return;
    }
    cache.accumUs = 0;

    const uint8_t level = platform.batteryLevelPercent();
    const bool charging = platform.batteryCharging();

    if (cache.valid && cache.level == level && cache.charging == charging) {
        return;
    }

    cache.level = level;
    cache.charging = charging;
    cache.valid = true;

    uint16_t color = 0x07E0; // green
    if (level < 10) {
        color = 0xF800; // red
    } else if (level < 30) {
        color = 0xFFE0; // yellow
    }

    char buf[32];
    if (charging) {
        std::snprintf(buf, sizeof(buf), "Batt Charging: %u%%", unsigned(level));
    } else {
        std::snprintf(buf, sizeof(buf), "Batt: %u%%", unsigned(level));
    }

    overlay.setFooterRight(buf, color);
}

} // namespace

const SaveData::SaveEntry* App::activeSave() const {
    return hasActiveSave() ? saveData_.entry(activeSaveIndex_) : nullptr;
}

bool App::loadSaves() {
    if (!plat_) return false;

    if (!saveData_.load(plat_->fs())) {
        return false;
    }

    if (saveData_.hasLastPlayed()) {
        activeSaveIndex_ = saveData_.lastPlayedIndex();
        syncRuntimeProgressFromActiveSave();
    } else {
        activeSaveIndex_ = SaveData::kNoSelection;
        unlockedLevelCount_ = 1;
        selectedLevel_ = 0;
    }

    return true;
}

bool App::saveSaves() const {
    if (!plat_) return false;
    return saveData_.save(plat_->fs());
}

bool App::createNewSave(const char* name) {
    uint8_t index = SaveData::kNoSelection;
    if (!saveData_.createEntry(name, index)) {
        return false;
    }

    activeSaveIndex_ = index;
    saveData_.setLastPlayedIndex(index);
    syncRuntimeProgressFromActiveSave();
    return saveSaves();
}

bool App::selectSave(std::size_t index) {
    if (index >= saveData_.entryCount()) {
        return false;
    }

    activeSaveIndex_ = static_cast<uint8_t>(index);
    if (!saveData_.setLastPlayedIndex(activeSaveIndex_)) {
        return false;
    }

    syncRuntimeProgressFromActiveSave();
    return saveSaves();
}

bool App::deleteSave(std::size_t index) {
    if (index >= saveData_.entryCount()) {
        return false;
    }

    if (!saveData_.deleteEntry(index)) {
        return false;
    }

    if (saveData_.hasLastPlayed()) {
        activeSaveIndex_ = saveData_.lastPlayedIndex();
        syncRuntimeProgressFromActiveSave();
    } else {
        activeSaveIndex_ = SaveData::kNoSelection;
        unlockedLevelCount_ = 1;
        selectedLevel_ = 0;
    }

    return saveSaves();
}

bool App::canContinue() const {
    return hasAnySave() && saveData_.hasLastPlayed();
}

bool App::continueGame() {
    if (!canContinue()) {
        return false;
    }

    const uint8_t idx = saveData_.lastPlayedIndex();
    if (idx >= saveData_.entryCount()) {
        return false;
    }

    activeSaveIndex_ = idx;
    syncRuntimeProgressFromActiveSave();

    const std::size_t frontier =
        (unlockedLevelCount_ > 0) ? (unlockedLevelCount_ - 1) : 0;

    return startLevel(frontier);
}

void App::syncRuntimeProgressFromActiveSave() {
    if (!hasActiveSave()) {
        unlockedLevelCount_ = 1;
        selectedLevel_ = 0;
        return;
    }

    const SaveData::SaveEntry* e = saveData_.entry(activeSaveIndex_);
    if (!e) {
        unlockedLevelCount_ = 1;
        selectedLevel_ = 0;
        return;
    }

#ifdef GV3D_TESTING
    unlockedLevelCount_ = kLevelCount;
#else
    std::size_t unlocked = e->unlockedCount;
    if (unlocked < 1) unlocked = 1;
    if (unlocked > kLevelCount) unlocked = kLevelCount;
    unlockedLevelCount_ = unlocked;
#endif

    selectedLevel_ = (unlockedLevelCount_ > 0) ? (unlockedLevelCount_ - 1) : 0;
    if (selectedLevel_ >= kLevelCount) {
        selectedLevel_ = kLevelCount - 1;
    }
}

bool App::saveCurrentProgress() {
    if (!hasActiveSave()) return false;
    if (selectedLevel_ >= kLevelCount) return false;

    SaveData::SaveEntry* e = saveData_.entry(activeSaveIndex_);
    if (!e) return false;

    const uint8_t progress = static_cast<uint8_t>(game_.progressPercent());
    if (progress > e->percentComplete[selectedLevel_]) {
        e->percentComplete[selectedLevel_] = progress;
    }

    return saveSaves();
}

bool App::saveLevelCompletion(std::size_t levelIndex) {
    if (!hasActiveSave()) return false;
    if (levelIndex >= kLevelCount) return false;

    SaveData::SaveEntry* e = saveData_.entry(activeSaveIndex_);
    if (!e) return false;

    e->percentComplete[levelIndex] = 100;

#ifndef GV3D_TESTING
    const std::size_t frontier = (e->unlockedCount > 0) ? (e->unlockedCount - 1) : 0;
    if (levelIndex == frontier && e->unlockedCount < kLevelCount) {
        ++e->unlockedCount;
    }
#endif

    syncRuntimeProgressFromActiveSave();
    return saveSaves();
}

uint8_t App::collectedStarsForLevel(std::size_t levelIndex) const {
    if (!hasActiveSave()) return 0;
    if (levelIndex >= kLevelCount) return 0;

    const SaveData::SaveEntry* e = saveData_.entry(activeSaveIndex_);
    if (!e) return 0;

    return uint8_t(e->stars[levelIndex] & 0x07u);
}

bool App::collectStar(std::size_t levelIndex, uint8_t starIndex) {
    if (!hasActiveSave()) return false;
    if (levelIndex >= kLevelCount) return false;
    if (starIndex >= 3) return false;

    SaveData::SaveEntry* e = saveData_.entry(activeSaveIndex_);
    if (!e) return false;

    const uint8_t bit = uint8_t(1u << starIndex);
    const uint8_t oldMask = uint8_t(e->stars[levelIndex] & 0x07u);
    const uint8_t newMask = uint8_t(oldMask | bit);

    if (newMask == oldMask) {
        return true;
    }

    e->stars[levelIndex] = newMask;
    return saveSaves();
}

std::size_t App::unlockedLevelCount() const {
#ifdef GV3D_TESTING
    return kLevelCount;
#else
    return unlockedLevelCount_;
#endif
}

bool App::isLevelUnlocked(std::size_t i) const {
#ifdef GV3D_TESTING
    return i < kLevelCount;
#else
    return i < unlockedLevelCount_;
#endif
}

const char *App::levelName(std::size_t i) const
{
    if (i >= kLevelCount) return "";
    return kLevels[i].name;
}

const char* App::levelPath(std::size_t i) const {
    if (i >= kLevelCount) return "";
    return kLevels[i].path;
}

int App::levelPercentComplete(std::size_t levelIndex) const {
    if (!hasActiveSave()) return 0;
    if (levelIndex >= kLevelCount) return 0;

    const SaveData::SaveEntry* e = saveData_.entry(activeSaveIndex_);
    if (!e) return 0;

    return int(e->percentComplete[levelIndex]);
}

void App::changeState(IAppState& next) {
    if (currentState_ == &next) {
        return;
    }

    if (currentState_) {
        currentState_->onExit(*this);
    }

    currentState_ = &next;
    currentState_->onEnter(*this);
}

void App::showTitle() {
    changeState(titleState_);
}

void App::showHomeMenu() {
    changeState(homeMenuState_);
}

void App::showNewGame() {
    changeState(newGameState_);
}

void App::showSavedGames() {
    changeState(savedGamesState_);
}

void App::showOptions() {
    changeState(optionsState_);
}

void App::showLevelSelect() {
    changeState(levelSelectState_);
}

void App::showPlaying() {
    changeState(playingState_);
}

bool App::startLevel(std::size_t levelIndex) {
    if (!plat_) return false;
    if (levelIndex >= kLevelCount) return false;
    if (!isLevelUnlocked(levelIndex)) {
        statusOverlay_.addWarning("Level locked");
        return false;
    }

    game_.reset();
    game_.setFileSystem(&plat_->fs());
    game_.setLevelIndex(levelIndex);
    game_.setCollectedStarsMask(collectedStarsForLevel(levelIndex));

    const LevelEntry& e = kLevels[levelIndex];
    if (!game_.loadLevel(e.path)) {
        statusOverlay_.addError("Failed to load level");
        statusOverlay_.addInfo(e.path);
        return false;
    }

    selectedLevel_ = levelIndex;
    showPlaying();
    return true;
}

InputState App::pollInput() const {
    const IInput& kb = plat_->input();

    InputState in{};
    in.thrust        = kb.down(KEY_SPACE);
    in.thrustPressed = kb.pressed(KEY_SPACE);

    in.upPressed    = kb.pressed(KEY_UP);
    in.downPressed  = kb.pressed(KEY_DOWN);
    in.leftPressed  = kb.pressed(KEY_LEFT);
    in.rightPressed = kb.pressed(KEY_RIGHT);

    in.confirm = kb.pressed(KEY_ENTER) || kb.pressed(KEY_RETURN);
    in.back    = kb.pressed(KEY_ESC)   || kb.pressed(KEY_BACKSPACE);
    in.pausePressed = kb.pressed(KEY_ESC) || kb.pressed(KEY_POWER);
    in.overlayTogglePressed = kb.pressed(KEY_F1);

    return in;
}

int App::run(IPlatform& platform) {
    plat_ = &platform;
    init(*plat_);

    uint32_t accumUs = 0;
    BatteryFooterCache batteryCache{};

    while (true) {
        uint32_t dtUs = plat_->dtUs();
        if (dtUs > 250000) dtUs = 250000;
        accumUs += dtUs;

        if (accumUs < kFrameUs) {
            continue;
        }

        plat_->input().update();
        InputState in = pollInput();

        accumUs -= kFrameUs;

        if (currentState_) {
            currentState_->update(*this, in, kFrameUs);
            if (currentState_) {
                updateBatteryFooter(statusOverlay_, *plat_, batteryCache, kFrameUs);
                currentState_->render(*this, plat_->display(), frame_);
            }
        }
    }

    return 0;
}

void App::init(IPlatform& platform) {
    plat_->init();
    (void)plat_->fs().init();

    w_ = plat_->display().width();
    h_ = plat_->display().height();

    game_.reset();
    game_.setFileSystem(&platform.fs());

    statusOverlay_.clear();
    selectedLevel_ = 0;
    unlockedLevelCount_ = 1;
    activeSaveIndex_ = SaveData::kNoSelection;

    if (!loadSaves()) {
        statusOverlay_.addWarning("Could not load saves");
    }

    showTitle();
}

} // namespace gv