#include "App.hpp"
#include "platform/Keys.hpp"

namespace gv {

namespace {

static constexpr uint32_t kFrameUs = 33333; // ~30 FPS

} // namespace

int App::run(IPlatform& platform) {
    plat = &platform;
    init(*plat);

    uint32_t accumUs = 0;

    while (true) {
        uint32_t dtUs = plat->dtUs();
        if (dtUs > 250000) dtUs = 250000;
        accumUs += dtUs;

        if (accumUs < kFrameUs) {
            continue;
        }

        plat->input().update();
        const IInput& kb = plat->input();

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

        accumUs -= kFrameUs;

        tick(in, kFrameUs);

        if (appState_ == AppState::TitleScreen) {
            titleScreen_.draw(plat->display());
        } else {
            plat->display().beginFrame();
            plat->display().draw(renderList());
            plat->display().endFrame();
        }
    }

    return 0;
}

void App::init(IPlatform& platform) {
    plat->init();
    (void)plat->fs().init();

    w = plat->display().width();
    h = plat->display().height();

    game.reset();
    game.setFileSystem(&platform.fs());

    for (std::size_t i = 0; i < kLevelCount; ++i) {
        levelTexts_[i].setText(kLevels[i].name);
    }

    Camera cam{};
    cam.focal = fx::fromInt(180);
    cam.cx = fx::fromInt(w / 2);
    cam.cy = fx::fromInt(h / 2);

    cam.pos    = Vec3fx{ fx::fromInt(-20), fx::fromInt(20), fx::fromInt(120) };
    cam.target = Vec3fx{ fx::fromInt(40),  fx::fromInt(0),  fx::fromInt(0) };
    cam.up     = Vec3fx{ fx::fromInt(0),   fx::fromInt(1),  fx::fromInt(0) };

    renderer.setCamera(cam);
    renderer.resetEffects();

    if (titleScreen_.load(platform.fs(), "assets/title.rgb565")) {
        appState_ = AppState::TitleScreen;
    } else {
        statusOverlay_.addError("Error: Could not load title.rgb565.");
        statusOverlay_.addInfo("Is the SD card inserted?");
        appState_ = AppState::LevelSelect;
    }

    selectedLevel_ = 0;
}

bool App::loadSelectedLevel() {
    if (!plat) return false;
    if (selectedLevel_ >= kLevelCount) return false;

    game.reset();
    game.setFileSystem(&plat->fs());
    renderer.resetEffects();

    const LevelEntry& e = kLevels[selectedLevel_];
    if (!game.loadLevel(e.path)) {
        statusOverlay_.addError("Failed to load level");
        statusOverlay_.addInfo(e.path);
        return false;
    }

    appState_ = AppState::Playing;
    return true;
}

void App::buildLevelSelect(RenderList& rl) const {
    static constexpr uint16_t kWhite = 0xFFFF;

    const int titleX = (w - menuTitle_.width()) / 2;
    rl.addText(&menuTitle_, (int16_t)titleX, 12, kWhite);

    const int startY = 40;
    const int lineStep = 12;
    const int itemX = 16;

    for (std::size_t i = 0; i < kLevelCount; ++i) {
        const bool selected = (i == selectedLevel_);
        rl.addText(&levelTexts_[i],(int16_t)itemX, (int16_t)(startY + int(i) * lineStep), kWhite, 255, selected);
    }
}

void App::tick(const InputState& in, uint32_t dtUs) {
    auto rebuildLevelSelect = [&]() {
        rl.clear();
        buildLevelSelect(rl);
        renderer.buildStatusOverlay(rl, statusOverlay_);
    };

    auto rebuildGameplay = [&]() {
        rl.clear();
        renderer.buildScene(rl, game, game.scrollX());
        renderer.buildHud(rl, game);
        renderer.buildStatusOverlay(rl, statusOverlay_);
    };

    auto returnToLevelSelect = [&]() {
        appState_ = AppState::LevelSelect;
        game.reset();
        renderer.resetEffects();
        rebuildLevelSelect();
    };

    if (appState_ == AppState::TitleScreen) {
        titleScreen_.update(in);
        if (titleScreen_.accepted()) {
            titleScreen_.unload();
            appState_ = AppState::LevelSelect;
            rebuildLevelSelect();
        }
        return;
    }

    if (in.overlayTogglePressed) {
        statusOverlay_.toggleVisible();
    }

    if (appState_ == AppState::LevelSelect) {
        if (in.upPressed && selectedLevel_ > 0) {
            --selectedLevel_;
        } else if (in.downPressed && (selectedLevel_ + 1) < kLevelCount) {
            ++selectedLevel_;
        }

        if (in.confirm || in.thrustPressed) {
            (void)loadSelectedLevel();
        }

        rebuildLevelSelect();
        return;
    }

    if (in.pausePressed && !game.paused()) {
        game.pause();
        rebuildGameplay();
        return;
    }

    if (game.paused()) {
        if (in.thrustPressed) {
            game.resume();
        }

        rebuildGameplay();
        return;
    }

    const RunState prevState = game.state();

    const fx dt = fx::fromMicros(dtUs);
    game.update(in, dt);

    if (prevState != RunState::Dead && game.state() == RunState::Dead) {
        const uint64_t t = plat->nowUs();
        const uint32_t seed = uint32_t(t) ^ uint32_t(t >> 32);
        renderer.startShipExplosion(game, seed);
    }

    renderer.update(dt);

    if (game.state() == RunState::Dead && !renderer.explosionActive()) {
        returnToLevelSelect();
        return;
    }

    if (game.finishedScroll()) {
        returnToLevelSelect();
        return;
    }

    if (in.back) {
        returnToLevelSelect();
        return;
    }

    Camera cam = renderer.camera();

    const fx follow = fx::fromRatio(3, 20); // 0.15
    const fx yOff = game.ship().y * follow;

    cam.pos.y    = fx::fromInt(22) + yOff;
    cam.target.y = fx::fromInt(0)  + yOff;

    cam.pos.x    = fx::fromInt(-20);
    cam.target.x = fx::fromInt(40);

    cam.pos.z    = fx::fromInt(120);
    cam.target.z = fx::fromInt(0);

    renderer.setCamera(cam);

    rebuildGameplay();
}

} // namespace gv