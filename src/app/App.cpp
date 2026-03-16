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

void App::unlockNextLevel() {
#ifndef GV3D_TESTING
    if (unlockedLevelCount_ < kLevelCount) {
        ++unlockedLevelCount_;
    }
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

    showTitle();
}

} // namespace gv