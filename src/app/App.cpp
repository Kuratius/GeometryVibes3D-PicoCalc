#include "App.hpp"
#include "platform/Keys.hpp"

namespace gv {

int App::run(IPlatform& platform) {
    plat = &platform;

    init(*plat);

    while (true) {
        const uint32_t dt = plat->dtUs();

        // InputState mapping lives at the app layer now.
        plat->input().update();
        const IInput& kb = plat->input();

        InputState in{};
        in.thrust        = kb.down(KEY_SPACE);
        in.thrustPressed = kb.pressed(KEY_SPACE);

        in.up    = kb.down(KEY_UP);
        in.down  = kb.down(KEY_DOWN);
        in.left  = kb.down(KEY_LEFT);
        in.right = kb.down(KEY_RIGHT);

        in.confirm = kb.pressed(KEY_ENTER) || kb.pressed(KEY_RETURN);
        in.back    = kb.pressed(KEY_ESC)   || kb.pressed(KEY_BACKSPACE);
        in.pausePressed = kb.pressed(KEY_ESC) || kb.pressed(KEY_F1) || kb.pressed(KEY_POWER);

        plat->display().beginFrame();
        tick(in, dt);
        plat->display().draw(renderList());
        plat->display().endFrame();
    }

    return 0;
}

void App::init(IPlatform& platform) {
    plat->init();
    (void)plat->fs().init();
    
    w = plat->display().width(); h = plat->display().height();

    game.reset();

    // Game uses IFileSystem for level streaming.
    game.setFileSystem(&platform.fs());

    // Load first level (I ignore failure for now; later I can add an on-screen indicator).
    (void)game.loadLevel("levels/L02.BIN");

    Camera cam{};
    cam.focal = fx::fromInt(180);
    cam.cx = fx::fromInt(w / 2);
    cam.cy = fx::fromInt(h / 2);

    cam.pos    = Vec3fx{ fx::fromInt(-20), fx::fromInt(20), fx::fromInt(120) };
    cam.target = Vec3fx{ fx::fromInt(40),  fx::fromInt(0),  fx::fromInt(0) };
    cam.up     = Vec3fx{ fx::fromInt(0),   fx::fromInt(1),  fx::fromInt(0) };

    renderer.setCamera(cam);
}

void App::tick(const InputState& in, uint32_t dtUs) {
    const fx dt = fx::fromMicros(dtUs);
    game.update(in, dt);

    Camera cam = renderer.camera();

    // I follow the ship vertically by shifting both pos.y and target.y to keep pitch stable.
    const fx follow = fx::fromRatio(3, 20); // 0.15
    const fx yOff = game.ship().y * follow;

    cam.pos.y    = fx::fromInt(22) + yOff;
    cam.target.y = fx::fromInt(0)  + yOff;

    cam.pos.x    = fx::fromInt(-20);
    cam.target.x = fx::fromInt(40);

    cam.pos.z    = fx::fromInt(120);
    cam.target.z = fx::fromInt(0);

    renderer.setCamera(cam);

    rl.clear();
    renderer.buildScene(rl, game, game.scrollX());
}

} // namespace gv