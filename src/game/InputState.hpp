#pragma once

namespace gv {

struct InputState {
    // Continuous
    bool thrust = false;

    // Pressed this frame
    bool thrustPressed = false;
    bool upPressed = false;
    bool downPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;

    bool confirm = false;
    bool back = false;
    bool pausePressed = false;
    bool overlayTogglePressed = false;

    // UI editing
    bool backspacePressed = false;
    bool deletePressed = false;
    char typedChar = '\0';
};

} // namespace gv