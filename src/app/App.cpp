#include "App.hpp"
#include "platform/Keys.hpp"

namespace gv {

namespace {

static constexpr uint32_t kFrameUs = 33333; // ~30 FPS

} // namespace

const char* App::levelName(std::size_t i) const {
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

    game_.reset();
    game_.setFileSystem(&plat_->fs());

    const LevelEntry& e = kLevels[levelIndex];
    if (!game_.loadLevel(e.path)) {
        statusOverlay_.addError("Failed to load level");
        statusOverlay_.addInfo(e.path);
        return false;
    }

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

    showTitle();
}

} // namespace gv