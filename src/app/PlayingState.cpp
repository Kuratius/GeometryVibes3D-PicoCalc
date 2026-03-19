#include "PlayingState.hpp"
#include "App.hpp"
#include "core/Config.hpp"
#include "game/Game.hpp"
#include "platform/IPlatform.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlayView.hpp"

namespace gv {

void PlayingState::setupDefaultCamera(App& app) {
    cam_ = Camera{};
    cam_.focal = fx::fromInt(180);
    cam_.cx = fx::fromInt(app.displayWidth() / 2);
    cam_.cy = fx::fromInt(app.displayHeight() / 2);

    cam_.pos    = Vec3fx{ fx::fromInt(-20), fx::fromInt(20), fx::fromInt(120) };
    cam_.target = Vec3fx{ fx::fromInt(40),  fx::fromInt(0),  fx::fromInt(0) };
    cam_.up     = Vec3fx{ fx::fromInt(0),   fx::fromInt(1),  fx::fromInt(0) };

    buildCameraBasis(cam_);
}

void PlayingState::resetPresentationState() {
    trail_ = TrailState{};
    explosion_ = ExplosionState{};
    portalRays_ = PortalRayState{};
}

void PlayingState::onEnter(App& app) {
    resetPresentationState();
    setupDefaultCamera(app);
}

void PlayingState::onExit(App& app) {
    (void)app;
}

void PlayingState::returnToLevelSelect(App& app) {
    app.game().reset();
    resetPresentationState();
    app.showLevelSelect();
}

void PlayingState::update(App& app, const InputState& in, uint32_t dtUs) {
    Game& game = app.game();

    if (in.overlayTogglePressed) {
        app.statusOverlay().toggleVisible();
    }

    if (in.pausePressed && !game.paused()) {
        game.pause();
        return;
    }

    if (game.paused()) {
        if (in.thrustPressed) {
            game.resume();
        }
        return;
    }

    const RunState prevState = game.state();

    const fx dt = fx::fromMicros(dtUs);
    game.update(in, dt);

    if (prevState != RunState::Dead && game.state() == RunState::Dead) {
        app.saveCurrentProgress();

        const uint64_t t = app.platform().nowUs();
        const uint32_t seed = uint32_t(t) ^ uint32_t(t >> 32);
        sceneBuilder_.startShipExplosion(explosion_, game, seed);
    }

    sceneBuilder_.updateExplosion(explosion_, dt);
    sceneBuilder_.updatePortalRays(portalRays_, dt);

    if (game.state() == RunState::Dead && !explosion_.active) {
        if (in.thrustPressed) {
            app.saveCurrentProgress();
            returnToLevelSelect(app);
            return;
        }
    }

    if (game.finishedScroll()) {
        app.saveLevelCompletion(app.selectedLevel());
        returnToLevelSelect(app);
        return;
    }

    if (in.back) {
        app.saveCurrentProgress();
        returnToLevelSelect(app);
        return;
    }

    const fx follow = fx::fromRatio(3, 20);
    const fx yOff = game.ship().y * follow;

    cam_.pos.y    = fx::fromInt(22) + yOff;
    cam_.target.y = fx::fromInt(0)  + yOff;

    cam_.pos.x    = fx::fromInt(-20);
    cam_.target.x = fx::fromInt(40);

    cam_.pos.z    = fx::fromInt(120);
    cam_.target.z = fx::fromInt(0);

    buildCameraBasis(cam_);
}

void PlayingState::render(App& app, IDisplay& display, RenderList& rl) {
    rl.clear();
    sceneBuilder_.buildScene(rl, cam_, app.game(), app.game().scrollX(), trail_, explosion_, portalRays_);

     if (app.game().collisionHighlightEnabled()) {
        sceneBuilder_.buildCollisionDebug(rl, cam_, app.game());
    }

    sceneBuilder_.buildHud(rl, app.game(), app.displayWidth(), app.displayHeight());
    StatusOverlayView::appendTo(
        rl,
        app.statusOverlay(),
        app.displayWidth(),
        app.displayHeight()
    );

    display.beginFrame();
    display.draw(rl);
    display.endFrame();
}

} // namespace gv